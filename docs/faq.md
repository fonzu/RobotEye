# FAQ

## General

### Does this library work with SPI displays?

Yes. RobotEye only uses U8g2's generic drawing API — it does not care about the bus protocol. Just change your U8g2 driver object from I2C to SPI (e.g. `U8G2_SH1106_128X64_NONAME_F_4W_HW_SPI`). The library code needs zero changes.

### Does it work with SSD1306 displays?

Yes. Any display supported by U8g2 works. The screen size is auto-detected in `begin()`.

### My screen is not 128×64. Will it work?

Yes. `begin()` reads the actual dimensions from U8g2 and auto-centers the eyes. For very small screens, you may want to reduce `BASE_EYE_W` / `BASE_EYE_H`.

### Can I use this on AVR (Arduino Uno / Nano)?

Yes, but you should enable page buffer mode to save RAM:

```cpp
// in RobotEye.h:
#define RE_PAGE_BUFFER
```

This reduces RAM usage from 1024 bytes to 128 bytes (for 128×64), at the cost of slower refresh.

### What MCUs are supported?

ESP32, ESP8266, AVR (ATmega328P etc.), STM32, and any platform with an Arduino core and U8g2 support.

---

## Behavior

### Why does `setExpression()` sometimes not switch immediately?

- **Morph mode** (`useBlink=false`): takes 500ms to interpolate. The expression changes gradually.
- **Blink mode** (`useBlink=true`): waits for the eye to close (200ms), then fades in the new expression while opening (100ms). Total ~300ms.
- **Bitmap expressions**: switch instantly (cannot morph with vector).

Use `isBusy()` to check if a transition is in progress.

### Why does my expression only show once and then freeze?

Check if you are calling `setExpression()` every frame with the same expression. The library ignores redundant calls when idle (`expr == _currentExpr && _transState == TR_IDLE`). Call it once when you want to switch, not every frame.

### How do I disable the boot animation?

Set `DEBUG_SKIP_BOOT = true` in `robot_eye.hpp`.

### How do I disable random behavior for testing?

Use the debug switches in `robot_eye.hpp`:

```cpp
static constexpr bool DEBUG_FIXED_BLINK        = true;  // fixed 5s blink interval
static constexpr bool DEBUG_DISABLE_GAZE       = true;  // no random glancing
static constexpr bool DEBUG_DISABLE_DRIFT      = true;  // no position drift
static constexpr bool DEBUG_DISABLE_HALF_BLINK = true;  // no half-blink squint
```

### The eyes look off-center. How do I adjust?

Use the 4-direction offset constants:

```cpp
static constexpr int16_t OFFSET_LEFT  = 0;  // shift left
static constexpr int16_t OFFSET_RIGHT = 0;  // shift right
static constexpr int16_t OFFSET_UP    = 0;  // shift up
static constexpr int16_t OFFSET_DOWN  = 0;  // shift down
```

For example, to shift eyes 2px left: `OFFSET_LEFT = 2`.

---

## Performance

### Is floating-point math a problem on MCUs without FPU?

Most float operations are simple multiply/add for interpolation, which compilers handle reasonably well. The expensive operation (`sqrtf` for the ellipse) is precomputed once in `begin()` into a 13-entry lookup table. Runtime ellipse drawing uses only integer math + one float scale.

If you need maximum performance on AVR, consider reducing the frame rate or enabling page buffer mode.

### How much RAM does this use?

- **Full buffer mode**: `width × height / 8` bytes (1024 B for 128×64) + ~200 B for library state
- **Page buffer mode**: `width` bytes (128 B for 128×64) + ~200 B for library state

### How much flash (program memory)?

Approximately 8–12 KB depending on which expressions are used. Bitmap expressions add the Irisoled bitmap data (~1 KB each if enabled).

---

## Custom Expressions

### My custom expression shows nothing or the wrong thing.

Check:
1. Did you call `setExprGenerator()` **after** `begin()`? (Built-ins are registered in `begin()`, so register after.)
2. Is your function signature exactly `FaceParams func(const FaceParams&, int16_t, int16_t)`?
3. Are you modifying a **copy** of `base` and returning it? (Don't modify `base` in place — it's const.)

### Can I animate parameters inside my custom expression?

Generators are called once per frame during calculation. You can use `millis()` inside your generator for time-based animation, but be aware that the generator is also called during morph transitions (for interpolation endpoints). For most cases, static geometry + the library's built-in blink/drift is sufficient.

### Can I replace a built-in expression permanently?

Yes. Call `setExprGenerator(RobotEye::EXPR_HAPPY, myFunc)` after `begin()`. Pass `nullptr` to restore the default.

---

## Bitmap Expressions

### I get a compile error about `Irisoled.h`.

Either install the Irisoled library, or comment out `RE_ENABLE_BITMAP_EXPR` in `robot_eye.hpp`. The 4 bitmap expressions (Warning / Left Signal / Right Signal / Mode) will show "not showing" text instead.

### Can I use my own bitmaps instead of Irisoled?

Currently the bitmap expressions are hardcoded to use Irisoled's bitmap arrays. To use custom bitmaps, you would need to modify `_drawFrame()` in `robot_eye.cpp` to point to your own bitmap data. The `_drawBitmapIrisoled()` function accepts any `const uint8_t*` in horizontal XBM format (16 bytes per row, MSB-left).

---

## Troubleshooting

### The screen is blank after upload.

1. Check I2C address (common: 0x3C or 0x3D)
2. Check SDA/SCL pins match your board
3. Verify `u8g2.begin()` is called before `eye.begin()`
4. Try `u8g2.setContrast(200)` — some displays need higher contrast

### The boot animation plays but then nothing happens.

This is normal if you haven't called `setExpression()`. After boot, the eyes enter NORMAL idle with random blinking, glancing, and drifting. If you disabled all random behavior with debug flags, the eyes will just stay open and blink occasionally.

### Blinking looks jittery.

Make sure you are not calling `delay()` in your loop. `update()` must be called at a consistent rate (≥30 FPS recommended).

### The excited expression looks wrong.

Check `EXCITED_MASK_RX` (default 25) and `EXCITED_MASK_RY` (default 12). The ellipse is drawn in black over each eye center. If RX is too large relative to the eye, it may mask the entire eye.
