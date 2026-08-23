#include <RobotEye.h>

#ifdef RE_ENABLE_BITMAP_EXPR
  #include "Irisoled.h"
#endif

#include <math.h>

// ============================================================
//  平台兼容：PROGMEM 字节读取
// ============================================================
#if defined(__AVR__)
  #include <avr/pgmspace.h>
  #define RE_PGM_READ_BYTE(addr) pgm_read_byte(addr)
#else
  #define RE_PGM_READ_BYTE(addr) (*(const uint8_t*)(addr))
#endif

// ============================================================
//  绘制颜色（U8g2：1=白色绘制，0=黑色擦除）
//  内部使用，不暴露头文件
// ============================================================
namespace {
    constexpr uint8_t COL_WHITE = 1;
    constexpr uint8_t COL_BLACK = 0;
}


// ============================================================
//
//          内置表情生成函数（可被 setExprGenerator 替换）
//  每个函数接收：base=基准脸（含漂移/偏移），lcy/rcy=左右眼中心Y
//  返回该表情的 FaceParams。所有参数引用 RobotEye:: 常量
//
// ============================================================
namespace {

// normal：基准脸无变换
FaceParams re_gen_normal(const FaceParams& base, int16_t, int16_t) {
    return base;
}

// happy：底部裁切形成上弯笑眼
FaceParams re_gen_happy(const FaceParams& base, int16_t, int16_t) {
    FaceParams f = base;
    uint8_t cut = (uint8_t)(RobotEye::BASE_EYE_H * RobotEye::HAPPY_BOTTOM_CUT_RATIO);
    f.left.bottomCut = f.right.bottomCut = cut;
    return f;
}

// angry：保持normal完整尺寸+底部原生圆角，仅从顶部裁切
// 左眼左高右低，右眼右高左低
FaceParams re_gen_angry(const FaceParams& base, int16_t, int16_t) {
    FaceParams f = base;
    uint8_t cutH  = (uint8_t)(RobotEye::BASE_EYE_H * (1.0f - RobotEye::ANGRY_HEIGHT_RATIO));
    uint8_t slope = (uint8_t)(RobotEye::BASE_EYE_H * RobotEye::ANGRY_SLOPE_RATIO);
    f.left.topSlopeL  = cutH;        f.left.topSlopeR  = cutH + slope;
    f.right.topSlopeL = cutH + slope; f.right.topSlopeR = cutH;
    return f;
}

// sad：顶部斜线（外低内高八字眉）+整体下移
FaceParams re_gen_sad(const FaceParams& base, int16_t, int16_t) {
    FaceParams f = base;
    uint8_t hi = (uint8_t)(RobotEye::BASE_EYE_H * RobotEye::SAD_SLOPE_HIGH_RATIO);
    uint8_t lo = (uint8_t)(RobotEye::BASE_EYE_H * RobotEye::SAD_SLOPE_LOW_RATIO);
    f.left.y  = (int16_t)(base.left.y  + RobotEye::SAD_DOWN_OFFSET);
    f.right.y = (int16_t)(base.right.y + RobotEye::SAD_DOWN_OFFSET);
    f.left.topSlopeL  = lo;  f.left.topSlopeR  = hi;
    f.right.topSlopeL = hi;  f.right.topSlopeR = lo;
    return f;
}

// surprised：两眼向中间靠近
FaceParams re_gen_surprised(const FaceParams& base, int16_t, int16_t) {
    FaceParams f = base;
    f.left.x  = (int16_t)(base.left.x  + RobotEye::SURPRISED_INSET_PX);
    f.right.x = (int16_t)(base.right.x - RobotEye::SURPRISED_INSET_PX);
    return f;
}

// sleepy：顶部斜线比sad更缓，Y不下移
FaceParams re_gen_sleepy(const FaceParams& base, int16_t, int16_t) {
    FaceParams f = base;
    uint8_t hi = (uint8_t)(RobotEye::BASE_EYE_H * RobotEye::SLEEPY_SLOPE_HIGH_RATIO);
    uint8_t lo = (uint8_t)(RobotEye::BASE_EYE_H * RobotEye::SLEEPY_SLOPE_LOW_RATIO);
    f.left.topSlopeL  = lo;  f.left.topSlopeR  = hi;
    f.right.topSlopeL = hi;  f.right.topSlopeR = lo;
    return f;
}

// excited：参数同happy，渲染时额外画椭圆遮挡
FaceParams re_gen_excited(const FaceParams& base, int16_t lcy, int16_t rcy) {
    return re_gen_happy(base, lcy, rcy);
}

// scared：angry镜像（内高外低）+上移+直角
FaceParams re_gen_scared(const FaceParams& base, int16_t, int16_t) {
    FaceParams f = base;
    f.left.y  = (int16_t)(base.left.y  - RobotEye::SCARED_UP_OFFSET);
    f.right.y = (int16_t)(base.right.y - RobotEye::SCARED_UP_OFFSET);
    f.left.r  = f.right.r = 0;
    f.left.topSlopeL  = RobotEye::SCARED_SLOPE_PX; f.left.topSlopeR  = 0;
    f.right.topSlopeL = 0;                         f.right.topSlopeR = RobotEye::SCARED_SLOPE_PX;
    return f;
}

// focused：高度×1/2，中心不变，圆角按比例缩
FaceParams re_gen_focused(const FaceParams& base, int16_t lcy, int16_t rcy) {
    FaceParams f = base;
    uint8_t h = (uint8_t)(RobotEye::BASE_EYE_H * RobotEye::FOCUSED_HEIGHT_RATIO);
    uint8_t r = (uint8_t)(RobotEye::BASE_EYE_R * RobotEye::FOCUSED_HEIGHT_RATIO);
    f.left.h = f.right.h = h;
    f.left.r = f.right.r = r;
    f.left.y  = (int16_t)(lcy - h / 2);
    f.right.y = (int16_t)(rcy - h / 2);
    return f;
}

// worried：scared斜线形状+居中（不上移）+圆角
FaceParams re_gen_worried(const FaceParams& base, int16_t, int16_t) {
    FaceParams f = base;
    f.left.r  = f.right.r = RobotEye::WORRIED_CORNER_RADIUS;
    f.left.topSlopeL  = RobotEye::SCARED_SLOPE_PX; f.left.topSlopeR  = 0;
    f.right.topSlopeL = 0;                         f.right.topSlopeR = RobotEye::SCARED_SLOPE_PX;
    return f;
}

// despair：scared形状（含上移）+y不变+h缩小（从顶部裁）+坡度绝对值不变
FaceParams re_gen_despair(const FaceParams& base, int16_t, int16_t) {
    FaceParams f = base;
    uint8_t h = (uint8_t)(RobotEye::BASE_EYE_H * RobotEye::DESPAIR_HEIGHT_RATIO);
    f.left.y  = (int16_t)(base.left.y  - RobotEye::SCARED_UP_OFFSET);
    f.right.y = (int16_t)(base.right.y - RobotEye::SCARED_UP_OFFSET);
    f.left.h  = f.right.h = h;
    f.left.r  = f.right.r = 0;
    f.left.topSlopeL  = RobotEye::SCARED_SLOPE_PX; f.left.topSlopeR  = 0;
    f.right.topSlopeL = 0;                         f.right.topSlopeR = RobotEye::SCARED_SLOPE_PX;
    return f;
}

// alert：宽度×1/4（变窄）+高度+4（上下对称扩展，中心不变）
FaceParams re_gen_alert(const FaceParams& base, int16_t lcy, int16_t rcy) {
    FaceParams f = base;
    uint8_t w = (uint8_t)(RobotEye::BASE_EYE_W * RobotEye::ALERT_WIDTH_RATIO);
    uint8_t h = (uint8_t)(RobotEye::BASE_EYE_H + RobotEye::ALERT_HEIGHT_ADD);
    uint8_t r = w / 2;
    f.left.w = f.right.w = w;
    f.left.h = f.right.h = h;
    f.left.r = f.right.r = r;
    f.left.x  = (int16_t)(base.left.x  + (RobotEye::BASE_EYE_W - w) / 2);
    f.right.x = (int16_t)(base.right.x + (RobotEye::BASE_EYE_W - w) / 2);
    f.left.y  = (int16_t)(lcy - h / 2);
    f.right.y = (int16_t)(rcy - h / 2);
    return f;
}

// blink：4px横线居中
FaceParams re_gen_blink(const FaceParams& base, int16_t lcy, int16_t rcy) {
    FaceParams f = base;
    f.left.h = f.right.h = 4;
    f.left.r = f.right.r = 2;
    f.left.y  = (int16_t)(lcy - 2);
    f.right.y = (int16_t)(rcy - 2);
    return f;
}

FaceParams re_gen_blink_down(const FaceParams& base, int16_t lcy, int16_t rcy) {
    FaceParams f = base;
    f.left.h = f.right.h = 4;
    f.left.r = f.right.r = 2;
    f.left.y  = (int16_t)(lcy - 2 + RobotEye::BLINK_DOWN_OFFSET);
    f.right.y = (int16_t)(rcy - 2 + RobotEye::BLINK_DOWN_OFFSET);
    return f;
}

FaceParams re_gen_blink_up(const FaceParams& base, int16_t lcy, int16_t rcy) {
    FaceParams f = base;
    f.left.h = f.right.h = 4;
    f.left.r = f.right.r = 2;
    f.left.y  = (int16_t)(lcy - 2 - RobotEye::BLINK_UP_OFFSET);
    f.right.y = (int16_t)(rcy - 2 - RobotEye::BLINK_UP_OFFSET);
    return f;
}

// disoriented：30×30大眼+两眼间距+10
FaceParams re_gen_disoriented(const FaceParams& base, int16_t lcy, int16_t rcy) {
    FaceParams f = base;
    uint8_t w = RobotEye::DISORIENTED_W, h = RobotEye::DISORIENTED_H;
    int16_t add = RobotEye::DISORIENTED_GAP_ADD / 2;
    f.left.w = f.right.w = w;
    f.left.h = f.right.h = h;
    f.left.r = f.right.r = (uint8_t)(w * 0.4f);
    f.left.x  = (int16_t)(base.left.x  - add - (w - RobotEye::BASE_EYE_W) / 2);
    f.right.x = (int16_t)(base.right.x + add + (w - RobotEye::BASE_EYE_W) / 2);
    f.left.y  = (int16_t)(lcy - h / 2);
    f.right.y = (int16_t)(rcy - h / 2);
    return f;
}

// furious：同angry逻辑，裁切更深（2/3）
FaceParams re_gen_furious(const FaceParams& base, int16_t, int16_t) {
    FaceParams f = base;
    uint8_t cutH  = (uint8_t)(RobotEye::BASE_EYE_H * (1.0f - RobotEye::FURIOUS_HEIGHT_RATIO));
    uint8_t slope = (uint8_t)(RobotEye::BASE_EYE_H * RobotEye::ANGRY_SLOPE_RATIO);
    f.left.topSlopeL  = cutH;        f.left.topSlopeR  = cutH + slope;
    f.right.topSlopeL = cutH + slope; f.right.topSlopeR = cutH;
    return f;
}

FaceParams re_gen_look_down(const FaceParams& base, int16_t, int16_t) {
    FaceParams f = base;
    f.left.y  = (int16_t)(base.left.y  + RobotEye::LOOK_DOWN_OFFSET);
    f.right.y = (int16_t)(base.right.y + RobotEye::LOOK_DOWN_OFFSET);
    return f;
}

FaceParams re_gen_look_up(const FaceParams& base, int16_t, int16_t) {
    FaceParams f = base;
    f.left.y  = (int16_t)(base.left.y  - RobotEye::LOOK_UP_OFFSET);
    f.right.y = (int16_t)(base.right.y - RobotEye::LOOK_UP_OFFSET);
    return f;
}

// look_left：左眼偏移+放大1.3倍，右眼仅偏移不放大
FaceParams re_gen_look_left(const FaceParams& base, int16_t lcy, int16_t) {
    FaceParams f = base;
    uint8_t w = (uint8_t)(RobotEye::BASE_EYE_W * RobotEye::GAZE_EYE_SCALE);
    uint8_t h = (uint8_t)(RobotEye::BASE_EYE_H * RobotEye::GAZE_EYE_SCALE);
    uint8_t r = (uint8_t)(RobotEye::BASE_EYE_R * RobotEye::GAZE_EYE_SCALE);
    f.left.x = (int16_t)(base.left.x - RobotEye::GAZE_EYE_OFFSET_PX - (w - RobotEye::BASE_EYE_W) / 2);
    f.left.y = (int16_t)(lcy - h / 2);
    f.left.w = w; f.left.h = h; f.left.r = r;
    f.right.x = (int16_t)(base.right.x - RobotEye::GAZE_EYE_OFFSET_PX);
    return f;
}

FaceParams re_gen_look_right(const FaceParams& base, int16_t, int16_t rcy) {
    FaceParams f = base;
    uint8_t w = (uint8_t)(RobotEye::BASE_EYE_W * RobotEye::GAZE_EYE_SCALE);
    uint8_t h = (uint8_t)(RobotEye::BASE_EYE_H * RobotEye::GAZE_EYE_SCALE);
    uint8_t r = (uint8_t)(RobotEye::BASE_EYE_R * RobotEye::GAZE_EYE_SCALE);
    f.right.x = (int16_t)(base.right.x + RobotEye::GAZE_EYE_OFFSET_PX - (w - RobotEye::BASE_EYE_W) / 2);
    f.right.y = (int16_t)(rcy - h / 2);
    f.right.w = w; f.right.h = h; f.right.r = r;
    f.left.x = (int16_t)(base.left.x + RobotEye::GAZE_EYE_OFFSET_PX);
    return f;
}

FaceParams re_gen_wink_left(const FaceParams& base, int16_t lcy, int16_t) {
    FaceParams f = base;
    f.left.h = 4; f.left.r = 2; f.left.y = (int16_t)(lcy - 2);
    return f;
}

FaceParams re_gen_wink_right(const FaceParams& base, int16_t, int16_t rcy) {
    FaceParams f = base;
    f.right.h = 4; f.right.r = 2; f.right.y = (int16_t)(rcy - 2);
    return f;
}

// bored：平弧偏下
FaceParams re_gen_bored(const FaceParams& base, int16_t, int16_t) {
    FaceParams f = base;
    f.left.y = f.right.y = (int16_t)(base.left.y + 20);
    f.left.h = f.right.h = 14;
    f.left.r = f.right.r = 7;
    f.left.topCut = f.right.topCut = 7;
    return f;
}

} // anonymous namespace


// ============================================================
//                      构造与初始化
// ============================================================

RobotEye::RobotEye(U8G2* u8g2) : _u8g2(u8g2) {
    _reset();
}

void RobotEye::_reset() {
    _bootPhase = DEBUG_SKIP_BOOT ? BOOT_IDLE : BOOT_CLOSE;
    _bootStartTime = millis();
    _bootBlinkCnt = 0;
    _lastBootBlinkEnd = 0;

    _blinkState = BLK_IDLE;
    _blinkHoldDuration = BLINK_HOLD_MIN_MS;
    _nextBlinkTime = 0;

    _halfBlinkState = HB_IDLE;
    _nextHalfBlinkTime = 0;

    _gazeState = GZ_IDLE;
    _gazeHoldEnd = 0;
    _nextGazeTime = 0;
    _gazeDirExpr = EXPR_NORMAL;

    _driftX = _driftY = 0;
    _driftTargetX = _driftTargetY = 0;
    _nextDriftTime = 0;

    _transState = TR_IDLE;
    _transFromExpr = EXPR_NORMAL;
    _transToExpr = EXPR_NORMAL;

    _currentExpr = EXPR_NORMAL;
    _batteryLevel = 80;

    _frameMode = FRAME_EYE;
    _frameExcitedMask = 0;
}

void RobotEye::begin(uint32_t seed) {
    // 1. 从 U8g2 读取实际屏幕尺寸（读不到用后备值）
    _screenW = _u8g2->getDisplayWidth();
    _screenH = _u8g2->getDisplayHeight();
    if (_screenW == 0) _screenW = SCREEN_W_DEFAULT;
    if (_screenH == 0) _screenH = SCREEN_H_DEFAULT;

    // 2. 根据屏幕尺寸自动居中，再叠加4方向微调
    //    dx = RIGHT - LEFT，dy = DOWN - UP
    int16_t dx = (int16_t)(OFFSET_RIGHT - OFFSET_LEFT);
    int16_t dy = (int16_t)(OFFSET_DOWN  - OFFSET_UP);
    uint16_t totalW = 2 * BASE_EYE_W + BASE_EYE_GAP;
    _baseLeftX  = (int16_t)((_screenW - totalW) / 2) + dx;
    _baseRightX = _baseLeftX + BASE_EYE_W + BASE_EYE_GAP;
    _baseY      = (int16_t)((_screenH - BASE_EYE_H) / 2) + dy;

    // 3. 注册所有内置表情生成函数
    _registerBuiltins();

    // 4. 预计算 excited 椭圆遮挡查找表
    //    椭圆方程 x = rx * sqrt(1 - y²/ry²)，只在初始化时算一次
    //    运行时查表，无 sqrtf
    for (int y = 0; y <= EXCITED_MASK_RY; y++) {
        float ratio = 1.0f - (float)(y * y) / (float)(EXCITED_MASK_RY * EXCITED_MASK_RY);
        if (ratio < 0) ratio = 0;
        _ellipseLut[y] = (uint8_t)((float)EXCITED_MASK_RX * sqrtf(ratio));
    }

    // 5. 设置随机种子
    if (seed != 0) {
        // 用户指定种子
        randomSeed(seed);
    } else {
        // 自动选择平台
#if defined(ESP32)
        randomSeed(esp_random());           // ESP32 硬件真随机数
#elif defined(ESP8266)
        randomSeed(ESP.getCycleCount());    // ESP8266 CPU周期计数器
#else
        // 通用后备：millis() 做种子，多跑几次增加离散度
        randomSeed((uint32_t)millis());
        for (volatile int i = 0; i < 16; i++) random();
#endif
    }

    _reset();
}

void RobotEye::_registerBuiltins() {
    // 先全部清空（用户槽位默认为 nullptr → 显示normal脸）
    for (uint8_t i = 0; i < EXPR_COUNT; i++) _exprGens[i] = nullptr;

    _exprGens[EXPR_NORMAL]      = re_gen_normal;
    _exprGens[EXPR_HAPPY]       = re_gen_happy;
    _exprGens[EXPR_ANGRY]       = re_gen_angry;
    _exprGens[EXPR_SAD]         = re_gen_sad;
    _exprGens[EXPR_SURPRISED]   = re_gen_surprised;
    _exprGens[EXPR_SLEEPY]      = re_gen_sleepy;
    _exprGens[EXPR_EXCITED]     = re_gen_excited;
    _exprGens[EXPR_SCARED]      = re_gen_scared;
    _exprGens[EXPR_FOCUSED]     = re_gen_focused;
    _exprGens[EXPR_WORRIED]     = re_gen_worried;
    _exprGens[EXPR_DESPAIR]     = re_gen_despair;
    _exprGens[EXPR_ALERT]       = re_gen_alert;
    _exprGens[EXPR_BLINK]       = re_gen_blink;
    _exprGens[EXPR_BLINK_DOWN]  = re_gen_blink_down;
    _exprGens[EXPR_BLINK_UP]    = re_gen_blink_up;
    _exprGens[EXPR_DISORIENTED] = re_gen_disoriented;
    _exprGens[EXPR_FURIOUS]     = re_gen_furious;
    _exprGens[EXPR_LOOK_DOWN]   = re_gen_look_down;
    _exprGens[EXPR_LOOK_UP]     = re_gen_look_up;
    _exprGens[EXPR_LOOK_LEFT]   = re_gen_look_left;
    _exprGens[EXPR_LOOK_RIGHT]  = re_gen_look_right;
    _exprGens[EXPR_WINK_LEFT]   = re_gen_wink_left;
    _exprGens[EXPR_WINK_RIGHT]  = re_gen_wink_right;
    _exprGens[EXPR_BORED]       = re_gen_bored;
}

// ============================================================
//                      公有接口
// ============================================================

void RobotEye::setExpression(Expression expr) {
    setExpression(expr, false);
}

void RobotEye::setExpression(Expression expr, bool useBlink) {
    if (expr >= EXPR_COUNT) return;
    if (expr == _currentExpr && _transState == TR_IDLE) return;

    // 任何外部切换都立即打断半眯眼（半眯眼不阻塞切换）
    _halfBlinkState = HB_IDLE;

    // 位图特殊表情直接硬切（位图与矢量参数无法插值）
    if (_isBitmapExpr(expr) || _isBitmapExpr(_currentExpr)) {
        _currentExpr = expr;
        _transState = TR_IDLE;
        _gazeState = GZ_IDLE;
        return;
    }

    _transToExpr = expr;
    _transFromExpr = _currentExpr;
    _gazeState = GZ_IDLE;

    if (useBlink) {
        _transState = TR_BLINK_SWITCH;
        _startBlink();
    } else {
        _transState = TR_MORPH;
        _transStartTime = millis();
    }
}

void RobotEye::setBatteryLevel(uint8_t percent) {
    _batteryLevel = percent > 100 ? 100 : percent;
}

void RobotEye::triggerBlink() {
    if (_blinkState == BLK_IDLE) _startBlink();
}

RobotEye::Expression RobotEye::getCurrentExpression() const {
    return _currentExpr;
}

bool RobotEye::isBusy() const {
    // 半眯眼不算忙（可被随时打断），只检查真正阻塞切换的状态
    return _transState != TR_IDLE || _gazeState != GZ_IDLE ||
           _blinkState != BLK_IDLE;
}

void RobotEye::setExprGenerator(Expression id, RE_ExprGenerator gen) {
    if (id < EXPR_COUNT) {
        _exprGens[id] = gen;
    }
}

// ============================================================
//                      主循环
// ============================================================
void RobotEye::update() {
    uint32_t now = millis();
    // 1. 推进所有状态机
    _updateBoot(now);
    _updateTransition(now);
    _updateGaze(now);
    _updateDrift(now);
    _updateHalfBlink(now);
    _updateBlink(now);
    // 2. 计算当前帧参数（只算一次）
    _computeFrame(now);
    // 3. 渲染到屏幕（全帧一次完成，或页缓冲多次绘制）
    _renderBuffer();
}

// ============================================================
//                   数学工具
// ============================================================
float RobotEye::_clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
float RobotEye::_lerp(float a, float b, float t) { return a + (b - a) * t; }

// 缓动函数：先加速后减速
float RobotEye::_easeInOutQuad(float t) {
    if (t < 0.5f) return 2.0f * t * t;
    float u = -2.0f * t + 2.0f;
    return 1.0f - u * u * 0.5f;
}

uint8_t RobotEye::_lerpU8(uint8_t a, uint8_t b, float t) {
    return (uint8_t)(a + (int)(b - a) * t);
}
int16_t RobotEye::_lerpI16(int16_t a, int16_t b, float t) {
    return (int16_t)(a + (int)(b - a) * t);
}
uint8_t RobotEye::_minU8(uint8_t a, uint8_t b) { return a < b ? a : b; }
uint8_t RobotEye::_maxU8(uint8_t a, uint8_t b) { return a > b ? a : b; }

// ============================================================
//                   参数插值
// ============================================================
EyeParams RobotEye::_lerpEye(const EyeParams& a, const EyeParams& b, float t) {
    EyeParams r;
    r.x = _lerpI16(a.x, b.x, t);
    r.y = _lerpI16(a.y, b.y, t);
    r.w = _lerpU8(a.w, b.w, t);
    r.h = _lerpU8(a.h, b.h, t);
    r.r = _lerpU8(a.r, b.r, t);
    r.topCut = _lerpU8(a.topCut, b.topCut, t);
    r.bottomCut = _lerpU8(a.bottomCut, b.bottomCut, t);
    r.topSlopeL = _lerpU8(a.topSlopeL, b.topSlopeL, t);
    r.topSlopeR = _lerpU8(a.topSlopeR, b.topSlopeR, t);
    r.bottomSlopeL = _lerpU8(a.bottomSlopeL, b.bottomSlopeL, t);
    r.bottomSlopeR = _lerpU8(a.bottomSlopeR, b.bottomSlopeR, t);
    return r;
}

FaceParams RobotEye::_lerpFace(const FaceParams& a, const FaceParams& b, float t) {
    FaceParams r;
    r.left  = _lerpEye(a.left,  b.left,  t);
    r.right = _lerpEye(a.right, b.right, t);
    return r;
}

// ============================================================
//  基准脸：动态居中坐标 + 随机漂移
// ============================================================
FaceParams RobotEye::_getBaseFace() {
    FaceParams f;
    int16_t dx = (int16_t)_driftX;
    int16_t dy = (int16_t)_driftY;
    f.left  = { (int16_t)(_baseLeftX  + dx), (int16_t)(_baseY + dy),
                BASE_EYE_W, BASE_EYE_H, BASE_EYE_R, 0,0,0,0,0,0 };
    f.right = { (int16_t)(_baseRightX + dx), (int16_t)(_baseY + dy),
                BASE_EYE_W, BASE_EYE_H, BASE_EYE_R, 0,0,0,0,0,0 };
    return f;
}

FaceParams RobotEye::_getHappyFace() {
    FaceParams base = _getBaseFace();
    int16_t lcy = base.left.y + base.left.h / 2;
    int16_t rcy = base.right.y + base.right.h / 2;
    return re_gen_happy(base, lcy, rcy);
}

// 查表调用表情生成函数
FaceParams RobotEye::_getExprFace(Expression id) {
    FaceParams base = _getBaseFace();
    int16_t lcy = base.left.y  + base.left.h  / 2;
    int16_t rcy = base.right.y + base.right.h / 2;
    if (id < EXPR_COUNT && _exprGens[id] != nullptr) {
        return _exprGens[id](base, lcy, rcy);
    }
    // 未注册的槽位（含用户槽位）返回 normal 基准脸
    return base;
}

// ============================================================
//  眨眼变换：高度压缩（中心Y不变）+ 左右膨胀
// ============================================================
FaceParams RobotEye::_applyBlink(const FaceParams& src, float amount) {
    FaceParams r = src;
    if (amount <= 0.001f) return r;

    float hScale = 1.0f - amount * (1.0f - BLINK_MIN_H_RATIO);
    float expand = amount * BLINK_EXPAND_PX;

    int16_t lcy = src.left.y + src.left.h / 2;
    r.left.h = (uint8_t)((float)src.left.h * hScale);
    if (r.left.h < 2) r.left.h = 2;
    r.left.y = lcy - r.left.h / 2;
    r.left.w = (uint8_t)((float)src.left.w + expand * 2.0f);
    r.left.x = src.left.x - (int16_t)expand;
    r.left.r = (uint8_t)((float)src.left.r * hScale);
    r.left.topCut = (uint8_t)((float)src.left.topCut * hScale);
    r.left.bottomCut = (uint8_t)((float)src.left.bottomCut * hScale);
    r.left.topSlopeL = (uint8_t)((float)src.left.topSlopeL * hScale);
    r.left.topSlopeR = (uint8_t)((float)src.left.topSlopeR * hScale);

    int16_t rcy = src.right.y + src.right.h / 2;
    r.right.h = (uint8_t)((float)src.right.h * hScale);
    if (r.right.h < 2) r.right.h = 2;
    r.right.y = rcy - r.right.h / 2;
    r.right.w = (uint8_t)((float)src.right.w + expand * 2.0f);
    r.right.x = src.right.x - (int16_t)expand;
    r.right.r = (uint8_t)((float)src.right.r * hScale);
    r.right.topCut = (uint8_t)((float)src.right.topCut * hScale);
    r.right.bottomCut = (uint8_t)((float)src.right.bottomCut * hScale);
    r.right.topSlopeL = (uint8_t)((float)src.right.topSlopeL * hScale);
    r.right.topSlopeR = (uint8_t)((float)src.right.topSlopeR * hScale);
    return r;
}

// ============================================================
//  半眯眼变换：仅高度微缩，不膨胀宽度
// ============================================================
FaceParams RobotEye::_applyHalfBlink(const FaceParams& src, float amount) {
    FaceParams r = src;
    if (amount <= 0.001f) return r;

    float hScale = 1.0f - amount * (1.0f - HALF_BLINK_HEIGHT_RATIO);

    int16_t lcy = src.left.y + src.left.h / 2;
    r.left.h = (uint8_t)((float)src.left.h * hScale);
    r.left.y = lcy - r.left.h / 2;
    r.left.r = (uint8_t)((float)src.left.r * hScale);
    r.left.topCut = (uint8_t)((float)src.left.topCut * hScale);
    r.left.bottomCut = (uint8_t)((float)src.left.bottomCut * hScale);
    r.left.topSlopeL = (uint8_t)((float)src.left.topSlopeL * hScale);
    r.left.topSlopeR = (uint8_t)((float)src.left.topSlopeR * hScale);

    int16_t rcy = src.right.y + src.right.h / 2;
    r.right.h = (uint8_t)((float)src.right.h * hScale);
    r.right.y = rcy - r.right.h / 2;
    r.right.r = (uint8_t)((float)src.right.r * hScale);
    r.right.topCut = (uint8_t)((float)src.right.topCut * hScale);
    r.right.bottomCut = (uint8_t)((float)src.right.bottomCut * hScale);
    r.right.topSlopeL = (uint8_t)((float)src.right.topSlopeL * hScale);
    r.right.topSlopeR = (uint8_t)((float)src.right.topSlopeR * hScale);
    return r;
}

// ============================================================
//  矢量渲染 - 单只眼睛
//
//  绘制顺序：白色圆角矩形 --- 顶部水平裁切 --- 底部水平裁切 --- 顶部斜线裁切
//
//  topSlopeL/R 语义：该侧从顶部向下的裁切深度
//    mn=min(L,R) 前mn行整行涂黑；mx=max(L,R) mn~mx行三角形
//    R>L → 三角形在右（左高右低）；L>R → 三角形在左（左低右高）
// ============================================================
void RobotEye::_drawEye(const EyeParams& p) {
    if (p.w == 0 || p.h == 0) return;

    uint8_t r = p.r;
    if (r > p.w / 2) r = p.w / 2;
    if (r > p.h / 2) r = p.h / 2;

    // 1. 白色圆角矩形（基础眼型）
    _u8g2->setDrawColor(COL_WHITE);
    _u8g2->drawRBox(p.x, p.y, p.w, p.h, r);

    // 2. 顶部水平裁切
    if (p.topCut > 0 && p.topCut < p.h) {
        _u8g2->setDrawColor(COL_BLACK);
        _u8g2->drawBox(p.x, p.y, p.w, p.topCut);
    }

    // 3. 底部水平裁切
    if (p.bottomCut > 0 && p.bottomCut < p.h) {
        _u8g2->setDrawColor(COL_BLACK);
        _u8g2->drawBox(p.x, p.y + p.h - p.bottomCut, p.w, p.bottomCut);
    }

    // 4. 顶部斜线裁切（逐行精确计算，区块与drawTriangle一致，边缘更直）
    if (p.topSlopeL > 0 || p.topSlopeR > 0) {
        uint8_t mn = _minU8(p.topSlopeL, p.topSlopeR);
        uint8_t mx = _maxU8(p.topSlopeL, p.topSlopeR);
        if (mn > p.h) mn = p.h;
        if (mx > p.h) mx = p.h;

        _u8g2->setDrawColor(COL_BLACK);

        // 4a. 共同高度部分整行涂黑
        if (mn > 0)
            _u8g2->drawBox(p.x, p.y, p.w, mn);

        // 4b. 三角形区域逐行精确绘制
        if (mx > mn) {
            uint8_t span = mx - mn;
            for (uint8_t dy = 0; dy < span; dy++) {
                uint8_t y = mn + dy;
                uint8_t blackW;
                if (p.topSlopeR > p.topSlopeL) {
                    // 三角形在右侧：dy=0全黑，逐行变窄
                    blackW = (uint8_t)(p.w - (uint32_t)p.w * dy / span);
                    if (blackW > 0)
                        _u8g2->drawBox(p.x + p.w - blackW, p.y + y, blackW, 1);
                } else {
                    // 三角形在左侧：dy=0全黑，逐行变窄
                    blackW = (uint8_t)((uint32_t)p.w * (span - dy) / span);
                    if (blackW > 0)
                        _u8g2->drawBox(p.x, p.y + y, blackW, 1);
                }
            }
        }
    }

    _u8g2->setDrawColor(COL_WHITE);
}

void RobotEye::_drawFace(const FaceParams& face) {
    _drawEye(face.left);
    _drawEye(face.right);
}

// ============================================================
//  填充椭圆（excited黑色遮挡用）
//  使用预计算查找表 _ellipseLut，运行时只有整数乘除
//
//  参数：cx/cy=中心，rx/ry=当前椭圆半径（随morph变化），
//        mask=0~1 椭圆大小比例
//  原理：查表得到全尺寸半宽，再按mask等比缩放
//        （等比缩放不改变椭圆形状）
// ============================================================
void RobotEye::_fillEllipse(int cx, int cy, int rx, int ry, float mask) {
    if (rx <= 0 || ry <= 0 || mask <= 0.01f) return;
    _u8g2->setDrawColor(COL_BLACK);
    for (int y = -ry; y <= ry; y++) {
        // 将当前y映射到查找表索引（0~EXCITED_MASK_RY）
        int ay = y < 0 ? -y : y;
        int lutY = (int)((float)ay * EXCITED_MASK_RY / ry);
        if (lutY > EXCITED_MASK_RY) lutY = EXCITED_MASK_RY;
        // 查表得全尺寸半宽，按mask缩放
        int x = (int)((float)_ellipseLut[lutY] * mask);
        if (x > 0)
            _u8g2->drawBox(cx - x, cy + y, (uint8_t)(x * 2), 1);
    }
    _u8g2->setDrawColor(COL_WHITE);
}

// ============================================================
//  电池矢量绘制（分段格，居中自适应屏幕）
// ============================================================
void RobotEye::_drawBattery(uint8_t level) {
    int x = (_screenW - BATTERY_W) / 2;
    int y = (_screenH - BATTERY_H) / 2;
    int bodyW = BATTERY_W - BATTERY_TERM_W;

    _u8g2->setDrawColor(COL_WHITE);
    _u8g2->drawRFrame(x, y, bodyW, BATTERY_H, 2);
    _u8g2->drawBox(x + bodyW, y + (BATTERY_H - BATTERY_TERM_H) / 2,
                   BATTERY_TERM_W, BATTERY_TERM_H);

    // 四舍五入计算点亮格数
    uint8_t lit = (uint8_t)((level * BATTERY_GRID_COUNT + 99) / 100);
    if (lit > BATTERY_GRID_COUNT) lit = BATTERY_GRID_COUNT;

    int gridAreaW = bodyW - 6;
    int gridW = gridAreaW / BATTERY_GRID_COUNT;
    for (uint8_t i = 0; i < lit; i++) {
        _u8g2->drawBox(x + 3 + i * gridW, y + 3, gridW - 1, BATTERY_H - 6);
    }
}

// ============================================================
//  位图表情未启用时的占位文字
// ============================================================
void RobotEye::_drawNotShowing() {
    _u8g2->setFont(u8g2_font_6x10_tr);
    _u8g2->setDrawColor(COL_WHITE);
    const char* msg = "not showing";
    int16_t w = _u8g2->getStrWidth(msg);
    _u8g2->drawStr((_screenW - w) / 2, _screenH / 2, msg);
}

#ifdef RE_ENABLE_BITMAP_EXPR
// ============================================================
//  逐像素解析 Irisoled 位图（逐字节优化版）
//
//  格式：128×64 横排XBM，每行16字节，MSB在左，PROGMEM存储
//  U8g2 的 drawXBM 位序不兼容，所以手动解析
//
//  全黑字节跳过、全白字节批量drawBox(8px)、混合才逐像素
// ============================================================
void RobotEye::_drawBitmapIrisoled(const uint8_t* bitmap) {
    _u8g2->setDrawColor(COL_WHITE);
    for (int y = 0; y < BITMAP_H; y++) {
        for (int byteIdx = 0; byteIdx < BITMAP_W / 8; byteIdx++) {
            uint8_t byteVal = RE_PGM_READ_BYTE(&bitmap[y * (BITMAP_W / 8) + byteIdx]);
            if (byteVal == 0x00) continue;       // 全黑跳过
            int x = byteIdx * 8;
            if (byteVal == 0xFF) {
                _u8g2->drawBox(x, y, 8, 1);     // 全白批量画8像素
            } else {
                for (int bit = 0; bit < 8; bit++) {
                    if (byteVal & (0x80 >> bit))
                        _u8g2->drawPixel(x + bit, y);
                }
            }
        }
    }
}
#endif

bool RobotEye::_isSpecialExpr(Expression expr) const {
    return expr == EXPR_BATTERY || expr == EXPR_WARNING ||
           expr == EXPR_LEFT_SIGNAL || expr == EXPR_RIGHT_SIGNAL || expr == EXPR_MODE;
}

bool RobotEye::_isBitmapExpr(Expression expr) const {
    return expr == EXPR_WARNING || expr == EXPR_LEFT_SIGNAL ||
           expr == EXPR_RIGHT_SIGNAL || expr == EXPR_MODE;
}

// ============================================================
//                   开机动画状态机
// ============================================================
void RobotEye::_updateBoot(uint32_t now) {
    if (DEBUG_SKIP_BOOT) return;
    switch (_bootPhase) {
    case BOOT_CLOSE:
        if (now - _bootStartTime > BOOT_CLOSE_MS) {
            _bootPhase = BOOT_TO_HALF;
            _bootStartTime = now;
        }
        break;
    case BOOT_TO_HALF:
        if (now - _bootStartTime >= BOOT_TO_HALF_MS) {
            _bootPhase = BOOT_HALF_BLINK;
            _bootBlinkCnt = 0;
            _lastBootBlinkEnd = now;
        }
        break;
    case BOOT_HALF_BLINK:
        if (_bootBlinkCnt >= BOOT_HALF_BLINK_CNT && _blinkState == BLK_IDLE) {
            _bootPhase = BOOT_TO_FULL;
            _bootStartTime = now;
        }
        break;
    case BOOT_TO_FULL:
        if (now - _bootStartTime >= BOOT_TO_FULL_MS) {
            _bootPhase = BOOT_IDLE;
            _nextBlinkTime = now + _randBlinkInterval();
            _nextGazeTime = now + random(GAZE_INTERVAL_MIN, GAZE_INTERVAL_MAX);
            _nextHalfBlinkTime = now + random(HALF_BLINK_INTERVAL_MIN, HALF_BLINK_INTERVAL_MAX);
            _nextDriftTime = now + DRIFT_INTERVAL_MS;
        }
        break;
    case BOOT_IDLE:
        break;
    }
}

// ============================================================
//                   眨眼状态机
// ============================================================
void RobotEye::_updateBlink(uint32_t now) {
    if (_bootPhase == BOOT_TO_HALF || _bootPhase == BOOT_TO_FULL) return;

    switch (_blinkState) {
    case BLK_IDLE: {
        bool shouldBlink = false;
        if (_bootPhase == BOOT_HALF_BLINK) {
            if (_bootBlinkCnt < BOOT_HALF_BLINK_CNT &&
                now - _lastBootBlinkEnd >= BOOT_HALF_BLINK_GAP)
                shouldBlink = true;
        } else if (_bootPhase == BOOT_IDLE) {
            if (now >= _nextBlinkTime) shouldBlink = true;
        }
        if (shouldBlink) {
            _startBlink();
            if (_bootPhase == BOOT_HALF_BLINK)
                _bootBlinkCnt++;
            else
                _nextBlinkTime = now + _randBlinkInterval();
        }
        break;
    }
    case BLK_CLOSING:
        if (now - _blinkStartTime >= BLINK_CLOSE_MS) {
            _blinkState = BLK_HOLD;
            _blinkStartTime = now;
            _blinkHoldDuration = random(BLINK_HOLD_MIN_MS, BLINK_HOLD_MAX_MS + 1);
        }
        break;
    case BLK_HOLD:
        if (now - _blinkStartTime >= _blinkHoldDuration) {
            _blinkState = BLK_OPENING;
            _blinkStartTime = now;
        }
        break;
    case BLK_OPENING:
        if (now - _blinkStartTime >= BLINK_OPEN_MS) {
            _blinkState = BLK_IDLE;
            if (_transState == TR_BLINK_SWITCH) {
                _currentExpr = _transToExpr;
                _transState = TR_IDLE;
            }
            if (_bootPhase == BOOT_HALF_BLINK)
                _lastBootBlinkEnd = now;
        }
        break;
    }
}

void RobotEye::_startBlink() {
    _blinkState = BLK_CLOSING;
    _blinkStartTime = millis();
}

uint32_t RobotEye::_randBlinkInterval() {
    if (DEBUG_FIXED_BLINK) return 5000;
    switch (_currentExpr) {
    case EXPR_NORMAL: case EXPR_SURPRISED: return random(7000, 12001);
    case EXPR_ANGRY:  case EXPR_HAPPY:     return random(2000, 6001);
    default: return random(BLINK_INTERVAL_MIN, BLINK_INTERVAL_MAX + 1);
    }
}

// ============================================================
//                   半眯眼状态机
// ============================================================
void RobotEye::_updateHalfBlink(uint32_t now) {
    if (DEBUG_DISABLE_HALF_BLINK) return;
    if (_bootPhase != BOOT_IDLE) return;
    if (_isSpecialExpr(_currentExpr)) return;
    if (_blinkState != BLK_IDLE) return;
    if (_transState != TR_IDLE) return;

    switch (_halfBlinkState) {
    case HB_IDLE:
        if (now >= _nextHalfBlinkTime) {
            if (random(0, 100) < HALF_BLINK_CHANCE) {
                _halfBlinkState = HB_SHRINKING;
                _halfBlinkStartTime = now;
            }
            _nextHalfBlinkTime = now + random(HALF_BLINK_INTERVAL_MIN, HALF_BLINK_INTERVAL_MAX);
        }
        break;
    case HB_SHRINKING:
        if (now - _halfBlinkStartTime >= HALF_BLINK_PHASE_MS) {
            _halfBlinkState = HB_EXPANDING;
            _halfBlinkStartTime = now;
        }
        break;
    case HB_EXPANDING:
        if (now - _halfBlinkStartTime >= HALF_BLINK_PHASE_MS)
            _halfBlinkState = HB_IDLE;
        break;
    }
}

// ============================================================
//                   瞟眼状态机
// ============================================================
void RobotEye::_updateGaze(uint32_t now) {
    if (DEBUG_DISABLE_GAZE) return;
    if (_bootPhase != BOOT_IDLE) return;
    if (_currentExpr != EXPR_NORMAL) return;
    if (_transState != TR_IDLE) return;

    switch (_gazeState) {
    case GZ_IDLE:
        if (_blinkState != BLK_IDLE) break;
        if (now < _nextGazeTime) break;
        if (random(0, 100) < GAZE_CHANCE) {
            int dir = random(0, 4);
            switch (dir) {
            case 0: _gazeDirExpr = EXPR_LOOK_LEFT;  break;
            case 1: _gazeDirExpr = EXPR_LOOK_RIGHT; break;
            case 2: _gazeDirExpr = EXPR_LOOK_UP;    break;
            default: _gazeDirExpr = EXPR_LOOK_DOWN; break;
            }
            _gazeState = GZ_TO_DIR;
            _gazeStartTime = now;
        }
        _nextGazeTime = now + random(GAZE_INTERVAL_MIN, GAZE_INTERVAL_MAX);
        break;
    case GZ_TO_DIR:
        if (now - _gazeStartTime >= GAZE_TRANS_MS) {
            _gazeState = GZ_HOLDING;
            _gazeHoldEnd = now + random(GAZE_HOLD_MIN_MS, GAZE_HOLD_MAX_MS + 1);
        }
        break;
    case GZ_HOLDING:
        if (now >= _gazeHoldEnd) {
            _gazeState = GZ_TO_CENTER;
            _gazeStartTime = now;
        }
        break;
    case GZ_TO_CENTER:
        if (now - _gazeStartTime >= GAZE_TRANS_MS)
            _gazeState = GZ_IDLE;
        break;
    }
}

// ============================================================
//                   随机漂移
// ============================================================
void RobotEye::_updateDrift(uint32_t now) {
    if (DEBUG_DISABLE_DRIFT) return;
    if (_bootPhase != BOOT_IDLE) return;
    if (_currentExpr != EXPR_NORMAL) return;

    if (now >= _nextDriftTime) {
        _driftTargetX = (float)random(-DRIFT_AMPLITUDE, DRIFT_AMPLITUDE + 1);
        _driftTargetY = (float)random(-DRIFT_AMPLITUDE, DRIFT_AMPLITUDE + 1);
        // 生成时直接限制在绝对边界内（非实时碰撞检测）
        if (_driftTargetX < -DRIFT_MAX_X) _driftTargetX = -DRIFT_MAX_X;
        if (_driftTargetX >  DRIFT_MAX_X) _driftTargetX =  DRIFT_MAX_X;
        if (_driftTargetY < -DRIFT_MAX_Y) _driftTargetY = -DRIFT_MAX_Y;
        if (_driftTargetY >  DRIFT_MAX_Y) _driftTargetY =  DRIFT_MAX_Y;
        _nextDriftTime = now + DRIFT_INTERVAL_MS;
    }
    _driftX += (_driftTargetX - _driftX) * DRIFT_SPEED;
    _driftY += (_driftTargetY - _driftY) * DRIFT_SPEED;
}

// ============================================================
//                   表情过渡
// ============================================================
void RobotEye::_updateTransition(uint32_t now) {
    if (_transState != TR_MORPH) return;
    if (now - _transStartTime >= MORPH_TRANS_MS) {
        _currentExpr = _transToExpr;
        _transState = TR_IDLE;
    }
}

// ============================================================
//                   计算层：计算当前帧参数
//  每帧只执行一次。结果存入 _frameMode/_frameFace/_frameExcitedMask，
//  供绘制层读取。页缓冲模式下绘制层执行多次但不会重复计算。
// ============================================================
void RobotEye::_computeFrame(uint32_t now) {
    // --- 特殊表情：位图 ---
    if (_isBitmapExpr(_currentExpr)) {
#ifdef RE_ENABLE_BITMAP_EXPR
        _frameMode = FRAME_BITMAP;
#else
        _frameMode = FRAME_NOT_SHOW;
#endif
        return;
    }

    // --- 特殊表情：电池 ---
    if (_currentExpr == EXPR_BATTERY) {
        _frameMode = FRAME_BATTERY;
        return;
    }

    // --- 矢量眼睛表情 ---
    _frameMode = FRAME_EYE;
    _frameExcitedMask = 0.0f;

    if (_transState == TR_MORPH) {
        // morph 过渡：所有参数线性插值
        float t = _clamp01((float)(now - _transStartTime) / MORPH_TRANS_MS);
        t = _easeInOutQuad(t);
        if (_transToExpr == EXPR_EXCITED) {
            _frameFace = _lerpFace(_getExprFace(_transFromExpr), _getHappyFace(), t);
            _frameExcitedMask = t;
        } else if (_transFromExpr == EXPR_EXCITED) {
            _frameFace = _lerpFace(_getHappyFace(), _getExprFace(_transToExpr), t);
            _frameExcitedMask = 1.0f - t;
        } else {
            _frameFace = _lerpFace(_getExprFace(_transFromExpr), _getExprFace(_transToExpr), t);
        }
    }
    else if (_transState == TR_BLINK_SWITCH && _blinkState == BLK_OPENING) {
        // 眨眼切换的睁眼阶段：新表情随睁眼渐变显现
        float t = _clamp01((float)(now - _blinkStartTime) / BLINK_OPEN_MS);
        t = _easeInOutQuad(t);
        if (_transToExpr == EXPR_EXCITED) {
            _frameFace = _lerpFace(_getExprFace(_transFromExpr), _getHappyFace(), t);
            _frameExcitedMask = t;
        } else if (_transFromExpr == EXPR_EXCITED) {
            _frameFace = _lerpFace(_getHappyFace(), _getExprFace(_transToExpr), t);
            _frameExcitedMask = 1.0f - t;
        } else {
            _frameFace = _lerpFace(_getExprFace(_transFromExpr), _getExprFace(_transToExpr), t);
        }
    }
    else if (_gazeState == GZ_TO_DIR) {
        float t = _clamp01((float)(now - _gazeStartTime) / GAZE_TRANS_MS);
        t = _easeInOutQuad(t);
        _frameFace = _lerpFace(_getExprFace(EXPR_NORMAL), _getExprFace(_gazeDirExpr), t);
    }
    else if (_gazeState == GZ_HOLDING) {
        _frameFace = _getExprFace(_gazeDirExpr);
    }
    else if (_gazeState == GZ_TO_CENTER) {
        float t = _clamp01((float)(now - _gazeStartTime) / GAZE_TRANS_MS);
        t = _easeInOutQuad(t);
        _frameFace = _lerpFace(_getExprFace(_gazeDirExpr), _getExprFace(EXPR_NORMAL), t);
    }
    // --- 开机动画各阶段 ---
    else if (_bootPhase == BOOT_CLOSE) {
        _frameFace.left  = { _baseLeftX,  (int16_t)(_baseY + 15), BASE_EYE_W, 2, 1, 0,0,0,0,0,0 };
        _frameFace.right = { _baseRightX, (int16_t)(_baseY + 15), BASE_EYE_W, 2, 1, 0,0,0,0,0,0 };
    }
    else if (_bootPhase == BOOT_TO_HALF) {
        float t = _clamp01((float)(now - _bootStartTime) / BOOT_TO_HALF_MS);
        t = _easeInOutQuad(t);
        FaceParams closed, half;
        closed.left  = { _baseLeftX,  (int16_t)(_baseY + 15), BASE_EYE_W, 2, 1, 0,0,0,0,0,0 };
        closed.right = { _baseRightX, (int16_t)(_baseY + 15), BASE_EYE_W, 2, 1, 0,0,0,0,0,0 };
        uint8_t hh = (uint8_t)(BASE_EYE_H * BOOT_HALF_OPEN_RATIO);
        half.left  = { _baseLeftX,  (int16_t)(_baseY + (BASE_EYE_H - hh)/2),
                       BASE_EYE_W, hh, (uint8_t)(BASE_EYE_R*BOOT_HALF_OPEN_RATIO), 0,0,0,0,0,0 };
        half.right = { _baseRightX, (int16_t)(_baseY + (BASE_EYE_H - hh)/2),
                       BASE_EYE_W, hh, (uint8_t)(BASE_EYE_R*BOOT_HALF_OPEN_RATIO), 0,0,0,0,0,0 };
        _frameFace = _lerpFace(closed, half, t);
    }
    else if (_bootPhase == BOOT_HALF_BLINK) {
        uint8_t hh = (uint8_t)(BASE_EYE_H * BOOT_HALF_OPEN_RATIO);
        _frameFace.left  = { _baseLeftX,  (int16_t)(_baseY + (BASE_EYE_H - hh)/2),
                       BASE_EYE_W, hh, (uint8_t)(BASE_EYE_R*BOOT_HALF_OPEN_RATIO), 0,0,0,0,0,0 };
        _frameFace.right = { _baseRightX, (int16_t)(_baseY + (BASE_EYE_H - hh)/2),
                       BASE_EYE_W, hh, (uint8_t)(BASE_EYE_R*BOOT_HALF_OPEN_RATIO), 0,0,0,0,0,0 };
    }
    else if (_bootPhase == BOOT_TO_FULL) {
        float t = _clamp01((float)(now - _bootStartTime) / BOOT_TO_FULL_MS);
        t = _easeInOutQuad(t);
        uint8_t hh = (uint8_t)(BASE_EYE_H * BOOT_HALF_OPEN_RATIO);
        FaceParams half, full;
        half.left  = { _baseLeftX,  (int16_t)(_baseY + (BASE_EYE_H - hh)/2),
                       BASE_EYE_W, hh, (uint8_t)(BASE_EYE_R*BOOT_HALF_OPEN_RATIO), 0,0,0,0,0,0 };
        half.right = { _baseRightX, (int16_t)(_baseY + (BASE_EYE_H - hh)/2),
                       BASE_EYE_W, hh, (uint8_t)(BASE_EYE_R*BOOT_HALF_OPEN_RATIO), 0,0,0,0,0,0 };
        full = _getBaseFace();
        _frameFace = _lerpFace(half, full, t);
    }
    else {
        // 稳定状态
        _frameFace = _getExprFace(_currentExpr);
        if (_currentExpr == EXPR_EXCITED) _frameExcitedMask = 1.0f;
    }

    // --- 叠加眨眼变换 ---
    float blinkAmount = 0;
    if (_blinkState == BLK_CLOSING) {
        float t = _clamp01((float)(now - _blinkStartTime) / BLINK_CLOSE_MS);
        blinkAmount = _easeInOutQuad(t);
    } else if (_blinkState == BLK_HOLD) {
        blinkAmount = 1.0f;
    } else if (_blinkState == BLK_OPENING) {
        float t = _clamp01((float)(now - _blinkStartTime) / BLINK_OPEN_MS);
        blinkAmount = 1.0f - _easeInOutQuad(t);
    }
    _frameFace = _applyBlink(_frameFace, blinkAmount);

    // --- 叠加半眯眼变换 ---
    float halfBlinkAmount = 0;
    if (_halfBlinkState == HB_SHRINKING) {
        float t = _clamp01((float)(now - _halfBlinkStartTime) / HALF_BLINK_PHASE_MS);
        halfBlinkAmount = _easeInOutQuad(t);
    } else if (_halfBlinkState == HB_EXPANDING) {
        float t = _clamp01((float)(now - _halfBlinkStartTime) / HALF_BLINK_PHASE_MS);
        halfBlinkAmount = 1.0f - _easeInOutQuad(t);
    }
    _frameFace = _applyHalfBlink(_frameFace, halfBlinkAmount);
}

// ============================================================
//  渲染缓冲：根据编译配置选择全帧或页缓冲模式
// ============================================================
void RobotEye::_renderBuffer() {
#ifdef RE_PAGE_BUFFER
    // 页缓冲模式：同一帧绘制多次（每次1页），省RAM
    _u8g2->firstPage();
    do {
        _drawFrame();
    } while (_u8g2->nextPage());
#else
    // 全帧缓冲模式：一次清屏+绘制+发送
    _u8g2->clearBuffer();
    _drawFrame();
    _u8g2->sendBuffer();
#endif
}

// ============================================================
//  绘制层：纯绘制，无副作用，只读 _frame* 成员变量
//  页缓冲模式下此函数执行多次，不得修改任何状态
// ============================================================
void RobotEye::_drawFrame() {
    switch (_frameMode) {
    case FRAME_BITMAP:
#ifdef RE_ENABLE_BITMAP_EXPR
        switch (_currentExpr) {
        case EXPR_WARNING:      _drawBitmapIrisoled(Irisoled::warning); break;
        case EXPR_LEFT_SIGNAL:  _drawBitmapIrisoled(Irisoled::left_signal); break;
        case EXPR_RIGHT_SIGNAL: _drawBitmapIrisoled(Irisoled::right_signal); break;
        case EXPR_MODE:         _drawBitmapIrisoled(Irisoled::mode); break;
        default: break;
        }
#endif
        break;

    case FRAME_BATTERY:
        _drawBattery(_batteryLevel);
        break;

    case FRAME_NOT_SHOW:
        _drawNotShowing();
        break;

    case FRAME_EYE:
        _drawFace(_frameFace);

        // excited 椭圆遮挡（查表，无sqrtf）
        if (_frameExcitedMask > 0.01f) {
            int rx = (int)((float)EXCITED_MASK_RX * _frameExcitedMask);
            int ry = (int)((float)EXCITED_MASK_RY * _frameExcitedMask);
            if (rx > 0 && ry > 0) {
                int lcx = _frameFace.left.x + _frameFace.left.w / 2;
                int lcy = _frameFace.left.y + _frameFace.left.h / 2;
                int rcx = _frameFace.right.x + _frameFace.right.w / 2;
                int rcy = _frameFace.right.y + _frameFace.right.h / 2;
                _fillEllipse(lcx, lcy, rx, ry, _frameExcitedMask);
                _fillEllipse(rcx, rcy, rx, ry, _frameExcitedMask);
            }
        }

        // 调试网格
        if (DEBUG_SHOW_GRID) {
            _u8g2->setDrawColor(COL_WHITE);
            for (int x = 0; x < _screenW; x += 16)
                _u8g2->drawLine(x, 0, x, _screenH - 1);
            for (int y = 0; y < _screenH; y += 16)
                _u8g2->drawLine(0, y, _screenW - 1, y);
        }
        break;
    }
}
