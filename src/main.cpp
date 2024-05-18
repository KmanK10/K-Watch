#include <main.hpp>

TFT_eSPI tft;

Info info; // Create the info struct

// Variables:
bool pmuIRQ = false; // Whether there is a PMU IRQ or not
bool charging = 0;
int batPercent;
int realPercent;

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

    info.flag.refresh = true;

    // Initialize battery stuff
    batPercent = watch.getBatteryPercent();
    if (batPercent <= 20) lv_obj_set_style_bg_color(ui_batBar, lv_color_hex(0xFF0000), LV_PART_INDICATOR | LV_STATE_DEFAULT); // Make percent bar red

    // watch.setDateTime(2024, 5, 17, 11, 05, 00);
}

void loop() {
    if (info.flag.secondChanged) {
        updateBattery();
    }

    handleTime();
    writeTime();

    delay(lv_task_handler()); // Lets the GUI run in the background
}

// Set battery indicators
void updateBattery() {
    realPercent = watch.getBatteryPercent();

    // Started and stopped charging
    if (!charging && watch.isCharging()) {
        charging = true;
        lv_obj_set_style_bg_color(ui_batBar, lv_color_hex(0x00FF00), LV_PART_INDICATOR | LV_STATE_DEFAULT); // Make percent bar green
        vibrate();
        Serial.println("Charging");
    } else if (charging && !watch.isCharging())  {
        charging = false;
        lv_obj_set_style_bg_color(ui_batBar, lv_color_white(), LV_PART_INDICATOR | LV_STATE_DEFAULT); // Make percent bar white
    }

    // Set the batPercent label
    if (charging && realPercent < 100) lv_label_set_text_fmt(ui_batPercent, "%i%s", realPercent, LV_SYMBOL_CHARGE);
    else lv_label_set_text_fmt(ui_batPercent, "%i", realPercent);

    // Set the battery icon based on battery percentage
    lv_bar_set_value(ui_batBar, realPercent, LV_ANIM_OFF);

    printf("realPercent: %i batPercent: %i\n", realPercent, batPercent);

    // Set battery icon color if battery is too low
    if (realPercent <= 20 && batPercent > 20) lv_obj_set_style_bg_color(ui_batBar, lv_color_hex(0xFF0000), LV_PART_INDICATOR | LV_STATE_DEFAULT); // Make percent bar red
    else if (realPercent > 20 && batPercent <= 20) { lv_obj_set_style_bg_color(ui_batBar, lv_color_white(), LV_PART_INDICATOR | LV_STATE_DEFAULT); // Make percent bar white
        Serial.println("Battery low");
    }

    batPercent = realPercent;
}