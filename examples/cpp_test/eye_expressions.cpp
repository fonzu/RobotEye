#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <RobotEye.h>

// ============================================================
//  Test: Eye Expressions Cycle (morph transition)
//  What it tests:
//    Waits for boot animation to finish, then cycles through all
//    24 vector eye expressions one by one. Each expression is held
//    for 3 seconds. Switching uses morph mode (useBlink=false),
//    so all geometric parameters smoothly interpolate over 500ms.
//    This verifies every expression draws correctly and that morph
//    transitions between any two expressions are smooth.
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
//  List of all 24 vector eye expressions to cycle through
// ============================================================
const RobotEye::Expression eyeExprs[] = {
    RobotEye::EXPR_NORMAL,
    RobotEye::EXPR_HAPPY,
    RobotEye::EXPR_ANGRY,
    RobotEye::EXPR_SAD,
    RobotEye::EXPR_SURPRISED,
    RobotEye::EXPR_SLEEPY,
    RobotEye::EXPR_EXCITED,
    RobotEye::EXPR_SCARED,
    RobotEye::EXPR_FOCUSED,
    RobotEye::EXPR_WORRIED,
    RobotEye::EXPR_DESPAIR,
    RobotEye::EXPR_ALERT,
    RobotEye::EXPR_BLINK,
    RobotEye::EXPR_BLINK_DOWN,
    RobotEye::EXPR_BLINK_UP,
    RobotEye::EXPR_DISORIENTED,
    RobotEye::EXPR_FURIOUS,
    RobotEye::EXPR_LOOK_DOWN,
    RobotEye::EXPR_LOOK_UP,
    RobotEye::EXPR_LOOK_LEFT,
    RobotEye::EXPR_LOOK_RIGHT,
    RobotEye::EXPR_WINK_LEFT,
    RobotEye::EXPR_WINK_RIGHT,
    RobotEye::EXPR_BORED,
};
const uint8_t eyeExprCount = sizeof(eyeExprs) / sizeof(eyeExprs[0]);

enum TestPhase : uint8_t {
    PHASE_WAIT_BOOT = 0,   // Wait for boot animation to complete
    PHASE_EYE_CYCLE,       // Cycle through all eye expressions
};
TestPhase testPhase = PHASE_WAIT_BOOT;
uint8_t exprIndex = 0;
uint32_t lastSwitch = 0;
const uint32_t HOLD_MS = 3000;   // Hold each expression for 3 seconds

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

    Serial.println("=== Eye Expressions Cycle Test ===");
}

void loop() {
    // Update every frame (must be called frequently, do NOT use delay() in loop)
    eye.update();
    uint32_t now = millis();

    switch (testPhase) {
    case PHASE_WAIT_BOOT:
        // Wait for boot animation to finish (~5.5s, allow margin to 6.5s)
        if (now > 6500 && !eye.isBusy()) {
            testPhase = PHASE_EYE_CYCLE;
            exprIndex = 0;
            lastSwitch = now;
            Serial.println("Start cycling eye expressions...");
        }
        break;

    case PHASE_EYE_CYCLE:
        // Switch to next expression when not busy and hold time elapsed
        if (!eye.isBusy() && now - lastSwitch >= HOLD_MS) {
            eye.setExpression(eyeExprs[exprIndex], false);   // false = morph transition
            Serial.print("Eye expression ["); Serial.print(exprIndex); Serial.println("]");
            exprIndex++;
            lastSwitch = now;
            // Loop back to the first expression after reaching the end
            if (exprIndex >= eyeExprCount) {
                exprIndex = 0;
                Serial.println("--- Restart cycle ---");
            }
        }
        break;
    }
}
