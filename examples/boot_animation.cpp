#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <RobotEye.h>

// ============================================================
//  Test: Boot Animation
//  What it tests:
//    The complete power-on sequence:
//    closed eyes (500ms) -> transition to half-open (1500ms) ->
//    blink 3 times (300ms gap) -> transition to fully open (1500ms)
//    After boot finishes, stays in NORMAL idle so you can also
//    observe automatic blinking / glancing / drift.
// ============================================================

// ============================================================
//  Hardware config (change pins here if needed)
// ============================================================
#define I2C_SDA_PIN     8       // I2C SDA pin
#define I2C_SCL_PIN     9       // I2C SCL pin

// U8g2 driver: SH1106 128x64 hardware I2C, full framebuffer
// For other screens: change this line (e.g. U8G2_SSD1306_128X64_NONAME_F_HW_I2C)
// RobotEye reads the actual screen size from this object and auto-centers.
// Page buffer: change _F_ to _1_ in the driver type,
//             and uncomment #define RE_PAGE_BUFFER in robot_eye.hpp
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

RobotEye eye(&u8g2);

void setup() {
    Serial.begin(115200);

    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

    u8g2.begin();
    u8g2.setContrast(200);
    u8g2.clearBuffer();
    u8g2.sendBuffer();

    // Initialize RobotEye
    // begin() with no arg -> auto-select platform random seed
    // begin(12345) -> fixed seed (reproducible random sequence, useful for debugging)
    eye.begin();

    Serial.println("=== Boot Animation Test ===");
}

void loop() {
    // Update every frame (must be called frequently, do NOT use delay() in loop)
    eye.update();

    // After boot animation completes, the eyes stay in NORMAL idle.
    // You can observe: auto blink, random glance, half-blink (squint), drift.
    // No manual expression switching here.
}
