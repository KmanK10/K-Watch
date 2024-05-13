#include <main.hpp>

TFT_eSPI tft;

// Variables:
bool pmuIRQ = false; // Whether there is a PMU IRQ or not

// Callback for button
void setPMUFlag() {
	pmuIRQ = true;
}

void setup() {
    Serial.begin(9600);
    // Initialize watch
    watch.begin();
    watch.configInterrupt();
	watch.enableFeature(
		SensorBMA423::FEATURE_STEP_CNTR |
		SensorBMA423::FEATURE_ANY_MOTION |
		SensorBMA423::FEATURE_NO_MOTION |
		SensorBMA423::FEATURE_ACTIVITY |
		SensorBMA423::FEATURE_TILT |
		SensorBMA423::FEATURE_WAKEUP,
		true);
    watch.attachPMU(setPMUFlag);

    beginLvglHelper(); // initialize LVGL

    initUI();

    watch.setBrightness(100);

    //watch.setDateTime(2024, 5, 13, 13, 40, 30);
}

void loop() {

}