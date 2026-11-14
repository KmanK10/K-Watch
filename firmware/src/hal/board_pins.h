#pragma once

// LilyGo T-Watch pin maps. Everything hardware-specific lives under hal/
// so the rest of the firmware can move to the custom STM32 board unchanged.

#define BOARD_TFT_WIDTH        240
#define BOARD_TFT_HEIGHT       240

#if defined(BOARD_TWATCH_S3) && defined(BOARD_TWATCH_2020_V3)
#error "Select exactly one T-Watch board"
#elif defined(BOARD_TWATCH_S3)

// BMA423: x <- y, y <- x, z <- -z.
#define BOARD_IMU_REMAP0       0x81
#define BOARD_IMU_REMAP1       0x01
#define BOARD_TOUCH_RST        -1

// ST7789 display (SPI)
#define BOARD_TFT_MOSI         13
#define BOARD_TFT_SCLK         18
#define BOARD_TFT_CS           12
#define BOARD_TFT_DC           38
#define BOARD_TFT_BL           45

// FT6336 touch (own I2C bus)
#define BOARD_TOUCH_SDA        39
#define BOARD_TOUCH_SCL        40
#define BOARD_TOUCH_INT        16

// Shared I2C bus: BMA423, PCF8563, AXP2101, DRV2605
#define BOARD_I2C_SDA          10
#define BOARD_I2C_SCL          11

#define BOARD_RTC_INT          17
#define BOARD_PMU_INT          21
#define BOARD_BMA423_INT1      14

#define BOARD_IR_PIN           2

// MAX98357A speaker amp (I2S)
#define BOARD_DAC_IIS_BCK      48
#define BOARD_DAC_IIS_WS       15
#define BOARD_DAC_IIS_DOUT     46

// SX1262 LoRa radio
#define BOARD_RADIO_SCK        3
#define BOARD_RADIO_MISO       4
#define BOARD_RADIO_MOSI       1
#define BOARD_RADIO_SS         5
#define BOARD_RADIO_DIO1       9
#define BOARD_RADIO_RST        8
#define BOARD_RADIO_BUSY       7

// PDM microphone
#define BOARD_MIC_DATA         47
#define BOARD_MIC_CLOCK        44

#elif defined(BOARD_TWATCH_2020_V3)

// Reference: LilyGo TTGO_TWatch_Library/src/board/twatch2020_v3.h.
#define BOARD_TFT_MOSI         19
#define BOARD_TFT_SCLK         18
#define BOARD_TFT_CS           5
#define BOARD_TFT_DC           27
#define BOARD_TFT_BL           15

#define BOARD_TOUCH_SDA        23
#define BOARD_TOUCH_SCL        32
#define BOARD_TOUCH_INT        38
#define BOARD_TOUCH_RST        14

// Shared I2C bus: BMA423, PCF8563, AXP202.
#define BOARD_I2C_SDA          21
#define BOARD_I2C_SCL          22
#define BOARD_RTC_INT          37
#define BOARD_PMU_INT          35
#define BOARD_BMA423_INT1      39
// V3 sensor mounting: x <- -y, y <- -x, z <- -z.
#define BOARD_IMU_REMAP0       0xA5
#define BOARD_IMU_REMAP1       0x01

#define BOARD_MOTOR_PIN        4
#define BOARD_IR_PIN           13
#define BOARD_DAC_IIS_BCK      26
#define BOARD_DAC_IIS_WS        25
#define BOARD_DAC_IIS_DOUT      33
#define BOARD_MIC_DATA         2
#define BOARD_MIC_CLOCK        0

#else
#error "Unsupported board: select twatch-s3 or twatch-2020-v3"
#endif
