#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <RobotEye.h>

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
RobotEye eye(&u8g2);

// 自定义表情
FaceParams myCustomEye(const FaceParams& base, int16_t, int16_t) {
    FaceParams f = base;
    f.left.bottomCut = f.right.bottomCut = 10;
    return f;
}

void setup() {
    Wire.begin(8, 9);
    u8g2.begin();
    eye.begin();

    eye.setExprGenerator(AnimeEye::EXPR_USER_1, myCustomEye);
    eye.setExpression(AnimeEye::EXPR_USER_1, false);  // ← 只调用一次，触发过渡
}

void loop() {
    eye.update();  // ← 只推进动画，不再调用 setExpression
}
