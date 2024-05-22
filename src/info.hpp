#include <LilyGoLib.h>

#ifndef INFOSTRUCT
#define INFOSTRUCT

struct Info {
    typedef struct {
        bool secondChanged = 1;
        bool minuteChanged = 1;
        bool hourChanged = 1;
        bool refresh = 1; // If the screen needs to be refreshed
        bool refreshBatColor = 1;
    } FlagInfo;
    FlagInfo flag;

    typedef struct {
        uint8_t hour;
        uint8_t minute;
        uint8_t second;
        uint8_t valSec;
        String date;
    } TimeInfo;
    TimeInfo time;

    typedef struct {
        int brightness = 100;
    } UtilityInfo;
    UtilityInfo utility;
};

#endif

extern Info info;