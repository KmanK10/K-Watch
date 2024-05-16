#include <main.hpp>

TFT_eSPI tft;

Info info; // Create the info struct

// Variables:
bool pmuIRQ = false; // Whether there is a PMU IRQ or not
static struct tm t_tm;
static char buffer[11];
int num = 0;

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

    // watch.setDateTime(2024, 5, 13, 13, 40, 30);
}

void loop() {
    //lv_label_set_text(batPercentLabel, String(watch.getBatteryPercent()).c_str());

    //if (watch.getTouched()) vibrate();

    handleTime();
    writeTime();

    delay(lv_task_handler()); // Lets the GUI run in the background
}