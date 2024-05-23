#include "timeFunctions.hpp"

// Variables
static time_t lastSec = 61;
static struct tm t_tm;
static struct timeval val;
//lv_obj_t *tick_index[62];

// Updates the time and date
void handleTime() {
    // Resets flags
    info.flag.secondChanged = 0;
    info.flag.minuteChanged = 0;
    info.flag.hourChanged = 0;

    gettimeofday(&val, NULL);
    getLocalTime(&t_tm);
    
    // Sets second variable
    //info.time.valSec = val.tv_sec % 60;

    // Handles seconds
    if (info.time.second != t_tm.tm_sec)
    {
        info.time.second = t_tm.tm_sec;
        info.flag.secondChanged = 1;

        // Handles minutes
        if (t_tm.tm_min != info.time.minute)
        {   
            info.time.minute = t_tm.tm_min;
            info.flag.minuteChanged;

            // Handles hours
            if (t_tm.tm_hour != info.time.hour)
            {   
                info.time.hour = t_tm.tm_hour;
                info.flag.hourChanged;
            }
        }
    }
}

// Draws the analogue and digital time as well as the date
void writeTime(){
    static char buffer[11];

    // Draws seconds and minutes only if they need to be changed
    if (info.flag.secondChanged || info.flag.refresh)
    {
        //lv_img_set_angle(ui_SecondHand, info.time.valSec * 60);
        //lv_img_set_angle(ui_MinuteHand, (t_tm.tm_min * 60) + (t_tm.tm_sec));

        // Draws hours only if they need to be changed
        if (info.flag.minuteChanged || info.flag.refresh)
        {
            //lv_img_set_angle(ui_HourHand, (t_tm.tm_hour * 300) + (t_tm.tm_min * 5));

            strftime(buffer, sizeof(buffer), "%I:%M", &t_tm);
            lv_label_set_text(ui_timeDigital, buffer);

            // Draws the date only if it needs to be changed
            if (info.flag.hourChanged || info.flag.refresh)
            {
                strftime(buffer, sizeof(buffer), "%a %b %e", &t_tm);
                lv_label_set_text(ui_date, buffer);
            }
        }
    }
}