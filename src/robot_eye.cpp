#include "robot_eye.hpp"
#ifdef RE_ENABLE_BITMAP_EXPR
  #include "Irisoled.h"
#endif
#include <math.h>
// ============================================================
//  Platform Compatibility: PROGMEM Byte Read
// ============================================================
#if defined(__AVR__)
  #include <avr/pgmspace.h>
  #define RE_PGM_READ_BYTE(addr) pgm_read_byte(addr)
#else
  #define RE_PGM_READ_BYTE(addr) (*(const uint8_t*)(addr))
#endif
// ============================================================
//  Drawing Color (U8g2: 1 = white draw, 0 = black erase)
//  Internal use only, not exposed in header
// ============================================================
namespace {
    constexpr uint8_t COL_WHITE = 1;
    constexpr uint8_t COL_BLACK = 0;
}
// ============================================================
//
//          Built-in Expression Generator Functions (can be replaced by setExprGenerator)
//  Each function receives: base=base face (with drift/offset), lcy/rcy=center Y of left/right eye
//  Returns FaceParams for this expression. All parameters reference RobotEye:: constants
//
// ============================================================
namespace {
// normal: base face without transformation
FaceParams re_gen_normal(const FaceParams& base, int16_t, int16_t) {
    return base;
}
// happy: bottom cut to form upward curved smiling eyes
FaceParams re_gen_happy(const FaceParams& base, int16_t, int16_t) {
    FaceParams f = base;
    uint8_t cut = (uint8_t)(RobotEye::BASE_EYE_H * RobotEye::HAPPY_BOTTOM_CUT_RATIO);
    f.left.bottomCut = f.right.bottomCut = cut;
    return f;
}
// angry: keep full normal size + native bottom rounded corners, only cut from top
// Left eye: higher left, lower right; Right eye: higher right, lower left
FaceParams re_gen_angry(const FaceParams& base, int16_t, int16_t) {
    FaceParams f = base;
    uint8_t cutH  = (uint8_t)(RobotEye::BASE_EYE_H * (1.0f - RobotEye::ANGRY_HEIGHT_RATIO));
    uint8_t slope = (uint8_t)(RobotEye::BASE_EYE_H * RobotEye::ANGRY_SLOPE_RATIO);
    f.left.topSlopeL  = cutH;        f.left.topSlopeR  = cutH + slope;
    f.right.topSlopeL = cutH + slope; f.right.topSlopeR = cutH;
    return f;
}
// sad: top slant line (outer low, inner high for eyebrow shape) + overall downward shift
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
// surprised: two eyes move toward the center
FaceParams re_gen_surprised(const FaceParams& base, int16_t, int16_t) {
    FaceParams f = base;
    f.left.x  = (int16_t)(base.left.x  + RobotEye::SURPRISED_INSET_PX);
    f.right.x = (int16_t)(base.right.x - RobotEye::SURPRISED_INSET_PX);
    return f;
}
// sleepy: top slope gentler than sad, Y position unchanged
FaceParams re_gen_sleepy(const FaceParams& base, int16_t, int16_t) {
    FaceParams f = base;
    uint8_t hi = (uint8_t)(RobotEye::BASE_EYE_H * RobotEye::SLEEPY_SLOPE_HIGH_RATIO);
    uint8_t lo = (uint8_t)(RobotEye::BASE_EYE_H * RobotEye::SLEEPY_SLOPE_LOW_RATIO);
    f.left.topSlopeL  = lo;  f.left.topSlopeR  = hi;
    f.right.topSlopeL = hi;  f.right.topSlopeR = lo;
    return f;
}
// excited: same parameters as happy, extra ellipse mask drawn during rendering
FaceParams re_gen_excited(const FaceParams& base, int16_t lcy, int16_t rcy) {
    return re_gen_happy(base, lcy, rcy);
}
// scared: mirrored angry shape (inner high, outer low) + upward shift + sharp right angle
FaceParams re_gen_scared(const FaceParams& base, int16_t, int16_t) {
    FaceParams f = base;
    f.left.y  = (int16_t)(base.left.y  - RobotEye::SCARED_UP_OFFSET);
    f.right.y = (int16_t)(base.right.y - RobotEye::SCARED_UP_OFFSET);
    f.left.r  = f.right.r = 0;
    f.left.topSlopeL  = RobotEye::SCARED_SLOPE_PX; f.left.topSlopeR  = 0;
    f.right.topSlopeL = 0;                         f.right.topSlopeR = RobotEye::SCARED_SLOPE_PX;
    return f;
}
// focused: height × 1/2, center unchanged, radius scaled proportionally
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
// worried: scared slope shape + centered (no upward shift) + rounded corners
FaceParams re_gen_worried(const FaceParams& base, int16_t, int16_t) {
    FaceParams f = base;
    f.left.r  = f.right.r = RobotEye::WORRIED_CORNER_RADIUS;
    f.left.topSlopeL  = RobotEye::SCARED_SLOPE_PX; f.left.topSlopeR  = 0;
    f.right.topSlopeL = 0;                         f.right.topSlopeR = RobotEye::SCARED_SLOPE_PX;
    return f;
}
// despair: scared shape (including upward shift) + fixed Y + reduced height (cut from top), absolute slope unchanged
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
// alert: width × 1/4 (narrower) + height +4 (symmetric expand up/down, center unchanged)
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
// blink: 4px horizontal line centered
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
// disoriented: 30×30 large eyes + eye gap +10
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
// furious: same logic as angry, deeper cut (2/3)
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
// look_left: left eye offset + scaled 1.3x, right eye offset only without scaling
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
// bored: flat arc shifted downward
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
//                      Constructor & Initialization
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
    // 1. Read actual screen size from U8g2 (use fallback if unavailable)
    _screenW = _u8g2->getDisplayWidth();
    _screenH = _u8g2->getDisplayHeight();
    if (_screenW == 0) _screenW = SCREEN_W_DEFAULT;
    if (_screenH == 0) _screenH = SCREEN_H_DEFAULT;
    // 2. Auto center based on screen size, then apply 4-direction fine offset
    //    dx = RIGHT - LEFT，dy = DOWN - UP
    int16_t dx = (int16_t)(OFFSET_RIGHT - OFFSET_LEFT);
    int16_t dy = (int16_t)(OFFSET_DOWN  - OFFSET_UP);
    uint16_t totalW = 2 * BASE_EYE_W + BASE_EYE_GAP;
    _baseLeftX  = (int16_t)((_screenW - totalW) / 2) + dx;
    _baseRightX = _baseLeftX + BASE_EYE_W + BASE_EYE_GAP;
    _baseY      = (int16_t)((_screenH - BASE_EYE_H) / 2) + dy;
    // 3. Register all built-in expression generators
    _registerBuiltins();
    // 4. Precompute lookup table for excited ellipse mask
    //    Ellipse equation x = rx * sqrt(1 - y²/ry²), calculated once on init
    //    Lookup table used at runtime, no sqrtf
    for (int y = 0; y <= EXCITED_MASK_RY; y++) {
        float ratio = 1.0f - (float)(y * y) / (float)(EXCITED_MASK_RY * EXCITED_MASK_RY);
        if (ratio < 0) ratio = 0;
        _ellipseLut[y] = (uint8_t)((float)EXCITED_MASK_RX * sqrtf(ratio));
    }
    // 5. Set random seed
    if (seed != 0) {
        // User specified seed
        randomSeed(seed);
    } else {
        // Auto select platform
#if defined(ESP32)
        randomSeed(esp_random());           // ESP32 hardware true random
#elif defined(ESP8266)
        randomSeed(ESP.getCycleCount());    // ESP8266 CPU cycle counter
#else
        // General fallback: millis() as seed, extra random runs for better distribution
        randomSeed((uint32_t)millis());
        for (volatile int i = 0; i < 16; i++) random();
#endif
    }
    _reset();
}
void RobotEye::_registerBuiltins() {
    // Clear all first (user slots default to nullptr → render normal face)
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
//                      Public API
// ============================================================
void RobotEye::setExpression(Expression expr) {
    setExpression(expr, false);
}
void RobotEye::setExpression(Expression expr, bool useBlink) {
    if (expr >= EXPR_COUNT) return;
    if (expr == _currentExpr && _transState == TR_IDLE) return;
    // Any external switch immediately interrupts half blink (half blink does not block switch)
    _halfBlinkState = HB_IDLE;
    // Bitmap special expressions switch instantly (bitmap and vector params cannot interpolate)
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
    // Half blink is not considered busy (interruptible anytime), only check blocking states
    return _transState != TR_IDLE || _gazeState != GZ_IDLE ||
           _blinkState != BLK_IDLE;
}
void RobotEye::setExprGenerator(Expression id, RE_ExprGenerator gen) {
    if (id < EXPR_COUNT) {
        _exprGens[id] = gen;
    }
}
// ============================================================
//                      Main Loop
// ============================================================
void RobotEye::update() {
    uint32_t now = millis();
    // 1. Advance all state machines
    _updateBoot(now);
    _updateTransition(now);
    _updateGaze(now);
    _updateDrift(now);
    _updateHalfBlink(now);
    _updateBlink(now);
    // 2. Calculate frame parameters once per frame
    _computeFrame(now);
    // 3. Render to screen (full frame once, or multiple passes for page buffer)
    _renderBuffer();
}
// ============================================================
//                   Math Helpers
// ============================================================
float RobotEye::_clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
float RobotEye::_lerp(float a, float b, float t) { return a + (b - a) * t; }
// Easing function: accelerate then decelerate
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
//                   Parameter Interpolation
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
//  Base Face: dynamically centered coordinates + random drift
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
// Lookup and call expression generator function
FaceParams RobotEye::_getExprFace(Expression id) {
    FaceParams base = _getBaseFace();
    int16_t lcy = base.left.y  + base.left.h  / 2;
    int16_t rcy = base.right.y + base.right.h / 2;
    if (id < EXPR_COUNT && _exprGens[id] != nullptr) {
        return _exprGens[id](base, lcy, rcy);
    }
    // Unregistered slot (including user slots) returns normal base face
    return base;
}
// ============================================================
//  Blink Transform: height compression (center Y unchanged) + horizontal expand
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
//  Half Blink Transform: slight height reduction, no width expansion
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
//  Vector Render - Single Eye
//
//  Draw Order: White rounded rectangle --- top horizontal cut --- bottom horizontal cut --- top slope cut
//
//  topSlopeL/R semantics: cutting depth downward from top on each side
//    mn=min(L,R): full horizontal black for first mn rows; mx=max(L,R): triangle between mn~mx
//    R>L → triangle on right (high left, low right); L>R → triangle on left (low left, high right)
// ============================================================
void RobotEye::_drawEye(const EyeParams& p) {
    if (p.w == 0 || p.h == 0) return;
    uint8_t r = p.r;
    if (r > p.w / 2) r = p.w / 2;
    if (r > p.h / 2) r = p.h / 2;
    // 1. White rounded rectangle (base eye shape)
    _u8g2->setDrawColor(COL_WHITE);
    _u8g2->drawRBox(p.x, p.y, p.w, p.h, r);
    // 2. Top horizontal cut
    if (p.topCut > 0 && p.topCut < p.h) {
        _u8g2->setDrawColor(COL_BLACK);
        _u8g2->drawBox(p.x, p.y, p.w, p.topCut);
    }
    // 3. Bottom horizontal cut
    if (p.bottomCut > 0 && p.bottomCut < p.h) {
        _u8g2->setDrawColor(COL_BLACK);
        _u8g2->drawBox(p.x, p.y + p.h - p.bottomCut, p.w, p.bottomCut);
    }
    // 4. Top slope cut (per-line precise calculation, same region as drawTriangle for cleaner edges)
    if (p.topSlopeL > 0 || p.topSlopeR > 0) {
        uint8_t mn = _minU8(p.topSlopeL, p.topSlopeR);
        uint8_t mx = _maxU8(p.topSlopeL, p.topSlopeR);
        if (mn > p.h) mn = p.h;
        if (mx > p.h) mx = p.h;
        _u8g2->setDrawColor(COL_BLACK);
        // 4a. Fill common height region with solid black
        if (mn > 0)
            _u8g2->drawBox(p.x, p.y, p.w, mn);
        // 4b. Per-line precise draw for triangular region
        if (mx > mn) {
            uint8_t span = mx - mn;
            for (uint8_t dy = 0; dy < span; dy++) {
                uint8_t y = mn + dy;
                uint8_t blackW;
                if (p.topSlopeR > p.topSlopeL) {
                    // Triangle on right: full black at dy=0, narrow line by line
                    blackW = (uint8_t)(p.w - (uint32_t)p.w * dy / span);
                    if (blackW > 0)
                        _u8g2->drawBox(p.x + p.w - blackW, p.y + y, blackW, 1);
                } else {
                    // Triangle on left: full black at dy=0, narrow line by line
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
//  Filled Ellipse (black mask for excited expression)
//  Uses precomputed lookup table _ellipseLut, only integer math at runtime
//
//  Params: cx/cy=center, rx/ry=current ellipse radius (changes during morph),
//        mask=0~1 ellipse scale ratio
//  Principle: lookup full-size half width, scale proportionally by mask
//        (uniform scaling preserves ellipse shape)
// ============================================================
void RobotEye::_fillEllipse(int cx, int cy, int rx, int ry, float mask) {
    if (rx <= 0 || ry <= 0 || mask <= 0.01f) return;
    _u8g2->setDrawColor(COL_BLACK);
    for (int y = -ry; y <= ry; y++) {
        // Map current y to lookup table index (0~EXCITED_MASK_RY)
        int ay = y < 0 ? -y : y;
        int lutY = (int)((float)ay * EXCITED_MASK_RY / ry);
        if (lutY > EXCITED_MASK_RY) lutY = EXCITED_MASK_RY;
        // Lookup full half width and scale with mask
        int x = (int)((float)_ellipseLut[lutY] * mask);
        if (x > 0)
            _u8g2->drawBox(cx - x, cy + y, (uint8_t)(x * 2), 1);
    }
    _u8g2->setDrawColor(COL_WHITE);
}
// ============================================================
//  Battery Vector Draw (segment grid, auto center for screen)
// ============================================================
void RobotEye::_drawBattery(uint8_t level) {
    int x = (_screenW - BATTERY_W) / 2;
    int y = (_screenH - BATTERY_H) / 2;
    int bodyW = BATTERY_W - BATTERY_TERM_W;
    _u8g2->setDrawColor(COL_WHITE);
    _u8g2->drawRFrame(x, y, bodyW, BATTERY_H, 2);
    _u8g2->drawBox(x + bodyW, y + (BATTERY_H - BATTERY_TERM_H) / 2,
                   BATTERY_TERM_W, BATTERY_TERM_H);
    // Calculate lit segments with rounding
    uint8_t lit = (uint8_t)((level * BATTERY_GRID_COUNT + 99) / 100);
    if (lit > BATTERY_GRID_COUNT) lit = BATTERY_GRID_COUNT;
    int gridAreaW = bodyW - 6;
    int gridW = gridAreaW / BATTERY_GRID_COUNT;
    for (uint8_t i = 0; i < lit; i++) {
        _u8g2->drawBox(x + 3 + i * gridW, y + 3, gridW - 1, BATTERY_H - 6);
    }
}
// ============================================================
//  Placeholder text when bitmap expression is disabled
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
//  Pixel-wise Irisoled bitmap parser (byte optimized version)
//
//  Format: 128×64 horizontal XBM, 16 bytes per line, MSB left-aligned, stored in PROGMEM
//  U8g2 drawXBM bit order incompatible, implemented manual parsing
//
//  Skip full black bytes, batch drawBox(8px) for full white bytes, pixel render only for mixed bytes
// ============================================================
void RobotEye::_drawBitmapIrisoled(const uint8_t* bitmap) {
    _u8g2->setDrawColor(COL_WHITE);
    for (int y = 0; y < BITMAP_H; y++) {
        for (int byteIdx = 0; byteIdx < BITMAP_W / 8; byteIdx++) {
            uint8_t byteVal = RE_PGM_READ_BYTE(&bitmap[y * (BITMAP_W / 8) + byteIdx]);
            if (byteVal == 0x00) continue;       // Skip fully black byte
            int x = byteIdx * 8;
            if (byteVal == 0xFF) {
                _u8g2->drawBox(x, y, 8, 1);     // Batch draw 8 pixels for fully white byte
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
//                   Boot Animation State Machine
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
//                   Blink State Machine
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
//                   Half Blink State Machine
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
//                   Gaze State Machine
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
//                   Random Drift
// ============================================================
void RobotEye::_updateDrift(uint32_t now) {
    if (DEBUG_DISABLE_DRIFT) return;
    if (_bootPhase != BOOT_IDLE) return;
    if (_currentExpr != EXPR_NORMAL) return;
    if (now >= _nextDriftTime) {
        _driftTargetX = (float)random(-DRIFT_AMPLITUDE, DRIFT_AMPLITUDE + 1);
        _driftTargetY = (float)random(-DRIFT_AMPLITUDE, DRIFT_AMPLITUDE + 1);
        // Restrict within absolute boundary on generation (no runtime collision check)
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
//                   Expression Transition
// ============================================================
void RobotEye::_updateTransition(uint32_t now) {
    if (_transState != TR_MORPH) return;
    if (now - _transStartTime >= MORPH_TRANS_MS) {
        _currentExpr = _transToExpr;
        _transState = TR_IDLE;
    }
}
// ============================================================
//                   Calculation Layer: compute frame parameters
//  Executed once per frame. Result stored in _frameMode/_frameFace/_frameExcitedMask,
//  read-only for render layer. Render runs multiple times in page buffer mode without recalculation.
// ============================================================
void RobotEye::_computeFrame(uint32_t now) {
    // --- Special Expression: Bitmap ---
    if (_isBitmapExpr(_currentExpr)) {
#ifdef RE_ENABLE_BITMAP_EXPR
        _frameMode = FRAME_BITMAP;
#else
        _frameMode = FRAME_NOT_SHOW;
#endif
        return;
    }
    // --- Special Expression: Battery ---
    if (_currentExpr == EXPR_BATTERY) {
        _frameMode = FRAME_BATTERY;
        return;
    }
    // --- Vector Eye Expression ---
    _frameMode = FRAME_EYE;
    _frameExcitedMask = 0.0f;
    if (_transState == TR_MORPH) {
        // Morph transition: linear interpolate all parameters
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
        // Opening phase of blink switch: new expression fades in with opening
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
    // --- Boot animation phases ---
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
        // Stable idle state
        _frameFace = _getExprFace(_currentExpr);
        if (_currentExpr == EXPR_EXCITED) _frameExcitedMask = 1.0f;
    }
    // --- Apply blink transform ---
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
    // --- Apply half blink transform ---
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
//  Render Buffer: select full buffer or page buffer by build config
// ============================================================
void RobotEye::_renderBuffer() {
#ifdef RE_PAGE_BUFFER
    // Page buffer mode: draw multiple passes (one page each), save RAM
    _u8g2->firstPage();
    do {
        _drawFrame();
    } while (_u8g2->nextPage());
#else
    // Full frame buffer mode: clear, draw, transmit once
    _u8g2->clearBuffer();
    _drawFrame();
    _u8g2->sendBuffer();
#endif
}
// ============================================================
//  Draw Layer: pure render logic with no side effects, read-only _frame* members
//  Runs multiple times under page buffer mode, must not modify any state
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
        // excited ellipse mask (lookup table, no sqrtf)
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
        // Debug grid
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
