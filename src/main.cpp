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
    if (watch.isCharging()) charging = true;

    // watch.setDateTime(2024, 5, 20, 9, 58, 45);
}

void loop() {
    handleTime();

    // Handles power interupt requests
    if (pmuIRQ) {
        watch.readPMU();

		// Short button presses
        if (watch.isPekeyShortPressIrq()) {
            // We are on the main screen
            if (lv_disp_get_scr_act(NULL) == ui_main) {
                // We are on the home screen
                if (lv_tileview_get_tile_act(ui_mainTV) == ui_home) {
                    lv_obj_set_tile(ui_mainTV, ui_apps, LV_ANIM_OFF); // Go to apps
                } else lv_obj_set_tile(ui_mainTV, ui_home, LV_ANIM_OFF); // We are not on the home screen, go home
                
            }
            // We are not on the main screen
            else {
                // Go home
                initMainScreen();
                lv_scr_load(ui_main);
            }
        }

        // Started charging
		else if (watch.isBatChagerStartIrq()) {
            charging = true;
            info.flag.refreshBatColor = true;
            vibrate();
        }

        watch.clearPMU();
    }

    // Update once per second
    if (info.flag.secondChanged || info.flag.refresh) {
        // Started and stopped charging
        if (charging && !watch.isCharging()) {
            charging = false;
            info.flag.refreshBatColor = true;
        }

        // Check if we are on the main screen
        if (lv_disp_get_scr_act(NULL) == ui_main) {
            // Check if we are on the home screen
            if (lv_tileview_get_tile_act(ui_mainTV) == ui_home) {
                updateBattery();
                writeTime();
            }
        }
    }

    delay(lv_task_handler()); // Lets the GUI run in the background

    info.flag.refresh = false;
}

// Set battery indicators
void updateBattery() {
    realPercent = watch.getBatteryPercent();

    if (info.flag.refreshBatColor) {
        info.flag.refreshBatColor = false;
        if (watch.isCharging()) {
            lv_obj_set_style_bg_color(ui_batBar, lv_color_hex(0x00FF55), LV_PART_INDICATOR | LV_STATE_DEFAULT); // Make percent bar green
        } else {
            lv_obj_set_style_bg_color(ui_batBar, lv_color_white(), LV_PART_INDICATOR | LV_STATE_DEFAULT); // Make percent bar white
        }

        if (realPercent <= 20 && batPercent > 20) lv_obj_set_style_bg_color(ui_batBar, lv_color_hex(0xFF0000), LV_PART_INDICATOR | LV_STATE_DEFAULT); // Make percent bar red
    }

    // Set the batPercent label
    if (charging && realPercent < 100) lv_label_set_text_fmt(ui_batPercent, "%i%s", realPercent, LV_SYMBOL_CHARGE);
    else lv_label_set_text_fmt(ui_batPercent, "%i", realPercent);

    // Set the battery icon based on battery percentage
    lv_bar_set_value(ui_batBar, realPercent, LV_ANIM_OFF);

    // Set battery icon color if battery is too low
    if (realPercent <= 20 && batPercent > 20) lv_obj_set_style_bg_color(ui_batBar, lv_color_hex(0xFF0000), LV_PART_INDICATOR | LV_STATE_DEFAULT); // Make percent bar red
    else if (realPercent > 20 && batPercent <= 20) lv_obj_set_style_bg_color(ui_batBar, lv_color_white(), LV_PART_INDICATOR | LV_STATE_DEFAULT); // Make percent bar white

    batPercent = realPercent;
}