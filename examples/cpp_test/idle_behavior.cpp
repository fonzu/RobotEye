#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <RobotEye.h>

// ============================================================
//  Test: Idle Behavior
//  What it tests:
//    After boot, stays in NORMAL expression forever so you can
//    observe the automatic idle animations:
//      1. Random blinking          (BLINK_INTERVAL_MIN/MAX)
//      2. Random glancing          (look up/down/left/right,
//                                   auto-return to center)
//      3. Random half-blink/squint (height micro-shrink)
//      4. Random position drift    (small smooth wandering)
//
//    To isolate a single behavior, set the corresponding debug
//    flag in robot_eye.hpp to true:
//      DEBUG_DISABLE_GAZE        - turn off glancing
//      DEBUG_DISABLE_DRIFT       - turn off drift
//      DEBUG_DISABLE_HALF_BLINK  - turn off half-blink
//      DEBUG_FIXED_BLINK         - fixed 5s blink interval
//      DEBUG_SKIP_BOOT           - skip boot animation
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

    Serial.println("=== Idle Behavior Test ===");
    Serial.println("Observing: blink, glance, half-blink, drift");
}

void loop() {
    // Update every frame (must be called frequently, do NOT use delay() in loop)
    eye.update();

    // No manual expression switching.
    // The eyes stay in NORMAL and run all automatic idle behaviors.
    // Use the debug flags in robot_eye.hpp to isolate individual behaviors.
}
