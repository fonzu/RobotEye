#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <RobotEye.h>

// ============================================================
//  Test: Special Expressions
//  What it tests:
//    1. Battery icon with 5 charge levels: 0% -> 25% -> 50% ->
//       75% -> 100% (vector segmented grid, setBatteryLevel).
//    2. The 4 bitmap special expressions: Warning, Left Signal,
//       Right Signal, Mode (requires Irisoled library and
//       RE_ENABLE_BITMAP_EXPR defined in robot_eye.hpp).
//    Each item is held for 3 seconds. After the full cycle it
//    returns to NORMAL idle.
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

// ============================================================
//  List of special expressions (battery is handled separately
//  because it needs setBatteryLevel calls)
// ============================================================
const RobotEye::Expression specialExprs[] = {
    RobotEye::EXPR_WARNING,
    RobotEye::EXPR_LEFT_SIGNAL,
    RobotEye::EXPR_RIGHT_SIGNAL,
    RobotEye::EXPR_MODE,
};
const uint8_t specialExprCount = sizeof(specialExprs) / sizeof(specialExprs[0]);

enum TestPhase : uint8_t {
    PHASE_WAIT_BOOT = 0,   // Wait for boot animation to complete
    PHASE_BATTERY,         // Cycle battery 0% -> 100% in 5 steps
    PHASE_BITMAP,          // Cycle 4 bitmap expressions
    PHASE_DONE,            // Return to NORMAL idle
};
TestPhase testPhase = PHASE_WAIT_BOOT;
uint8_t exprIndex = 0;
uint8_t batStep = 0;
uint32_t lastSwitch = 0;
const uint32_t HOLD_MS = 3000;   // Hold each item for 3 seconds

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

    Serial.println("=== Special Expressions Test ===");
}

void loop() {
    // Update every frame (must be called frequently, do NOT use delay() in loop)
    eye.update();
    uint32_t now = millis();

    switch (testPhase) {
    case PHASE_WAIT_BOOT:
        // Wait for boot animation to finish (~5.5s, allow margin to 6.5s)
        if (now > 6500 && !eye.isBusy()) {
            testPhase = PHASE_BATTERY;
            batStep = 0;
            lastSwitch = now;
            Serial.println("--- Start battery levels ---");
        }
        break;

    case PHASE_BATTERY:
        if (!eye.isBusy() && now - lastSwitch >= HOLD_MS) {
            // Battery in 5 steps: 0% -> 25% -> 50% -> 75% -> 100%
            eye.setExpression(RobotEye::EXPR_BATTERY);
            eye.setBatteryLevel(batStep * 25);
            Serial.print("Battery: "); Serial.print(batStep * 25); Serial.println("%");
            batStep++;
            lastSwitch = now;
            if (batStep > 4) {
                testPhase = PHASE_BITMAP;
                exprIndex = 0;
                Serial.println("--- Start bitmap expressions ---");
            }
        }
        break;

    case PHASE_BITMAP:
        if (!eye.isBusy() && now - lastSwitch >= HOLD_MS) {
            eye.setExpression(specialExprs[exprIndex]);
            Serial.print("Bitmap expression ["); Serial.print(exprIndex); Serial.println("]");
            exprIndex++;
            lastSwitch = now;
            if (exprIndex >= specialExprCount) {
                testPhase = PHASE_DONE;
                eye.setExpression(RobotEye::EXPR_NORMAL);
                Serial.println("--- All done, back to NORMAL idle ---");
            }
        }
        break;

    case PHASE_DONE:
        // Stay in NORMAL idle
        break;
    }
}
