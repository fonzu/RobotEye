#ifndef ROBOT_EYE_HPP
#define ROBOT_EYE_HPP

#include <Arduino.h>
#include <U8g2lib.h>

// ============================================================
//  RobotEye 矢量机器人眼睛库 U8g2
//
//  - 所有可调参数为类内 static constexpr，有作用域、有类型检查，
//    不污染全局命名空间，无需前缀防冲突
//  - 计算层与绘制层分离，支持全帧缓冲(_F_)和页缓冲(_1_/_2_)两种模式
//  - Irisoled 位图依赖可选（注释 RE_ENABLE_BITMAP_EXPR 即可移除）
//  - 屏幕尺寸从 U8g2 对象自动读取，眼睛坐标自动居中，支持4方向微调
//  - 24个内置表情可替换 + 8个用户自定义槽位，函数指针注册
//  - 平台自适应：AVR/ESP32/ESP8266/STM32 均可编译
//  - excited 椭圆遮挡预计算查找表，运行时无 sqrtf
// ============================================================


// ============================================================
//  【编译配置一】位图特殊表情开关
//  保留定义 → 启用 Warning/Signal/Mode 位图表情（需安装 Irisoled 库）
//  注释掉   → 不依赖 Irisoled，位图表情显示 "not showing"
// ============================================================
#define RE_ENABLE_BITMAP_EXPR

// ============================================================
//  【编译配置二】缓冲模式
//  注释掉（默认）→ 全帧缓冲：占用 width*height/8 字节 RAM
//                   （128×64 屏幕占 1024 字节），刷新快，推荐 ESP32
//  取消注释      → 页缓冲：占用 width 字节 RAM（128 字节），
//                   刷新慢（同帧绘制8次），适合小 RAM 的 AVR 等平台
// ============================================================
// #define RE_PAGE_BUFFER


// ============================================================
//  单只眼睛参数描述（矢量绘制最小单元）
//  所有表情通过此结构体描述，任意两个之间可线性插值实现 morph
// ============================================================
struct EyeParams {
    int16_t x, y;           // 左上角坐标
    uint8_t w, h;           // 宽度、高度
    uint8_t r;              // 圆角半径
    uint8_t topCut;         // 顶部水平裁切像素数（从顶部裁掉，保留下方）
    uint8_t bottomCut;      // 底部水平裁切像素数（从底部裁掉，保留上方）
    uint8_t topSlopeL;      // 顶部左侧裁切深度（0=无斜线，值越大该侧白色开始位置越低）
    uint8_t topSlopeR;      // 顶部右侧裁切深度
    uint8_t bottomSlopeL;   // 底部左侧（预留，当前未使用）
    uint8_t bottomSlopeR;   // 底部右侧（预留，当前未使用）
};

// 一双眼睛 = 左眼 + 右眼
struct FaceParams {
    EyeParams left;
    EyeParams right;
};

// 表情生成函数类型（用于替换内置表情或注册用户自定义表情）
// 参数：base=基准脸（含漂移和偏移），lcy/rcy=左右眼中心Y坐标
typedef FaceParams (*RE_ExprGenerator)(const FaceParams& base, int16_t lcy, int16_t rcy);


// ============================================================
//                      RobotEye 类
// ============================================================
class RobotEye {
public:
    // ========================================================
    //  表情枚举
    //  前24个为矢量眼睛表情，接着5个特殊表情，最后8个用户槽位
    // ========================================================
    enum Expression : uint8_t {
        // --- 矢量眼睛表情（24个）---
        EXPR_NORMAL = 0,      // 正常睁眼（基准表情）
        EXPR_ALERT,           // 警觉（窄竖条）
        EXPR_ANGRY,           // 愤怒（顶部斜线眉峰，底部保留normal圆角）
        EXPR_BLINK,           // 眨眼横线（居中）
        EXPR_BLINK_DOWN,      // 眨眼横线（偏下）
        EXPR_BLINK_UP,        // 眨眼横线（偏上）
        EXPR_BORED,           // 无聊（平弧偏下）
        EXPR_DESPAIR,         // 绝望（scared形状+顶部裁切）
        EXPR_DISORIENTED,     // 迷茫（大眼+宽间距）
        EXPR_EXCITED,         // 兴奋（happy+椭圆遮挡月牙眼）
        EXPR_FOCUSED,         // 专注（高度减半）
        EXPR_FURIOUS,         // 暴怒（angry更矮版）
        EXPR_HAPPY,           // 开心（底部裁切笑眼）
        EXPR_LOOK_DOWN,       // 向下看
        EXPR_LOOK_LEFT,       // 向左看（左眼放大偏移）
        EXPR_LOOK_RIGHT,      // 向右看（右眼放大偏移）
        EXPR_LOOK_UP,         // 向上看
        EXPR_SAD,             // 悲伤（顶部斜线+下移，八字眉）
        EXPR_SCARED,          // 害怕（angry镜像+上移+直角）
        EXPR_SLEEPY,          // 困倦（顶部缓斜线）
        EXPR_SURPRISED,       // 惊讶（两眼靠近）
        EXPR_WINK_LEFT,       // 左眼眨眼
        EXPR_WINK_RIGHT,      // 右眼眨眼
        EXPR_WORRIED,         // 担心（scared斜线+圆角+居中）
        // --- 特殊表情（5个）---
        EXPR_BATTERY,         // 电池图标（矢量分段格，setBatteryLevel控制）
        EXPR_WARNING,         // 警告（Irisoled位图，需启用RE_ENABLE_BITMAP_EXPR）
        EXPR_LEFT_SIGNAL,     // 左转向灯（Irisoled位图）
        EXPR_RIGHT_SIGNAL,    // 右转向灯（Irisoled位图）
        EXPR_MODE,            // 模式/设置（Irisoled位图）
        // --- 用户自定义槽位（8个）---
        EXPR_USER_1,          // 用户自定义表情1（用setExprGenerator注册）
        EXPR_USER_2,          // 用户自定义表情2
        EXPR_USER_3,          // 用户自定义表情3
        EXPR_USER_4,          // 用户自定义表情4
        EXPR_USER_5,          // 用户自定义表情5
        EXPR_USER_6,          // 用户自定义表情6
        EXPR_USER_7,          // 用户自定义表情7
        EXPR_USER_8,          // 用户自定义表情8
        EXPR_COUNT            // 表情总数（数组边界用，不是表情）
    };

    // ========================================================
    //  【可调参数集中区】
    //  所有参数为 static constexpr，编译期常量，不占 RAM。
    //  修改后重新编译即可生效。参数名自带类作用域，不会冲突。
    // ========================================================

    // --- 屏幕后备尺寸（实际尺寸在 begin() 时从 U8g2 自动读取）---
    static constexpr uint16_t SCREEN_W_DEFAULT = 128;  // 屏幕宽度后备值（像素）
    static constexpr uint16_t SCREEN_H_DEFAULT = 64;   // 屏幕高度后备值（像素）
    static constexpr uint8_t  BITMAP_W        = 128;   // Irisoled位图固定宽度
    static constexpr uint8_t  BITMAP_H        = 64;    // Irisoled位图固定高度

    // --- 眼睛基准几何（normal 基准，所有表情基于此变换）---
    static constexpr uint8_t BASE_EYE_W   = 32;   // 单眼宽度（像素）
    static constexpr uint8_t BASE_EYE_H   = 32;   // 单眼高度（像素）
    static constexpr uint8_t BASE_EYE_R   = 10;   // 单眼圆角半径（像素）
    static constexpr uint8_t BASE_EYE_GAP = 8;    // 两眼间距（像素）

    // --- 整体位置微调（自动居中后叠加，默认全0）---
    // 内部计算：dx = OFFSET_RIGHT - OFFSET_LEFT，dy = OFFSET_DOWN - OFFSET_UP
    // 例如想让眼睛整体左移2px，设 OFFSET_LEFT=2；想右移设 OFFSET_RIGHT
    static constexpr int16_t OFFSET_LEFT  = 0;    // 整体左移微调（像素）
    static constexpr int16_t OFFSET_RIGHT = 0;    // 整体右移微调（像素）
    static constexpr int16_t OFFSET_UP    = 0;    // 整体上移微调（像素）
    static constexpr int16_t OFFSET_DOWN  = 0;    // 整体下移微调（像素）

    // --- 眨眼系统 ---
    static constexpr float   BLINK_MIN_H_RATIO  = 0.06f; // 闭眼剩余高度比例（0~1），0.06≈2px
    static constexpr uint16_t BLINK_CLOSE_MS    = 200;    // 闭眼过程时长（毫秒）
    static constexpr uint16_t BLINK_HOLD_MIN_MS = 30;     // 闭眼最短停留（毫秒）
    static constexpr uint16_t BLINK_HOLD_MAX_MS = 60;     // 闭眼最长停留（毫秒）
    static constexpr uint16_t BLINK_OPEN_MS     = 100;    // 睁眼过程时长（毫秒）
    static constexpr uint8_t  BLINK_EXPAND_PX   = 4;      // 闭眼时单侧向外膨胀像素
    static constexpr uint32_t BLINK_INTERVAL_MIN = 7000;  // 两次眨眼最短间隔（毫秒，仅normal）
    static constexpr uint32_t BLINK_INTERVAL_MAX = 12000; // 两次眨眼最长间隔（毫秒，仅normal）

    // --- 半眯眼系统（空闲随机微缩，仅高度变化）---
    static constexpr uint8_t  HALF_BLINK_CHANCE        = 30;    // 触发概率（%）
    static constexpr uint16_t HALF_BLINK_PHASE_MS      = 150;   // 单段时长（毫秒），缩+放=300ms
    static constexpr float    HALF_BLINK_HEIGHT_RATIO  = 0.70f; // 缩到的最小高度比例
    static constexpr uint32_t HALF_BLINK_INTERVAL_MIN  = 4000;  // 检查间隔最小值（毫秒）
    static constexpr uint32_t HALF_BLINK_INTERVAL_MAX  = 8000;  // 检查间隔最大值（毫秒）

    // --- 瞟眼系统（normal→方向→自动回正完整周期）---
    static constexpr uint16_t GAZE_TRANS_MS     = 600;   // 方向切换过渡时长（毫秒）
    static constexpr uint16_t GAZE_HOLD_MIN_MS  = 1000;  // 瞟到后最短停留（毫秒）
    static constexpr uint16_t GAZE_HOLD_MAX_MS  = 2000;  // 瞟到后最长停留（毫秒）
    static constexpr uint32_t GAZE_INTERVAL_MIN = 4000;  // 检查间隔最小值（毫秒）
    static constexpr uint32_t GAZE_INTERVAL_MAX = 7000;  // 检查间隔最大值（毫秒）
    static constexpr uint8_t  GAZE_CHANCE       = 30;    // 触发概率（%）

    // --- 瞪眼（Look_Left / Look_Right 专属形态）---
    static constexpr uint8_t GAZE_EYE_OFFSET_PX   = 14;    // 朝向眼偏移像素数
    static constexpr float   GAZE_EYE_SCALE       = 1.3f;  // 朝向眼放大倍数
    static constexpr uint8_t GAZE_MIN_CENTER_DIST = 4;     // 两眼中心最小距离（像素）

    // --- normal 随机漂移 ---
    static constexpr uint8_t  DRIFT_AMPLITUDE   = 12;     // 单次漂移目标最大幅度（像素）
    static constexpr uint8_t  DRIFT_MAX_X       = 4;      // X方向绝对边界（像素）
    static constexpr uint8_t  DRIFT_MAX_Y       = 4;      // Y方向绝对边界（像素）
    static constexpr uint32_t DRIFT_INTERVAL_MS = 5000;   // 新目标生成间隔（毫秒）
    static constexpr float    DRIFT_SPEED       = 0.40f;  // 每帧逼近目标比例（0~1）

    // --- Morph 过渡 ---
    static constexpr uint16_t MORPH_TRANS_MS = 500;  // morph过渡总时长（毫秒）

    // --- 开机动画 ---
    // 流程：闭眼(500ms)→过渡到半睁(1500ms)→半睁眨3次(间隔300ms)→过渡到全睁(1500ms)
    static constexpr uint16_t BOOT_CLOSE_MS        = 500;   // 初始闭眼持续（毫秒）
    static constexpr uint16_t BOOT_TO_HALF_MS      = 1500;  // 闭眼→半睁过渡（毫秒）
    static constexpr float    BOOT_HALF_OPEN_RATIO = 0.7f;  // 半睁高度比例
    static constexpr uint8_t  BOOT_HALF_BLINK_CNT  = 3;     // 半睁状态眨眼次数
    static constexpr uint16_t BOOT_HALF_BLINK_GAP  = 300;   // 半睁眨眼间隔（毫秒）
    static constexpr uint16_t BOOT_TO_FULL_MS      = 1500;  // 半睁→全睁过渡（毫秒）

    // --- 各表情专属参数 ---
    static constexpr float HAPPY_BOTTOM_CUT_RATIO = 0.625f; // Happy底部裁切比例
    static constexpr float ANGRY_HEIGHT_RATIO     = 0.7f;   // Angry可见高度比例（从顶部裁）
    static constexpr float ANGRY_SLOPE_RATIO      = 0.25f;  // Angry斜线垂直跨度比例
    static constexpr int16_t SAD_DOWN_OFFSET      = 12;     // Sad整体下移像素
    static constexpr float SAD_SLOPE_HIGH_RATIO   = 0.5f;   // Sad斜线高点比例（外侧）
    static constexpr float SAD_SLOPE_LOW_RATIO    = 0.75f;  // Sad斜线低点比例（内侧）
    static constexpr float SLEEPY_SLOPE_HIGH_RATIO = 0.70f; // Sleepy斜线高点比例
    static constexpr float SLEEPY_SLOPE_LOW_RATIO  = 0.80f; // Sleepy斜线低点比例
    static constexpr uint8_t SURPRISED_INSET_PX   = 3;      // Surprised每眼内移像素
    static constexpr uint8_t EXCITED_MASK_RX      = 25;     // Excited椭圆水平半径
    static constexpr uint8_t EXCITED_MASK_RY      = 12;     // Excited椭圆垂直半径
    static constexpr uint8_t SCARED_SLOPE_PX      = 5;      // Scared斜线垂直跨度（像素）
    static constexpr int16_t SCARED_UP_OFFSET     = 6;      // Scared整体上移像素
    static constexpr float FOCUSED_HEIGHT_RATIO   = 0.5f;   // Focused高度比例
    static constexpr float DESPAIR_HEIGHT_RATIO   = 0.5f;   // Despair可见高度比例
    static constexpr float ALERT_WIDTH_RATIO      = 0.25f;  // Alert宽度比例
    static constexpr int16_t ALERT_HEIGHT_ADD     = 4;      // Alert高度增加量（像素）
    static constexpr int16_t BLINK_DOWN_OFFSET    = 10;     // Blink_Down下移像素
    static constexpr int16_t BLINK_UP_OFFSET      = 10;     // Blink_Up上移像素
    static constexpr uint8_t DISORIENTED_W        = 30;     // Disoriented单眼宽度
    static constexpr uint8_t DISORIENTED_H        = 30;     // Disoriented单眼高度
    static constexpr uint8_t DISORIENTED_GAP_ADD  = 10;     // Disoriented间距增加量
    static constexpr float FURIOUS_HEIGHT_RATIO   = (2.0f/3.0f); // Furious可见高度比例
    static constexpr int16_t LOOK_DOWN_OFFSET     = 10;     // Look_Down下移像素
    static constexpr int16_t LOOK_UP_OFFSET       = 10;     // Look_Up上移像素
    static constexpr uint8_t WORRIED_CORNER_RADIUS = 8;     // Worried圆角半径

    // --- 电池图标参数 ---
    static constexpr uint8_t BATTERY_GRID_COUNT = 6;   // 内部分格数
    static constexpr uint8_t BATTERY_W          = 44;  // 总宽度（含正极凸起）
    static constexpr uint8_t BATTERY_H          = 20;  // 高度
    static constexpr uint8_t BATTERY_TERM_W     = 4;   // 正极凸起宽度
    static constexpr uint8_t BATTERY_TERM_H     = 8;   // 正极凸起高度

    // --- 调试开关（true=启用）---
    static constexpr bool DEBUG_SKIP_BOOT          = false; // 跳过开机动画
    static constexpr bool DEBUG_FIXED_BLINK        = false; // 眨眼间隔固定5秒
    static constexpr bool DEBUG_DISABLE_GAZE       = false; // 禁用随机瞟眼
    static constexpr bool DEBUG_DISABLE_DRIFT      = false; // 禁用随机漂移
    static constexpr bool DEBUG_DISABLE_HALF_BLINK = false; // 禁用半眯眼
    static constexpr bool DEBUG_SHOW_GRID          = false; // 显示16px定位网格

    
    //  公有方法

    // 构造：传入已初始化的 U8g2 对象指针
    explicit RobotEye(U8G2* u8g2);

    // 初始化（在 setup() 中调用一次）
    // seed：随机种子，传0则自动选择平台最佳方案
    //        （ESP32用硬件真随机数，其他用millis()）
    //        传非0值则使用用户指定的种子
    void begin(uint32_t seed = 0);

    // 主循环更新（在 loop() 中每帧调用，建议 ≥30fps）
    // 内部自动推进所有状态机并渲染一帧
    void update();

    // 设置表情（morph 平滑切换，不眨眼）
    void setExpression(Expression expr);

    // 设置表情（可选择切换方式）
    // useBlink=true ：先闭眼→换表情→睁眼时渐变显现
    // useBlink=false：直接矢量插值 morph 过渡
    void setExpression(Expression expr, bool useBlink);

    // 设置电池电量 0~100（仅 EXPR_BATTERY 时生效）
    void setBatteryLevel(uint8_t percent);

    // 手动触发一次眨眼
    void triggerBlink();

    // 获取当前稳定表情（过渡中返回过渡前的表情）
    Expression getCurrentExpression() const;

    // 是否忙（morph过渡/眨眼切换/瞟眼中）
    // 注意：半眯眼不算忙，可被 setExpression 随时打断
    bool isBusy() const;

    // 替换表情生成函数（自定义表情）
    // id：表情枚举值（内置表情或 EXPR_USER_1~8）
    // gen：生成函数指针，传 nullptr 恢复为默认（normal脸）
    //
    // 使用示例：
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
    //  内部状态机枚举
    // ========================================================
    enum BootPhase      : uint8_t { BOOT_CLOSE, BOOT_TO_HALF, BOOT_HALF_BLINK, BOOT_TO_FULL, BOOT_IDLE };
    enum BlinkState     : uint8_t { BLK_IDLE, BLK_CLOSING, BLK_HOLD, BLK_OPENING };
    enum GazeState      : uint8_t { GZ_IDLE, GZ_TO_DIR, GZ_HOLDING, GZ_TO_CENTER };
    enum HalfBlinkState : uint8_t { HB_IDLE, HB_SHRINKING, HB_EXPANDING };
    enum TransState     : uint8_t { TR_IDLE, TR_MORPH, TR_BLINK_SWITCH };

    // 当前帧渲染模式（计算层设置，绘制层读取）
    enum FrameMode : uint8_t {
        FRAME_EYE,       // 矢量眼睛
        FRAME_BATTERY,   // 电池图标
        FRAME_BITMAP,    // Irisoled 位图
        FRAME_NOT_SHOW   // "not showing" 占位文字
    };

    // ========================================================
    //  成员变量 - 显示与几何
    // ========================================================
    U8G2*    _u8g2;              // U8g2 驱动指针
    uint16_t _screenW;           // 实际屏幕宽度（从U8g2读取）
    uint16_t _screenH;           // 实际屏幕高度
    int16_t  _baseLeftX;         // 左眼左上角X（自动居中+偏移微调）
    int16_t  _baseRightX;        // 右眼左上角X
    int16_t  _baseY;             // 双眼左上角Y

    // ========================================================
    //  成员变量 - 表情生成器表
    // ========================================================
    RE_ExprGenerator _exprGens[EXPR_COUNT];

    // ========================================================
    //  成员变量 - 椭圆预计算查找表
    //  begin()时计算，_fillEllipse运行时查表，无sqrtf
    //  索引 y(0~EXCITED_MASK_RY) → 该行半宽像素数
    // ========================================================
    uint8_t _ellipseLut[EXCITED_MASK_RY + 1];

    // ========================================================
    //  成员变量 - 各状态机
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
    //  成员变量 - 当前帧计算结果（计算层写，绘制层读）
    //  页缓冲模式下绘制层执行多次，只读这些变量不重复计算
    // ========================================================
    FrameMode  _frameMode;          // 当前帧类型
    FaceParams _frameFace;          // 当前帧眼睛参数
    float      _frameExcitedMask;   // 当前帧椭圆遮挡比例（0~1）

    // ========================================================
    //  数学工具
    // ========================================================
    static float   _clamp01(float v);
    static float   _lerp(float a, float b, float t);
    static float   _easeInOutQuad(float t);
    static uint8_t _lerpU8(uint8_t a, uint8_t b, float t);
    static int16_t _lerpI16(int16_t a, int16_t b, float t);
    static uint8_t _minU8(uint8_t a, uint8_t b);
    static uint8_t _maxU8(uint8_t a, uint8_t b);

    // ========================================================
    //  参数插值与表情生成
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
    //  渲染 - 计算层（每帧执行一次，推进状态+计算参数）
    // ========================================================
    void _computeFrame(uint32_t now);

    // ========================================================
    //  渲染 - 绘制层（纯绘制无副作用，页缓冲模式下执行多次）
    // ========================================================
    void _renderBuffer();        // 选择全帧/页缓冲方式调用_drawFrame
    void _drawFrame();           // 绘制一帧内容到缓冲
    void _drawEye(const EyeParams& p);
    void _drawFace(const FaceParams& face);
    void _fillEllipse(int cx, int cy, int rx, int ry, float mask);
    void _drawBattery(uint8_t level);
    void _drawNotShowing();
#ifdef RE_ENABLE_BITMAP_EXPR
    void _drawBitmapIrisoled(const uint8_t* bitmap);
#endif

    // ========================================================
    //  表情类型判断
    // ========================================================
    bool _isSpecialExpr(Expression expr) const;
    bool _isBitmapExpr(Expression expr) const;

    // ========================================================
    //  状态机更新
    // ========================================================
    void _updateBoot(uint32_t now);
    void _updateBlink(uint32_t now);
    void _updateHalfBlink(uint32_t now);
    void _updateGaze(uint32_t now);
    void _updateDrift(uint32_t now);
    void _updateTransition(uint32_t now);

    // ========================================================
    //  工具
    // ========================================================
    void _startBlink();
    uint32_t _randBlinkInterval();
    void _reset();
};

#endif // ROBOT_EYE_HPP
