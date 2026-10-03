import CoreLocation
import Foundation
import MapKit

/// When-in-use location, plus significant-change updates while the app is running.
/// A free Apple ID can use When In Use. Background relaunch for location would also need
/// Always authorization, so a weather request while the app is suspended uses the last fix.
@MainActor
final class LocationProvider: NSObject, ObservableObject {
    @Published private(set) var coordinate: GeoPoint?
    @Published private(set) var placeName: String?
    @Published private(set) var access: LocationAccess = .notDetermined

    var onLocation: (@MainActor (GeoPoint) -> Void)?
    var onAccess: (@MainActor (LocationAccess) -> Void)?

    private let manager = CLLocationManager()
    private var lastDelivered: CLLocation?
    private static let latitudeKey = "location.lat"
    private static let longitudeKey = "location.lon"
    private static let placeKey = "location.place"

    override init() {
        super.init()
        if UserDefaults.standard.object(forKey: Self.latitudeKey) != nil {
            let point = GeoPoint(
                latitude: UserDefaults.standard.double(forKey: Self.latitudeKey),
                longitude: UserDefaults.standard.double(forKey: Self.longitudeKey)
            )
            coordinate = point
            placeName = UserDefaults.standard.string(forKey: Self.placeKey)
            lastDelivered = CLLocation(latitude: point.latitude, longitude: point.longitude)
        }
    }

    func start() {
        manager.delegate = self
        manager.desiredAccuracy = kCLLocationAccuracyKilometer
        manager.distanceFilter = 500
        apply(manager.authorizationStatus)
        if access == .notDetermined {
            manager.requestWhenInUseAuthorization()
        }
    }

    func requestOneShot() {
        guard access == .authorized else { return }
        manager.requestLocation()
    }

    private func apply(_ status: CLAuthorizationStatus) {
        switch status {
        case .authorizedAlways, .authorizedWhenInUse:
            setAccess(.authorized)
            manager.requestLocation()
            manager.startMonitoringSignificantLocationChanges()
        case .denied, .restricted:
            setAccess(.denied)
        case .notDetermined:
            setAccess(.notDetermined)
        @unknown default:
            setAccess(.notDetermined)
        }
    }

    private func setAccess(_ access: LocationAccess) {
        guard self.access != access else { return }
        self.access = access
        onAccess?(access)
    }

    private func handle(_ locations: [CLLocation]) {
        guard let location = locations.last else { return }
        let moved = lastDelivered.map { location.distance(from: $0) } ?? .greatestFiniteMagnitude
        let point = GeoPoint(latitude: location.coordinate.latitude, longitude: location.coordinate.longitude)
        coordinate = point
        UserDefaults.standard.set(point.latitude, forKey: Self.latitudeKey)
        UserDefaults.standard.set(point.longitude, forKey: Self.longitudeKey)
        guard moved > 300 else { return }
        lastDelivered = location
        onLocation?(point)
    }

    /// City name for a forecast. Uses the cached name when the fix has not moved far.
    func resolvedPlace(near point: GeoPoint) async -> String {
        if let placeName, !placeName.isEmpty, let coordinate,
           abs(coordinate.latitude - point.latitude) < 0.05,
           abs(coordinate.longitude - point.longitude) < 0.05 {
            return placeName
        }
        let location = CLLocation(latitude: point.latitude, longitude: point.longitude)
        let name = await Self.placeName(for: location)
        guard let name, !name.isEmpty else { return placeName ?? "" }
        placeName = name
        UserDefaults.standard.set(name, forKey: Self.placeKey)
        return name
    }

    private static func placeName(for location: CLLocation) async -> String? {
        guard let request = MKReverseGeocodingRequest(location: location) else { return nil }
        guard let item = try? await request.mapItems.first else { return nil }
        if let city = item.addressRepresentations?.cityName, !city.isEmpty {
            return city
        }
        if let short = item.address?.shortAddress, !short.isEmpty {
            return short
        }
        return item.addressRepresentations?.regionName
    }
}

extension LocationProvider: CLLocationManagerDelegate {
    nonisolated func locationManagerDidChangeAuthorization(_ manager: CLLocationManager) {
        let status = manager.authorizationStatus
        MainActor.assumeIsolated { self.apply(status) }
    }

    nonisolated func locationManager(_ manager: CLLocationManager, didUpdateLocations locations: [CLLocation]) {
        MainActor.assumeIsolated { self.handle(locations) }
    }

    nonisolated func locationManager(_ manager: CLLocationManager, didFailWithError error: Error) {
        _ = error
    }
}
