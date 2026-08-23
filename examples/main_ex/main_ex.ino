#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <RobotEye.h>
// ============================================================
//  Hardware Configuration (Only modify here when changing screen/pins)
// ============================================================
#define I2C_SDA_PIN     8       // I2C SDA pin
#define I2C_SCL_PIN     9       // I2C SCL pin
// U8g2 driver object: SH1106 128×64 Hardware I2C full frame buffer
// To use other screens: modify the driver line below (e.g. U8G2_SSD1306_128X64_NONAME_F_HW_I2C)
// RobotEye automatically reads screen size from this object and centers content
// Page buffer: change _F_ to _1_ in driver definition,
//              and uncomment #define RE_PAGE_BUFFER in robot_eye.hpp
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
RobotEye eye(&u8g2);
// ============================================================
//  Custom Expression Example
//  Define a generator function with fixed signature:
//    FaceParams functionName(const FaceParams& base, int16_t lcy, int16_t rcy)
//  Then register it to any slot (built-in or EXPR_USER_1~8) using setExprGenerator
// ============================================================
// FaceParams myCustomEye(const FaceParams& base, int16_t, int16_t) {
//     FaceParams f = base;
//     f.left.bottomCut = f.right.bottomCut = 10;  // Custom cut value
//     return f;
// }
// ============================================================
//  Test Expression List
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
const RobotEye::Expression specialExprs[] = {
    RobotEye::EXPR_BATTERY,
    RobotEye::EXPR_WARNING,
    RobotEye::EXPR_LEFT_SIGNAL,
    RobotEye::EXPR_RIGHT_SIGNAL,
    RobotEye::EXPR_MODE,
};
const uint8_t specialExprCount = sizeof(specialExprs) / sizeof(specialExprs[0]);
enum TestPhase : uint8_t {
    PHASE_WAIT_BOOT = 0,
    PHASE_EYE_CYCLE,
    PHASE_SPECIAL_CYCLE,
    PHASE_IDLE_LIVE,
};
TestPhase testPhase = PHASE_WAIT_BOOT;
uint8_t exprIndex = 0;
uint32_t lastSwitch = 0;
const uint32_t HOLD_MS = 3000;
void setup() {
    Serial.begin(115200);
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    u8g2.begin();
    u8g2.setContrast(200);
    u8g2.clearBuffer();
    u8g2.sendBuffer();
    // Initialize RobotEye
    // begin() without parameter → auto select platform random seed
    // begin(12345) → fixed seed (reproducible random sequence for debugging)
    eye.begin();
    // Register custom expression to user slot (uncomment to test)
    // eye.setExprGenerator(RobotEye::EXPR_USER_1, myCustomEye);
    Serial.println("=== RobotEye v3.0 ===");
}
void loop() {
    // Update every frame (must be called frequently, do not use delay inside loop)
    eye.update();
    uint32_t now = millis();
    switch (testPhase) {
    case PHASE_WAIT_BOOT:
        // Wait for boot animation finish (~5.5s, reserve margin to 6.5s)
        if (now > 6500 && !eye.isBusy()) {
            testPhase = PHASE_EYE_CYCLE;
            exprIndex = 0;
            lastSwitch = now;
            Serial.println("Start cycling eye expressions...");
        }
        break;
    case PHASE_EYE_CYCLE:
        if (!eye.isBusy() && now - lastSwitch >= HOLD_MS) {
            eye.setExpression(eyeExprs[exprIndex], false);
            Serial.print("Eye expression ["); Serial.print(exprIndex); Serial.println("]");
            exprIndex++;
            lastSwitch = now;
            if (exprIndex >= eyeExprCount) {
                testPhase = PHASE_SPECIAL_CYCLE;
                exprIndex = 0;
                Serial.println("--- Start special expressions ---");
            }
        }
        break;
    case PHASE_SPECIAL_CYCLE:
        if (!eye.isBusy() && now - lastSwitch >= HOLD_MS) {
            RobotEye::Expression e = specialExprs[exprIndex];
            if (e == RobotEye::EXPR_BATTERY) {
                // Battery 5 levels: 0% → 25% → 50% → 75% → 100%
                static uint8_t batStep = 0;
                eye.setExpression(RobotEye::EXPR_BATTERY);
                eye.setBatteryLevel(batStep * 25);
                Serial.print("Battery: "); Serial.print(batStep * 25); Serial.println("%");
                batStep++;
                if (batStep > 4) { batStep = 0; exprIndex++; }
            } else {
                eye.setExpression(e);
                Serial.print("Special expression ["); Serial.print(exprIndex); Serial.println("]");
                exprIndex++;
            }
            lastSwitch = now;
            if (exprIndex >= specialExprCount) {
                testPhase = PHASE_IDLE_LIVE;
                eye.setExpression(RobotEye::EXPR_NORMAL);
                Serial.println("--- All demos finished, enter idle mode ---");
            }
        }
        break;
    case PHASE_IDLE_LIVE:
        // Observe auto blink, glance, half-close eyes, random drift
        // You can manually call eye.setExpression(...) here for testing
        break;
    }
}
