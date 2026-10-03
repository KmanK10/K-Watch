import CoreBluetooth
import Foundation
import os

/// Finds the watch and keeps one encrypted connection open.
///
/// The watch is already bonded for notifications, so it usually will not appear in a scan.
/// This asks iOS for peripherals already connected with the companion service, and only scans
/// for that service UUID if none are connected. Core Bluetooth state restoration lets a
/// `weather.request` notification wake the app in the background.
@MainActor
final class WatchConnection: NSObject, ObservableObject {
    static let restoreIdentifier = "com.kiefermenard.kwatch.central"

    enum Link: Equatable {
        case bluetoothOff
        case unauthorized
        case searching
        case connected
    }

    @Published private(set) var link: Link = .searching
    @Published private(set) var isReady = false

    var onLink: (@MainActor (Link) -> Void)?
    var onReady: (@MainActor () -> Void)?
    var onEvent: (@MainActor (IncomingMessage) -> Void)?
    var onLog: (@MainActor (LogLine.Kind, String) -> Void)?

    private let log = Logger(subsystem: "com.kiefermenard.kwatch", category: "ble")
    private var central: CBCentralManager?
    private var peripheral: CBPeripheral?
    private var rx: CBCharacteristic?
    private var tx: CBCharacteristic?
    private var framer = ChunkFramer()
    private var writeQueue: [Data] = []
    private var writeInFlight = false
    private var writeAttempts = 0
    private var generation = 0
    private var writeGeneration = 0
    private var waiters: [Int: CheckedContinuation<IncomingMessage, Error>] = [:]
    private var nextID = 1
    private var started = false

    private static let peripheralKey = "watch.peripheral.id"
    private static let connectOptions: [String: Any] = [
        CBConnectPeripheralOptionNotifyOnConnectionKey: true,
        CBConnectPeripheralOptionNotifyOnDisconnectionKey: true,
        CBConnectPeripheralOptionNotifyOnNotificationKey: true,
    ]

    func start() {
        guard !started else { return }
        started = true
        // nil queue: callbacks arrive on the main thread, which is this actor.
        central = CBCentralManager(
            delegate: self,
            queue: nil,
            options: [
                CBCentralManagerOptionRestoreIdentifierKey: Self.restoreIdentifier,
                CBCentralManagerOptionShowPowerAlertKey: true,
            ]
        )
    }

    func makeID() -> Int {
        defer { nextID += 1 }
        return nextID
    }

    func request(_ payload: Data, id: Int) async throws -> IncomingMessage {
        guard isReady else { throw CompanionError.notReady }
        return try await withCheckedThrowingContinuation { continuation in
            waiters[id] = continuation
            do {
                try enqueue(payload)
            } catch {
                waiters[id] = nil
                continuation.resume(throwing: error)
                return
            }
            Task { [weak self] in
                try? await Task.sleep(nanoseconds: 15_000_000_000)
                self?.expire(id)
            }
        }
    }

    private func expire(_ id: Int) {
        guard let waiter = waiters.removeValue(forKey: id) else { return }
        waiter.resume(throwing: CompanionError.timedOut)
    }

    private func enqueue(_ payload: Data) throws {
        guard payload.count <= ChunkFramer.maxMessageBytes else {
            throw CompanionError.messageTooLarge(payload.count)
        }
        guard let peripheral, rx != nil else { throw CompanionError.notReady }
        let reported = peripheral.maximumWriteValueLength(for: .withResponse)
        // The firmware copies one write into a 512-byte buffer, flags byte included.
        let maxWrite = min(max(reported, 20), 512)
        writeQueue.append(contentsOf: ChunkFramer.chunks(for: payload, maxWriteLength: maxWrite))
        if let text = String(data: payload, encoding: .utf8) {
            record(.out, text)
        }
        pump()
    }

    private func pump() {
        guard !writeInFlight, let peripheral, let rx, !writeQueue.isEmpty else { return }
        writeInFlight = true
        writeGeneration = generation
        peripheral.writeValue(writeQueue[0], for: rx, type: .withResponse)
    }

    private func writeFinished(error: String?) {
        guard writeInFlight, writeGeneration == generation else { return }
        if let error {
            writeAttempts += 1
            record(.info, "Write failed: \(error)")
            if writeAttempts <= 3 {
                writeInFlight = false
                pump()
                return
            }
            let hint = error.lowercased().contains("encrypt") || error.lowercased().contains("auth")
                ? " Pair the watch in Settings → Bluetooth so the link is encrypted."
                : ""
            failAll(.writeFailed(error + hint))
            return
        }
        writeAttempts = 0
        writeInFlight = false
        if !writeQueue.isEmpty {
            writeQueue.removeFirst()
        }
        pump()
    }

    private func failAll(_ error: CompanionError) {
        generation += 1
        writeInFlight = false
        writeAttempts = 0
        writeQueue.removeAll()
        let pending = waiters
        waiters.removeAll()
        for waiter in pending.values {
            waiter.resume(throwing: error)
        }
    }

    private func deliver(_ message: IncomingMessage) {
        if let id = message.replyID, let waiter = waiters.removeValue(forKey: id) {
            waiter.resume(returning: message)
            return
        }
        onEvent?(message)
    }

    private func setLink(_ link: Link) {
        guard self.link != link else { return }
        self.link = link
        onLink?(link)
    }

    private func record(_ kind: LogLine.Kind, _ text: String) {
        log.info("\(kind.rawValue, privacy: .public) \(text, privacy: .public)")
        onLog?(kind, text)
    }

    // MARK: - Finding the watch

    private func lookForWatch() {
        guard let central, central.state == .poweredOn else { return }
        let service = CBUUID(string: CompanionUUID.service)
        let connected = central.retrieveConnectedPeripherals(withServices: [service])
        if let peripheral = preferred(in: connected) {
            record(.info, "Using the watch already connected to this iPhone.")
            central.stopScan()
            connect(peripheral)
            return
        }

        if let saved = UserDefaults.standard.string(forKey: Self.peripheralKey),
           let uuid = UUID(uuidString: saved) {
            let known = central.retrievePeripherals(withIdentifiers: [uuid])
            if let peripheral = known.first {
                record(.info, "Waiting for the remembered watch. The connection stays open until it is back in range.")
                connect(peripheral)
            }
        }

        record(.info, "Scanning for the companion service.")
        central.scanForPeripherals(withServices: [service], options: nil)
        setLink(.searching)
    }

    private func preferred(in peripherals: [CBPeripheral]) -> CBPeripheral? {
        guard let saved = UserDefaults.standard.string(forKey: Self.peripheralKey) else {
            return peripherals.first
        }
        return peripherals.first { $0.identifier.uuidString == saved } ?? peripherals.first
    }

    private func connect(_ peripheral: CBPeripheral) {
        guard let central else { return }
        self.peripheral = peripheral
        peripheral.delegate = self
        UserDefaults.standard.set(peripheral.identifier.uuidString, forKey: Self.peripheralKey)
        switch peripheral.state {
        case .connected:
            discover(peripheral)
        case .connecting:
            break
        default:
            central.connect(peripheral, options: Self.connectOptions)
        }
        if !isReady {
            setLink(.searching)
        }
    }

    private func discover(_ peripheral: CBPeripheral) {
        peripheral.delegate = self
        peripheral.discoverServices([CBUUID(string: CompanionUUID.service)])
    }

    private func subscribe() {
        guard let peripheral, let tx else { return }
        peripheral.setNotifyValue(true, for: tx)
    }

    private func markReady() {
        guard !isReady else { return }
        isReady = true
        setLink(.connected)
        record(.info, "Subscribed to the watch.")
        onReady?()
    }

    private func dropLink(reason: String) {
        record(.info, reason)
        isReady = false
        rx = nil
        tx = nil
        framer.reset()
        failAll(.disconnected)
        setLink(.searching)
    }
}

extension WatchConnection: CBCentralManagerDelegate {
    // The manager is created with a nil queue, so these run on the main thread.
    nonisolated func centralManagerDidUpdateState(_ central: CBCentralManager) {
        MainActor.assumeIsolated { self.applyCentralState(central.state) }
    }

    nonisolated func centralManager(_ central: CBCentralManager, willRestoreState dict: [String: Any]) {
        let peripherals = dict[CBCentralManagerRestoredStatePeripheralsKey] as? [CBPeripheral] ?? []
        let scanning = dict[CBCentralManagerRestoredStateScanServicesKey] != nil
        MainActor.assumeIsolated { self.restore(peripherals: peripherals, wasScanning: scanning) }
    }

    nonisolated func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String: Any], rssi RSSI: NSNumber) {
        MainActor.assumeIsolated { self.discovered(peripheral) }
    }

    nonisolated func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        MainActor.assumeIsolated { self.didConnect(peripheral) }
    }

    nonisolated func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
        let message = error?.localizedDescription ?? "Could not connect."
        MainActor.assumeIsolated {
            self.record(.info, message)
            self.lookForWatch()
        }
    }

    nonisolated func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        let message = error?.localizedDescription ?? "Watch disconnected."
        MainActor.assumeIsolated { self.didDisconnect(peripheral, message: message) }
    }
}

extension WatchConnection: CBPeripheralDelegate {
    nonisolated func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        MainActor.assumeIsolated {
            if let message = error?.localizedDescription {
                self.record(.info, "Service discovery failed: \(message)")
            }
            guard let service = peripheral.services?.first(where: { $0.uuid == CBUUID(string: CompanionUUID.service) }) else {
                return
            }
            peripheral.discoverCharacteristics(
                [CBUUID(string: CompanionUUID.rx), CBUUID(string: CompanionUUID.tx)],
                for: service
            )
        }
    }

    nonisolated func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        MainActor.assumeIsolated {
            if let message = error?.localizedDescription {
                self.record(.info, "Characteristic discovery failed: \(message)")
                return
            }
            self.rx = service.characteristics?.first { $0.uuid == CBUUID(string: CompanionUUID.rx) }
            self.tx = service.characteristics?.first { $0.uuid == CBUUID(string: CompanionUUID.tx) }
            self.subscribe()
        }
    }

    nonisolated func peripheral(_ peripheral: CBPeripheral, didUpdateNotificationStateFor characteristic: CBCharacteristic, error: Error?) {
        let message = error?.localizedDescription
        let notifying = characteristic.isNotifying
        let isTX = characteristic.uuid == CBUUID(string: CompanionUUID.tx)
        MainActor.assumeIsolated {
            guard isTX else { return }
            if let message {
                self.record(.info, "Could not subscribe: \(message)")
                if message.lowercased().contains("encrypt") || message.lowercased().contains("auth") {
                    self.record(.info, "Waiting for the bonded link to encrypt, then trying again.")
                    Task { @MainActor in
                        try? await Task.sleep(nanoseconds: 1_000_000_000)
                        self.subscribe()
                    }
                }
                return
            }
            if notifying {
                self.markReady()
            }
        }
    }

    nonisolated func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        let value = characteristic.value
        let message = error?.localizedDescription
        MainActor.assumeIsolated { self.received(value, error: message) }
    }

    nonisolated func peripheral(_ peripheral: CBPeripheral, didWriteValueFor characteristic: CBCharacteristic, error: Error?) {
        let message = error?.localizedDescription
        MainActor.assumeIsolated { self.writeFinished(error: message) }
    }
}

extension WatchConnection {
    fileprivate func applyCentralState(_ state: CBManagerState) {
        switch state {
        case .poweredOn:
            record(.info, "Bluetooth is on.")
            if let peripheral, peripheral.state == .connected {
                discover(peripheral)
            } else {
                lookForWatch()
            }
        case .poweredOff:
            dropLink(reason: "Bluetooth is off.")
            setLink(.bluetoothOff)
        case .unauthorized:
            dropLink(reason: "Bluetooth access is off for K-Watch.")
            setLink(.unauthorized)
        case .unsupported:
            dropLink(reason: "This iPhone does not support Bluetooth Low Energy.")
            setLink(.bluetoothOff)
        default:
            setLink(.searching)
        }
    }

    fileprivate func restore(peripherals: [CBPeripheral], wasScanning: Bool) {
        record(.info, "Restoring the Bluetooth connection.")
        if let peripheral = preferred(in: peripherals) ?? peripherals.first {
            self.peripheral = peripheral
            peripheral.delegate = self
            if peripheral.state == .connected {
                discover(peripheral)
            } else if let central {
                central.connect(peripheral, options: Self.connectOptions)
            }
        } else if wasScanning {
            lookForWatch()
        }
    }

    fileprivate func discovered(_ peripheral: CBPeripheral) {
        record(.info, "Found the watch while scanning.")
        central?.stopScan()
        connect(peripheral)
    }

    fileprivate func didConnect(_ peripheral: CBPeripheral) {
        central?.stopScan()
        self.peripheral = peripheral
        peripheral.delegate = self
        UserDefaults.standard.set(peripheral.identifier.uuidString, forKey: Self.peripheralKey)
        record(.info, "Connected. Discovering the companion service.")
        discover(peripheral)
    }

    fileprivate func didDisconnect(_ peripheral: CBPeripheral, message: String) {
        guard self.peripheral?.identifier == peripheral.identifier else { return }
        dropLink(reason: message)
        // A pending connect never times out, so keep one open for when the watch returns.
        lookForWatch()
    }

    fileprivate func received(_ data: Data?, error: String?) {
        if let error {
            record(.info, "Notification error: \(error)")
            return
        }
        guard let data, let message = framer.append(data) else { return }
        let text = String(data: message, encoding: .utf8) ?? "<\(message.count) bytes>"
        record(.inn, text)
        do {
            deliver(try MessageCodec.decode(message))
        } catch {
            record(.info, "Could not read a message from the watch.")
        }
    }
}
