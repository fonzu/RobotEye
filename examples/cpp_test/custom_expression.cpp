#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <RobotEye.h>

// ============================================================
//  Test: Custom Expression
//  What it tests:
//    How to define a custom expression generator function and
//    register it into a user slot (EXPR_USER_1) via
//    setExprGenerator(). After boot, it morphs to the custom
//    expression, holds 4 seconds, then morphs back to NORMAL,
//    and repeats. This verifies that user-registered generators
//    are called correctly and participate in morph transitions.
//
//  Generator function signature (fixed):
//    FaceParams funcName(const FaceParams& base,
//                        int16_t leftCenterY, int16_t rightCenterY)
//    - base: base face (already includes drift and position offset)
//    - return: the FaceParams for your expression
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
//  Custom expression: narrow squint smile
//  - Height shrinks to 50% (keeps vertical center)
//  - Bottom cut creates an upward-curved smiling shape
//  - Corner radius scales proportionally
// ============================================================
FaceParams mySquintSmile(const FaceParams& base, int16_t lcy, int16_t rcy) {
    FaceParams f = base;

    uint8_t h = (uint8_t)(RobotEye::BASE_EYE_H * 0.5f);
    uint8_t r = (uint8_t)(RobotEye::BASE_EYE_R * 0.5f);

    f.left.h = f.right.h = h;
    f.left.r = f.right.r = r;
    f.left.y  = (int16_t)(lcy - h / 2);
    f.right.y = (int16_t)(rcy - h / 2);

    // Bottom cut to form smile curve
    f.left.bottomCut = f.right.bottomCut = 6;

    return f;
}

// ============================================================
//  Another custom expression: wide alert bars
//  - Height very short, width full, like status indicators
// ============================================================
FaceParams myAlertBars(const FaceParams& base, int16_t lcy, int16_t rcy) {
    FaceParams f = base;

    f.left.h = f.right.h = 6;
    f.left.r = f.right.r = 3;
    f.left.y  = (int16_t)(lcy - 3);
    f.right.y = (int16_t)(rcy - 3);

    return f;
}

enum TestPhase : uint8_t {
    PHASE_WAIT_BOOT = 0,   // Wait for boot animation to complete
    PHASE_SHOW_CUSTOM_1,   // Morph to first custom expression
    PHASE_SHOW_CUSTOM_2,   // Morph to second custom expression
    PHASE_SHOW_NORMAL,     // Morph back to NORMAL
};
TestPhase testPhase = PHASE_WAIT_BOOT;
uint32_t lastSwitch = 0;
const uint32_t HOLD_MS = 4000;   // Hold each expression for 4 seconds

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

    // Register custom expressions into user slots
    eye.setExprGenerator(RobotEye::EXPR_USER_1, mySquintSmile);
    eye.setExprGenerator(RobotEye::EXPR_USER_2, myAlertBars);

    Serial.println("=== Custom Expression Test ===");
}

void loop() {
    // Update every frame (must be called frequently, do NOT use delay() in loop)
    eye.update();
    uint32_t now = millis();

    switch (testPhase) {
    case PHASE_WAIT_BOOT:
        // Wait for boot animation to finish (~5.5s, allow margin to 6.5s)
        if (now > 6500 && !eye.isBusy()) {
            testPhase = PHASE_SHOW_CUSTOM_1;
            lastSwitch = now;
            eye.setExpression(RobotEye::EXPR_USER_1, false);
            Serial.println("Custom 1: squint smile");
        }
        break;

    case PHASE_SHOW_CUSTOM_1:
        if (!eye.isBusy() && now - lastSwitch >= HOLD_MS) {
            testPhase = PHASE_SHOW_CUSTOM_2;
            lastSwitch = now;
            eye.setExpression(RobotEye::EXPR_USER_2, false);
            Serial.println("Custom 2: alert bars");
        }
        break;

    case PHASE_SHOW_CUSTOM_2:
        if (!eye.isBusy() && now - lastSwitch >= HOLD_MS) {
            testPhase = PHASE_SHOW_NORMAL;
            lastSwitch = now;
            eye.setExpression(RobotEye::EXPR_NORMAL, false);
            Serial.println("Back to NORMAL");
        }
        break;

    case PHASE_SHOW_NORMAL:
        if (!eye.isBusy() && now - lastSwitch >= HOLD_MS) {
            testPhase = PHASE_SHOW_CUSTOM_1;
            lastSwitch = now;
            eye.setExpression(RobotEye::EXPR_USER_1, false);
            Serial.println("Custom 1: squint smile");
        }
        break;
    }
}
