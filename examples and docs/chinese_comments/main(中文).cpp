#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <RobotEye.h>

// ============================================================
//  硬件配置（换屏幕/引脚只改这里）
// ============================================================
#define I2C_SDA_PIN     8       // I2C SDA 引脚
#define I2C_SCL_PIN     9       // I2C SCL 引脚

// U8g2 驱动对象：SH1106 128×64 硬件I2C 全屏缓冲
// 换其他屏幕：修改这一行驱动类型即可（如 U8G2_SSD1306_128X64_NONAME_F_HW_I2C）
// RobotEye 自动从该对象读取屏幕尺寸并居中
// 页缓冲：将驱动类型中的 _F_ 改为 _1_（页缓冲），
//           并在 robot_eye.hpp 中取消 #define RE_PAGE_BUFFER 的注释
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

RobotEye eye(&u8g2);

// ============================================================
//  【自定义表情示例】
//  定义一个生成函数，签名固定：
//    FaceParams 函数名(const FaceParams& base, int16_t lcy, int16_t rcy)
//  然后用 setExprGenerator 注册到任意槽位（内置或 EXPR_USER_1~8）
// ============================================================
// FaceParams myCustomEye(const FaceParams& base, int16_t, int16_t) {
//     FaceParams f = base;
//     f.left.bottomCut = f.right.bottomCut = 10;  // 自定义裁切
//     return f;
// }

// ============================================================
//  测试表情列表
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

    // 初始化 RobotEye
    // begin() 不传参 → 自动选择平台随机种子
    // begin(12345) → 使用固定种子（随机序列可复现，方便调试）
    eye.begin();

    // 注册自定义表情到用户槽位（取消注释即可测试）
    // eye.setExprGenerator(RobotEye::EXPR_USER_1, myCustomEye);

    Serial.println("=== RobotEye v3.0 ===");
}

void loop() {
    // 每帧更新（必须高频调用，不要在 loop 里用 delay）
    eye.update();
    uint32_t now = millis();

    switch (testPhase) {
    case PHASE_WAIT_BOOT:
        // 等待开机动画完成（约5.5秒，留余量到6.5秒）
        if (now > 6500 && !eye.isBusy()) {
            testPhase = PHASE_EYE_CYCLE;
            exprIndex = 0;
            lastSwitch = now;
            Serial.println("开始循环眼睛表情...");
        }
        break;

    case PHASE_EYE_CYCLE:
        if (!eye.isBusy() && now - lastSwitch >= HOLD_MS) {
            eye.setExpression(eyeExprs[exprIndex], false);
            Serial.print("眼睛表情["); Serial.print(exprIndex); Serial.println("]");
            exprIndex++;
            lastSwitch = now;
            if (exprIndex >= eyeExprCount) {
                testPhase = PHASE_SPECIAL_CYCLE;
                exprIndex = 0;
                Serial.println("--- 开始特殊表情 ---");
            }
        }
        break;

    case PHASE_SPECIAL_CYCLE:
        if (!eye.isBusy() && now - lastSwitch >= HOLD_MS) {
            RobotEye::Expression e = specialExprs[exprIndex];

            if (e == RobotEye::EXPR_BATTERY) {
                // 电池分5档：0% → 25% → 50% → 75% → 100%
                static uint8_t batStep = 0;
                eye.setExpression(RobotEye::EXPR_BATTERY);
                eye.setBatteryLevel(batStep * 25);
                Serial.print("电池: "); Serial.print(batStep * 25); Serial.println("%");
                batStep++;
                if (batStep > 4) { batStep = 0; exprIndex++; }
            } else {
                eye.setExpression(e);
                Serial.print("特殊表情["); Serial.print(exprIndex); Serial.println("]");
                exprIndex++;
            }

            lastSwitch = now;
            if (exprIndex >= specialExprCount) {
                testPhase = PHASE_IDLE_LIVE;
                eye.setExpression(RobotEye::EXPR_NORMAL);
                Serial.println("--- 全部展示完毕，进入待机 ---");
            }
        }
        break;

    case PHASE_IDLE_LIVE:
        // 观察自动眨眼、瞟眼、半眯眼、随机漂移
        // 也可以在此处手动调用 eye.setExpression(...) 测试
        break;
    }
}
