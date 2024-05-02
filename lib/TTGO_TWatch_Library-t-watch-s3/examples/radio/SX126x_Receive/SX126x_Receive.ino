/*
   RadioLib SX126x Receive Example

   This example listens for LoRa transmissions using SX126x Lora modules.
   To successfully receive data, the following settings have to be the same
   on both transmitter and receiver:
    - carrier frequency
    - bandwidth
    - spreading factor
    - coding rate
    - sync word
    - preamble length

   Other modules from SX126x family can also be used.

   For default module settings, see the wiki page
   https://github.com/jgromes/RadioLib/wiki/Default-configuration#sx126x---lora-modem

   For full API reference, see the GitHub Pages
   https://jgromes.github.io/RadioLib/
*/

// include the library
#include <LilyGoLib.h>
#include <LV_Helper.h>

lv_obj_t *label1;

void setup()
{
    Serial.begin(115200);

    watch.begin();

    // set carrier frequency to 433.5 MHz
    if (watch.setFrequency(433.5) == RADIOLIB_ERR_INVALID_FREQUENCY) {
        Serial.println(F("Selected frequency is invalid for this module!"));
        while (true);
    }

    beginLvglHelper();

    label1 = lv_label_create(lv_scr_act());
    lv_label_set_recolor(label1, true);    /*Enable re-coloring by commands in the text*/
    lv_label_set_text(label1, "Waiting for incoming transmission");
    lv_obj_center(label1);

}

void loop()
{
    Serial.print(F("[SX1262] Waiting for incoming transmission ... "));

    // you can receive data as an Arduino String
    // NOTE: receive() is a blocking method!
    //       See example ReceiveInterrupt for details
    //       on non-blocking reception method.
    String str;
    int state = watch.receive(str);

    // you can also receive data as byte array
    /*
      byte byteArr[8];
      int state = watch.receive(byteArr, 8);
    */

    if (state == RADIOLIB_ERR_NONE) {
        // packet was successfully received
        Serial.println(F("success!"));

        // print the data of the packet
        Serial.print(F("[SX1262] Data:\t\t"));
        Serial.println(str);

        // print the RSSI (Received Signal Strength Indicator)
        // of the last received packet
        Serial.print(F("[SX1262] RSSI:\t\t"));
        Serial.print(watch.getRSSI());
        Serial.println(F(" dBm"));

        // print the SNR (Signal-to-Noise Ratio)
        // of the last received packet
        Serial.print(F("[SX1262] SNR:\t\t"));
        Serial.print(watch.getSNR());
        Serial.println(F(" dB"));

        // print frequency error
        Serial.print(F("[SX1262] Frequency error:\t"));
        Serial.print(watch.getFrequencyError());
        Serial.println(F(" Hz"));


    } else if (state == RADIOLIB_ERR_RX_TIMEOUT) {
        // timeout occurred while waiting for a packet
        Serial.println(F("timeout!"));

    } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
        // packet was received, but is malformed
        Serial.println(F("CRC error!"));

    } else {
        // some other error occurred
        Serial.print(F("failed, code "));
        Serial.println(state);

    }
    lv_task_handler();
    delay(5);
}
