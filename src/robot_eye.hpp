#ifndef ROBOT_EYE_HPP
#define ROBOT_EYE_HPP
#include <Arduino.h>
#include <U8g2lib.h>
// ============================================================
//  RobotEye Vector Robot Eye Library for U8g2
// ============================================================
#define RE_ENABLE_BITMAP_EXPR
// ============================================================
//  [Build Config 2] Buffer Mode
//  Comment out (Default) → Full frame buffer: occupies width*height/8 bytes RAM
//                   (128×64 screen uses 1024 bytes), fast refresh, recommended for ESP32
//  Uncomment            → Page buffer: occupies width bytes RAM (128 bytes),
//                   slower refresh (draw 8 times per frame), suitable for low RAM platforms like AVR
// ============================================================
// #define RE_PAGE_BUFFER
// ============================================================
//  Single eye parameter description (minimum unit for vector drawing)
//  All expressions are described by this struct, linear interpolation between any two for morph
// ============================================================
struct EyeParams {
    int16_t x, y;           // Top-left coordinate
    uint8_t w, h;           // Width, Height
    uint8_t r;              // Rounded corner radius
    uint8_t topCut;         // Horizontal cut pixels from top (cut top, keep bottom part)
    uint8_t bottomCut;      // Horizontal cut pixels from bottom (cut bottom, keep top part)
    uint8_t topSlopeL;      // Top left cut depth (0=no slope, larger value lowers start position of white area)
    uint8_t topSlopeR;      // Top right cut depth
    uint8_t bottomSlopeL;   // Bottom left (reserved, unused currently)
    uint8_t bottomSlopeR;   // Bottom right (reserved, unused currently)
};
// A pair of eyes = left eye + right eye
struct FaceParams {
    EyeParams left;
    EyeParams right;
};
// Expression generator function type (used to replace built-in expressions or register custom user expressions)
// Params: base=base face (contains drift and offset), lcy/rcy=center Y coordinate of left/right eye
typedef FaceParams (*RE_ExprGenerator)(const FaceParams& base, int16_t lcy, int16_t rcy);
// ============================================================
//                      RobotEye Class
// ============================================================
class RobotEye {
public:
    // ========================================================
    //  Expression Enumeration
    //  First 24 are vector eye expressions, followed by 5 special expressions, last 8 user slots
    // ========================================================
    enum Expression : uint8_t {
        // --- Vector Eye Expressions (24 items) ---
        EXPR_NORMAL = 0,      // Normal open eye (base expression)
        EXPR_ALERT,           // Alert (narrow vertical bar)
        EXPR_ANGRY,           // Angry (top slope eyebrow peak, bottom keeps normal rounded corner)
        EXPR_BLINK,           // Blink horizontal line (centered)
        EXPR_BLINK_DOWN,      // Blink horizontal line (lower offset)
        EXPR_BLINK_UP,        // Blink horizontal line (upper offset)
        EXPR_BORED,           // Bored (flat arc lower position)
        EXPR_DESPAIR,         // Despair (scared shape + top cut)
        EXPR_DISORIENTED,     // Disoriented (large eyes + wider gap)
        EXPR_EXCITED,         // Excited (happy + ellipse mask crescent eyes)
        EXPR_FOCUSED,         // Focused (half height)
        EXPR_FURIOUS,         // Furious (shorter angry version)
        EXPR_HAPPY,           // Happy (bottom cut smiling eyes)
        EXPR_LOOK_DOWN,       // Look down
        EXPR_LOOK_LEFT,       // Look left (left eye scaled and offset)
        EXPR_LOOK_RIGHT,      // Look right (right eye scaled and offset)
        EXPR_LOOK_UP,         // Look up
        EXPR_SAD,             // Sad (top slope + shift down, frown eyebrows)
        EXPR_SCARED,          // Scared (mirror angry + shift up + sharp corner)
        EXPR_SLEEPY,          // Sleepy (gentle top slope)
        EXPR_SURPRISED,       // Surprised (eyes move closer)
        EXPR_WINK_LEFT,       // Left eye wink
        EXPR_WINK_RIGHT,      // Right eye wink
        EXPR_WORRIED,         // Worried (scared slope + rounded corner + centered)
        // --- Special Expressions (5 items) ---
        EXPR_BATTERY,         // Battery icon (vector segmented grid, controlled by setBatteryLevel)
        EXPR_WARNING,         // Warning (Irisoled bitmap, requires RE_ENABLE_BITMAP_EXPR)
        EXPR_LEFT_SIGNAL,     // Left turn signal (Irisoled bitmap)
        EXPR_RIGHT_SIGNAL,    // Right turn signal (Irisoled bitmap)
        EXPR_MODE,            // Mode/Settings (Irisoled bitmap)
        // --- User Custom Slots (8 items) ---
        EXPR_USER_1,          // User custom expression 1 (register with setExprGenerator)
        EXPR_USER_2,          // User custom expression 2
        EXPR_USER_3,          // User custom expression 3
        EXPR_USER_4,          // User custom expression 4
        EXPR_USER_5,          // User custom expression 5
        EXPR_USER_6,          // User custom expression 6
        EXPR_USER_7,          // User custom expression 7
        EXPR_USER_8,          // User custom expression 8
        EXPR_COUNT            // Total expression count (array boundary, not usable expression)
    };
    // ========================================================
    //  [Adjustable Parameter Central Area]
    //  All parameters are static constexpr, compile-time constants, no RAM consumption.
    //  Recompile after modification to take effect. Parameter names have class scope to avoid conflict.
    // ========================================================
    // --- Fallback screen size (actual size read from U8g2 during begin()) ---
    static constexpr uint16_t SCREEN_W_DEFAULT = 128;  // Fallback screen width (pixel)
    static constexpr uint16_t SCREEN_H_DEFAULT = 64;   // Fallback screen height (pixel)
    static constexpr uint8_t  BITMAP_W        = 128;   // Fixed width for Irisoled bitmap
    static constexpr uint8_t  BITMAP_H        = 64;    // Fixed height for Irisoled bitmap
    // --- Base eye geometry (normal baseline, all expressions transform based on this) ---
    static constexpr uint8_t BASE_EYE_W   = 32;   // Single eye width (pixel)
    static constexpr uint8_t BASE_EYE_H   = 32;   // Single eye height (pixel)
    static constexpr uint8_t BASE_EYE_R   = 10;   // Single eye rounded radius (pixel)
    static constexpr uint8_t BASE_EYE_GAP = 8;    // Gap between two eyes (pixel)
    // --- Global position fine tune (applied after auto center, default all zero) ---
    // Internal calculation: dx = OFFSET_RIGHT - OFFSET_LEFT, dy = OFFSET_DOWN - OFFSET_UP
    // Example: shift eyes left 2px → set OFFSET_LEFT=2; shift right → set OFFSET_RIGHT
    static constexpr int16_t OFFSET_LEFT  = 0;    // Global left shift fine tune (pixel)
    static constexpr int16_t OFFSET_RIGHT = 0;    // Global right shift fine tune (pixel)
    static constexpr int16_t OFFSET_UP    = 0;    // Global up shift fine tune (pixel)
    static constexpr int16_t OFFSET_DOWN  = 0;    // Global down shift fine tune (pixel)
    // --- Blink System ---
    static constexpr float   BLINK_MIN_H_RATIO  = 0.06f; // Remaining height ratio when closed (0~1), 0.06≈2px
    static constexpr uint16_t BLINK_CLOSE_MS    = 200;    // Duration of closing process (ms)
    static constexpr uint16_t BLINK_HOLD_MIN_MS = 30;     // Minimum hold duration while closed (ms)
    static constexpr uint16_t BLINK_HOLD_MAX_MS = 60;     // Maximum hold duration while closed (ms)
    static constexpr uint16_t BLINK_OPEN_MS     = 100;    // Duration of opening process (ms)
    static constexpr uint8_t  BLINK_EXPAND_PX   = 4;      // Single side expand pixel when closing
    static constexpr uint32_t BLINK_INTERVAL_MIN = 7000;  // Minimum interval between two blinks (ms, only normal expression)
    static constexpr uint32_t BLINK_INTERVAL_MAX = 12000; // Maximum interval between two blinks (ms, only normal expression)
    // --- Half blink system (random idle shrink, height only change) ---
    static constexpr uint8_t  HALF_BLINK_CHANCE        = 30;    // Trigger probability (%)
    static constexpr uint16_t HALF_BLINK_PHASE_MS      = 150;   // Single phase duration (ms), shrink + expand = 300ms
    static constexpr float    HALF_BLINK_HEIGHT_RATIO  = 0.70f; // Minimum height ratio after shrink
    static constexpr uint32_t HALF_BLINK_INTERVAL_MIN  = 4000;  // Minimum check interval (ms)
    static constexpr uint32_t HALF_BLINK_INTERVAL_MAX  = 8000;  // Maximum check interval (ms)
    // --- Gaze system (normal → direction → auto reset full cycle) ---
    static constexpr uint16_t GAZE_TRANS_MS     = 600;   // Direction switch transition duration (ms)
    static constexpr uint16_t GAZE_HOLD_MIN_MS  = 1000;  // Minimum hold time after gazing (ms)
    static constexpr uint16_t GAZE_HOLD_MAX_MS  = 2000;  // Maximum hold time after gazing (ms)
    static constexpr uint32_t GAZE_INTERVAL_MIN = 4000;  // Minimum check interval (ms)
    static constexpr uint32_t GAZE_INTERVAL_MAX = 7000;  // Maximum check interval (ms)
    static constexpr uint8_t  GAZE_CHANCE       = 30;    // Trigger probability (%)
    // --- Stare形态 (Exclusive for Look_Left / Look_Right) ---
    static constexpr uint8_t GAZE_EYE_OFFSET_PX   = 14;    // Offset pixel for facing eye
    static constexpr float   GAZE_EYE_SCALE       = 1.3f;  // Scale factor for facing eye
    static constexpr uint8_t GAZE_MIN_CENTER_DIST = 4;     // Minimum center distance between two eyes (pixel)
    // --- Normal random drift ---
    static constexpr uint8_t  DRIFT_AMPLITUDE   = 12;     // Max target amplitude for single drift (pixel)
    static constexpr uint8_t  DRIFT_MAX_X       = 4;      // Absolute X boundary (pixel)
    static constexpr uint8_t  DRIFT_MAX_Y       = 4;      // Absolute Y boundary (pixel)
    static constexpr uint32_t DRIFT_INTERVAL_MS = 5000;   // New target generate interval (ms)
    static constexpr float    DRIFT_SPEED       = 0.40f;  // Frame target approach ratio (0~1)
    // --- Morph Transition ---
    static constexpr uint16_t MORPH_TRANS_MS = 500;  // Total morph transition duration (ms)
    // --- Boot Animation ---
    // Flow: Close(500ms) → Transition to half open(1500ms) → 3 half open blinks(300ms gap) → Transition full open(1500ms)
    static constexpr uint16_t BOOT_CLOSE_MS        = 500;   // Initial close duration (ms)
    static constexpr uint16_t BOOT_TO_HALF_MS      = 1500;  // Close → half open transition (ms)
    static constexpr float    BOOT_HALF_OPEN_RATIO = 0.7f;  // Half open height ratio
    static constexpr uint8_t  BOOT_HALF_BLINK_CNT  = 3;     // Blink count at half open state
    static constexpr uint16_t BOOT_HALF_BLINK_GAP  = 300;   // Half open blink interval (ms)
    static constexpr uint16_t BOOT_TO_FULL_MS      = 1500;  // Half open → full open transition (ms)
    // --- Expression exclusive parameters ---
    static constexpr float HAPPY_BOTTOM_CUT_RATIO = 0.625f; // Happy bottom cut ratio
    static constexpr float ANGRY_HEIGHT_RATIO     = 0.7f;   // Angry visible height ratio (cut from top)
    static constexpr float ANGRY_SLOPE_RATIO      = 0.25f;  // Angry slope vertical span ratio
    static constexpr int16_t SAD_DOWN_OFFSET      = 12;     // Sad global down shift pixel
    static constexpr float SAD_SLOPE_HIGH_RATIO   = 0.5f;   // Sad slope high point ratio (outer side)
    static constexpr float SAD_SLOPE_LOW_RATIO    = 0.75f;  // Sad slope low point ratio (inner side)
    static constexpr float SLEEPY_SLOPE_HIGH_RATIO = 0.70f; // Sleepy slope high point ratio
    static constexpr float SLEEPY_SLOPE_LOW_RATIO  = 0.80f; // Sleepy slope low point ratio
    static constexpr uint8_t SURPRISED_INSET_PX   = 3;      // Surprised inward shift pixel per eye
    static constexpr uint8_t EXCITED_MASK_RX      = 25;     // Excited ellipse horizontal radius
    static constexpr uint8_t EXCITED_MASK_RY      = 12;     // Excited ellipse vertical radius
    static constexpr uint8_t SCARED_SLOPE_PX      = 5;      // Scared slope vertical span (pixel)
    static constexpr int16_t SCARED_UP_OFFSET     = 6;      // Scared global up shift pixel
    static constexpr float FOCUSED_HEIGHT_RATIO   = 0.5f;   // Focused height ratio
    static constexpr float DESPAIR_HEIGHT_RATIO   = 0.5f;   // Despair visible height ratio
    static constexpr float ALERT_WIDTH_RATIO      = 0.25f;  // Alert width ratio
    static constexpr int16_t ALERT_HEIGHT_ADD     = 4;      // Alert extra height (pixel)
    static constexpr int16_t BLINK_DOWN_OFFSET    = 10;     // Blink_Down shift down pixel
    static constexpr int16_t BLINK_UP_OFFSET      = 10;     // Blink_Up shift up pixel
    static constexpr uint8_t DISORIENTED_W        = 30;     // Disoriented single eye width
    static constexpr uint8_t DISORIENTED_H        = 30;     // Disoriented single eye height
    static constexpr uint8_t DISORIENTED_GAP_ADD  = 10;     // Disoriented extra gap pixel
    static constexpr float FURIOUS_HEIGHT_RATIO   = (2.0f/3.0f); // Furious visible height ratio
    static constexpr int16_t LOOK_DOWN_OFFSET     = 10;     // Look_Down shift down pixel
    static constexpr int16_t LOOK_UP_OFFSET       = 10;     // Look_Up shift up pixel
    static constexpr uint8_t WORRIED_CORNER_RADIUS = 8;     // Worried rounded corner radius
    // --- Battery Icon Parameters ---
    static constexpr uint8_t BATTERY_GRID_COUNT = 6;   // Inner segment count
    static constexpr uint8_t BATTERY_W          = 44;  // Total width (include positive terminal bump)
    static constexpr uint8_t BATTERY_H          = 20;  // Height
    static constexpr uint8_t BATTERY_TERM_W     = 4;   // Positive terminal bump width
    static constexpr uint8_t BATTERY_TERM_H     = 8;   // Positive terminal bump height
    // --- Debug Switch (true=enable) ---
    static constexpr bool DEBUG_SKIP_BOOT          = false; // Skip boot animation
    static constexpr bool DEBUG_FIXED_BLINK        = false; // Fixed blink interval 5s
    static constexpr bool DEBUG_DISABLE_GAZE       = false; // Disable random gaze
    static constexpr bool DEBUG_DISABLE_DRIFT      = false; // Disable random drift
    static constexpr bool DEBUG_DISABLE_HALF_BLINK = false; // Disable half blink
    static constexpr bool DEBUG_SHOW_GRID          = false; // Show 16px positioning grid
    
    // Public Methods
    // Constructor: pass pointer to initialized U8g2 object
    explicit RobotEye(U8G2* u8g2);
    // Initialize (call once inside setup())
    // seed: random seed, pass 0 to auto select best platform method
    //        (ESP32 uses hardware true random, others use millis())
    //        pass non-zero value to use custom seed
    void begin(uint32_t seed = 0);
    // Main loop update (call every frame inside loop(), recommend ≥30fps)
    // Automatically advance all state machines and render one frame internally
    void update();
    // Set expression (smooth morph transition, no blink)
    void setExpression(Expression expr);
    // Set expression (select transition method)
    // useBlink=true  : close eye → switch expression → fade in while opening
    // useBlink=false : direct vector interpolate morph transition
    void setExpression(Expression expr, bool useBlink);
    // Set battery level 0~100 (only valid for EXPR_BATTERY)
    void setBatteryLevel(uint8_t percent);
    // Manually trigger one blink
    void triggerBlink();
    // Get stable current expression (returns expression before transition during morph)
    Expression getCurrentExpression() const;
    // Check busy status (morph transition / blink switch / gazing)
    // Note: half blink is NOT considered busy, can be interrupted anytime by setExpression
    bool isBusy() const;
    // Replace expression generator function (custom expression)
    // id: expression enum value (built-in expression or EXPR_USER_1~8)
    // gen: generator function pointer, pass nullptr restore default (normal face)
    //
    // Usage example:
    //   FaceParams myHappy(const FaceParams& base, int16_t, int16_t) {
    //       FaceParams f = base;
    //       f.left.bottomCut = f.right.bottomCut = 8;
    //       return f;
    //   }
    //   eye.setExprGenerator(RobotEye::EXPR_USER_1, myHappy);
    //   eye.setExpression(RobotEye::EXPR_USER_1);
    void setExprGenerator(Expression id, RE_ExprGenerator gen);
private:
    // ========================================================
    //  Internal State Machine Enumeration
    // ========================================================
    enum BootPhase      : uint8_t { BOOT_CLOSE, BOOT_TO_HALF, BOOT_HALF_BLINK, BOOT_TO_FULL, BOOT_IDLE };
    enum BlinkState     : uint8_t { BLK_IDLE, BLK_CLOSING, BLK_HOLD, BLK_OPENING };
    enum GazeState      : uint8_t { GZ_IDLE, GZ_TO_DIR, GZ_HOLDING, GZ_TO_CENTER };
    enum HalfBlinkState : uint8_t { HB_IDLE, HB_SHRINKING, HB_EXPANDING };
    enum TransState     : uint8_t { TR_IDLE, TR_MORPH, TR_BLINK_SWITCH };
    // Render mode of current frame (set by calculation layer, read by draw layer)
    enum FrameMode : uint8_t {
        FRAME_EYE,       // Vector eyes
        FRAME_BATTERY,   // Battery icon
        FRAME_BITMAP,    // Irisoled bitmap
        FRAME_NOT_SHOW   // "not showing" placeholder text
    };
    // ========================================================
    //  Member Variables - Display & Geometry
    // ========================================================
    U8G2*    _u8g2;              // U8g2 driver pointer
    uint16_t _screenW;           // Actual screen width (read from U8g2)
    uint16_t _screenH;           // Actual screen height
    int16_t  _baseLeftX;         // Left eye top-left X (auto center + offset fine tune)
    int16_t  _baseRightX;        // Right eye top-left X
    int16_t  _baseY;             // Both eyes top-left Y
    // ========================================================
    //  Member Variables - Expression Generator Table
    // ========================================================
    RE_ExprGenerator _exprGens[EXPR_COUNT];
    // ========================================================
    //  Member Variables - Ellipse Precompute Lookup Table
    //  Calculated in begin(), _fillEllipse lookup at runtime without sqrtf
    //  Index y(0~EXCITED_MASK_RY) → half width pixel count of that line
    // ========================================================
    uint8_t _ellipseLut[EXCITED_MASK_RY + 1];
    // ========================================================
    //  Member Variables - All State Machines
    // ========================================================
    BootPhase      _bootPhase;
    uint32_t       _bootStartTime;
    int            _bootBlinkCnt;
    uint32_t       _lastBootBlinkEnd;
    BlinkState     _blinkState;
    uint32_t       _blinkStartTime;
    uint32_t       _blinkHoldDuration;
    uint32_t       _nextBlinkTime;
    HalfBlinkState _halfBlinkState;
    uint32_t       _halfBlinkStartTime;
    uint32_t       _nextHalfBlinkTime;
    GazeState      _gazeState;
    uint32_t       _gazeStartTime;
    uint32_t       _gazeHoldEnd;
    uint32_t       _nextGazeTime;
    Expression     _gazeDirExpr;
    float          _driftX, _driftY;
    float          _driftTargetX, _driftTargetY;
    uint32_t       _nextDriftTime;
    TransState     _transState;
    uint32_t       _transStartTime;
    Expression     _transFromExpr;
    Expression     _transToExpr;
    Expression     _currentExpr;
    uint8_t        _batteryLevel;
    // ========================================================
    //  Member Variables - Current Frame Calculation Result (write by calculation layer, read by draw layer)
    //  Page buffer mode executes draw layer multiple times, only read these variables without recalculation
    // ========================================================
    FrameMode  _frameMode;          // Current frame type
    FaceParams _frameFace;          // Current frame eye parameters
    float      _frameExcitedMask;   // Current frame ellipse mask ratio (0~1)
    // ========================================================
    //  Math Utilities
    // ========================================================
    static float   _clamp01(float v);
    static float   _lerp(float a, float b, float t);
    static float   _easeInOutQuad(float t);
    static uint8_t _lerpU8(uint8_t a, uint8_t b, float t);
    static int16_t _lerpI16(int16_t a, int16_t b, float t);
    static uint8_t _minU8(uint8_t a, uint8_t b);
    static uint8_t _maxU8(uint8_t a, uint8_t b);
    // ========================================================
    //  Parameter Interpolation & Expression Generation
    // ========================================================
    EyeParams  _lerpEye(const EyeParams& a, const EyeParams& b, float t);
    FaceParams _lerpFace(const FaceParams& a, const FaceParams& b, float t);
    FaceParams _getBaseFace();
    FaceParams _getExprFace(Expression id);
    FaceParams _getHappyFace();
    FaceParams _applyBlink(const FaceParams& src, float amount);
    FaceParams _applyHalfBlink(const FaceParams& src, float amount);
    void       _registerBuiltins();
    // ========================================================
    //  Render - Calculation Layer (execute once per frame, advance state + compute params)
    // ========================================================
    void _computeFrame(uint32_t now);
    // ========================================================
    //  Render - Draw Layer (pure draw with no side effect, run multiple times under page buffer mode)
    // ========================================================
    void _renderBuffer();        // Select full/page buffer to call _drawFrame
    void _drawFrame();           // Draw one frame content into buffer
    void _drawEye(const EyeParams& p);
    void _drawFace(const FaceParams& face);
    void _fillEllipse(int cx, int cy, int rx, int ry, float mask);
    void _drawBattery(uint8_t level);
    void _drawNotShowing();
#ifdef RE_ENABLE_BITMAP_EXPR
    void _drawBitmapIrisoled(const uint8_t* bitmap);
#endif
    // ========================================================
    //  Expression Type Judge
    // ========================================================
    bool _isSpecialExpr(Expression expr) const;
    bool _isBitmapExpr(Expression expr) const;
    // ========================================================
    //  State Machine Update
    // ========================================================
    void _updateBoot(uint32_t now);
    void _updateBlink(uint32_t now);
    void _updateHalfBlink(uint32_t now);
    void _updateGaze(uint32_t now);
    void _updateDrift(uint32_t now);
    void _updateTransition(uint32_t now);
    // ========================================================
    //  Utilities
    // ========================================================
    void _startBlink();
    uint32_t _randBlinkInterval();
    void _reset();
};
#endif // ROBOT_EYE_HPP
