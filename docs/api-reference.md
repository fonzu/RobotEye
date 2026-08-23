# API Reference

## Class: `RobotEye`

### Constructor

```cpp
explicit RobotEye(U8G2* u8g2);
```

Creates a RobotEye instance bound to an initialized U8g2 object. The U8g2 object must outlive the RobotEye instance.

**Parameters:**
- `u8g2` — pointer to a U8g2 driver object (e.g. `U8G2_SH1106_128X64_NONAME_F_HW_I2C`)

---

### `begin()`

```cpp
void begin(uint32_t seed = 0);
```

Initializes the library. Call once inside `setup()`, after `u8g2.begin()`.

**What it does:**
1. Reads actual screen width/height from U8g2 (falls back to 128×64 if zero)
2. Auto-centers eyes based on screen size, applies 4-direction offset
3. Registers all 24 built-in expression generators
4. Precomputes the 13-entry ellipse lookup table
5. Seeds the random number generator

**Parameters:**
- `seed` — random seed:
  - `0` (default): auto-select platform method
    - ESP32: `esp_random()` (hardware true random)
    - ESP8266: `ESP.getCycleCount()` (CPU cycle counter)
    - others: `millis()` + 16 warm-up `random()` calls
  - non-zero: use this exact seed (reproducible random sequence, useful for debugging)

---

### `update()`

```cpp
void update();
```

Main loop entry point. Call every frame inside `loop()`. Do not use `delay()` in the same loop.

**What it does (per call):**
1. Advances all 5 state machines (boot, transition, gaze, drift, half-blink, blink)
2. Computes frame parameters once (`_computeFrame`)
3. Renders to screen (`_renderBuffer`)

---

### `setExpression()`

```cpp
void setExpression(Expression expr);
void setExpression(Expression expr, bool useBlink);
```

Switches to a new expression.

**Parameters:**
- `expr` — target expression (see [Expression enum](#expression-enum))
- `useBlink`:
  - `false` (default): smooth vector morph interpolation over `MORPH_TRANS_MS` (500 ms)
  - `true`: close eyes → swap at closed point → fade in new expression while opening

**Behavior notes:**
- Calling with the same expression while idle is a no-op
- Any call immediately interrupts half-blink (half-blink is never blocking)
- Bitmap special expressions (Warning / Signal / Mode) switch instantly — they cannot morph with vector expressions
- If `expr >= EXPR_COUNT`, the call is ignored

---

### `setBatteryLevel()`

```cpp
void setBatteryLevel(uint8_t percent);
```

Sets the battery percentage for `EXPR_BATTERY`. Only visible when the current expression is battery.

**Parameters:**
- `percent` — 0–100 (values > 100 are clamped to 100)

The battery has `BATTERY_GRID_COUNT` (6) segments. Lit segments = `round(percent × 6 / 100)`.

---

### `triggerBlink()`

```cpp
void triggerBlink();
```

Manually triggers one blink. Only works when the blink state machine is idle (`BLK_IDLE`).

---

### `getCurrentExpression()`

```cpp
Expression getCurrentExpression() const;
```

Returns the current stable expression. During a morph transition, this returns the *from* expression (not the in-progress target).

---

### `isBusy()`

```cpp
bool isBusy() const;
```

Returns `true` if a morph transition, blink-switch, or gaze animation is in progress.

**Note:** half-blink is **not** considered busy — it can be interrupted at any time by `setExpression()`.

---

### `setExprGenerator()`

```cpp
void setExprGenerator(Expression id, RE_ExprGenerator gen);
```

Registers a custom expression generator function. Can replace any built-in expression or fill a user slot.

**Parameters:**
- `id` — expression enum value (built-in or `EXPR_USER_1` … `EXPR_USER_8`)
- `gen` — function pointer of type `RE_ExprGenerator`, or `nullptr` to restore default (renders normal face)

See [Custom Expressions](custom-expressions.md) for a full tutorial.

---

## Types

### `Expression` (enum)

```cpp
enum Expression : uint8_t {
    // 24 vector eye expressions
    EXPR_NORMAL, EXPR_ALERT, EXPR_ANGRY, EXPR_BLINK,
    EXPR_BLINK_DOWN, EXPR_BLINK_UP, EXPR_BORED, EXPR_DESPAIR,
    EXPR_DISORIENTED, EXPR_EXCITED, EXPR_FOCUSED, EXPR_FURIOUS,
    EXPR_HAPPY, EXPR_LOOK_DOWN, EXPR_LOOK_LEFT, EXPR_LOOK_RIGHT,
    EXPR_LOOK_UP, EXPR_SAD, EXPR_SCARED, EXPR_SLEEPY,
    EXPR_SURPRISED, EXPR_WINK_LEFT, EXPR_WINK_RIGHT, EXPR_WORRIED,
    // 5 special expressions
    EXPR_BATTERY, EXPR_WARNING, EXPR_LEFT_SIGNAL,
    EXPR_RIGHT_SIGNAL, EXPR_MODE,
    // 8 user slots
    EXPR_USER_1, EXPR_USER_2, EXPR_USER_3, EXPR_USER_4,
    EXPR_USER_5, EXPR_USER_6, EXPR_USER_7, EXPR_USER_8,
    EXPR_COUNT  // not a usable expression
};
```

See [Expressions](expressions.md) for visual descriptions.

### `EyeParams` (struct)

The minimum unit of vector drawing — describes one eye.

```cpp
struct EyeParams {
    int16_t x, y;           // top-left coordinate
    uint8_t w, h;           // width, height
    uint8_t r;              // rounded corner radius
    uint8_t topCut;         // horizontal cut from top (px)
    uint8_t bottomCut;      // horizontal cut from bottom (px)
    uint8_t topSlopeL;      // top-left slope cut depth (px)
    uint8_t topSlopeR;      // top-right slope cut depth (px)
    uint8_t bottomSlopeL;   // reserved (unused)
    uint8_t bottomSlopeR;   // reserved (unused)
};
```

All fields are linearly interpolated during morph transitions.

### `FaceParams` (struct)

A pair of eyes.

```cpp
struct FaceParams {
    EyeParams left;
    EyeParams right;
};
```

### `RE_ExprGenerator` (typedef)

```cpp
typedef FaceParams (*RE_ExprGenerator)(const FaceParams& base, int16_t lcy, int16_t rcy);
```

Function pointer type for expression generators.

**Parameters:**
- `base` — base face with drift and offset already applied
- `lcy` — center Y of left eye
- `rcy` — center Y of right eye

**Returns:** the `FaceParams` for this expression.

---

## Build Macros

These are `#define` in `robot_eye.hpp`, not class members.

| Macro | Default | Description |
|-------|---------|-------------|
| `RE_ENABLE_BITMAP_EXPR` | defined | Enables Warning/Signal/Mode bitmap expressions (requires Irisoled). Comment out to remove Irisoled dependency. |
| `RE_PAGE_BUFFER` | commented | Uncomment to use page buffer mode (128 B RAM, slower). Default is full framebuffer (1024 B, fast). |

See [Configuration](configuration.md) for all tunable parameters.
