# Architecture

This document describes the internal design of RobotEye.

---

## High-Level Structure

```
update()
  ├── _updateBoot()         // boot animation state machine
  ├── _updateTransition()   // morph transition state machine
  ├── _updateGaze()         // random glancing state machine
  ├── _updateDrift()        // random position drift
  ├── _updateHalfBlink()    // idle squint state machine
  ├── _updateBlink()        // blink state machine
  ├── _computeFrame()       // CALCULATION LAYER: compute this frame's params
  └── _renderBuffer()       // RENDER LAYER: draw to screen
        └── _drawFrame()
```

---

## Calculation / Render Split

The key architectural decision is separating **calculation** from **rendering**.

### `_computeFrame(uint32_t now)`

Runs **once per frame**. Advances all interpolation math and stores results in three member variables:

- `_frameMode` — what to draw (`FRAME_EYE`, `FRAME_BATTERY`, `FRAME_BITMAP`, `FRAME_NOT_SHOW`)
- `_frameFace` — the `FaceParams` for this frame
- `_frameExcitedMask` — ellipse mask scale (0–1) for excited expression

This function has **no drawing calls** and **no side effects** beyond writing those three variables.

### `_drawFrame()`

Runs **once** (full buffer mode) or **8 times** (page buffer mode). Reads only `_frameMode`, `_frameFace`, `_frameExcitedMask`, and `_currentExpr`. It must not modify any state.

### Why Split?

Page buffer mode requires drawing the same frame 8 times (once per 8px-tall page). If calculation were mixed with rendering, state machines would advance 8× per frame, and random numbers would differ between passes. By computing once and rendering from cached values, both modes share identical logic.

---

## State Machines

### 1. Boot (`_bootPhase`)

```
BOOT_CLOSE → BOOT_TO_HALF → BOOT_HALF_BLINK → BOOT_TO_FULL → BOOT_IDLE
  (500ms)      (1500ms)       (3 blinks)        (1500ms)
```

- `BOOT_CLOSE`: eyes are a 2px line at offset +15
- `BOOT_TO_HALF`: interpolate from closed to 70% height
- `BOOT_HALF_BLINK`: blink 3 times at half-open height (blinks close to minimum, not fully closed)
- `BOOT_TO_FULL`: interpolate from half-open to full NORMAL
- `BOOT_IDLE`: normal operation, all other state machines active

### 2. Blink (`_blinkState`)

```
BLK_IDLE → BLK_CLOSING → BLK_HOLD → BLK_OPENING → BLK_IDLE
```

- Closing: `BLINK_CLOSE_MS` (200ms), height compresses, width expands
- Hold: random `BLINK_HOLD_MIN_MS`–`BLINK_HOLD_MAX_MS` (30–60ms)
- Opening: `BLINK_OPEN_MS` (100ms)

If a blink-switch transition is active (`TR_BLINK_SWITCH`), the expression swap happens at the end of opening — the new expression fades in during the opening phase.

### 3. Half-Blink (`_halfBlinkState`)

```
HB_IDLE → HB_SHRINKING → HB_EXPANDING → HB_IDLE
```

Slow height micro-shrink (no width expansion). Only triggers in idle NORMAL. Can be interrupted by `setExpression()` at any time.

### 4. Gaze (`_gazeState`)

```
GZ_IDLE → GZ_TO_DIR → GZ_HOLDING → GZ_TO_CENTER → GZ_IDLE
```

Randomly glances up/down/left/right, holds, then returns to center. Only active when `currentExpr == EXPR_NORMAL`.

### 5. Transition (`_transState`)

- `TR_IDLE`: no transition
- `TR_MORPH`: smooth vector interpolation over `MORPH_TRANS_MS`
- `TR_BLINK_SWITCH`: expression swaps during a blink (close → swap at closed → fade in while opening)

---

## Expression Generation

### Generator Table

```cpp
RE_ExprGenerator _exprGens[EXPR_COUNT];
```

An array of function pointers, one per expression slot. Built-in expressions are registered in `_registerBuiltins()`. User expressions are added via `setExprGenerator()`.

### `_getExprFace(Expression id)`

1. Computes the base face (`_getBaseFace()`) — centered coordinates + drift
2. Calculates `lcy` / `rcy` (eye center Y)
3. Calls `_exprGens[id](base, lcy, rcy)`
4. If generator is `nullptr`, returns the base face (NORMAL)

### Built-in Generators

All 24 built-in generators live in an anonymous namespace in `robot_eye.cpp` (e.g. `re_gen_happy`, `re_gen_angry`). They are not exposed in the header.

---

## Interpolation

### `_lerpEye(a, b, t)` / `_lerpFace(a, b, t)`

Every field of `EyeParams` is linearly interpolated:

```
result = a + (b - a) × t
```

`x`, `y` use `int16_t` interpolation; `w`, `h`, `r`, cuts, slopes use `uint8_t` interpolation.

### Easing

All transitions use `_easeInOutQuad(t)`:

```
t < 0.5:  2t²
t ≥ 0.5:  1 - (-2t+2)² / 2
```

This gives smooth acceleration and deceleration.

### Excited Mask Interpolation

The excited ellipse mask is not part of `EyeParams`. It is tracked separately as `_frameExcitedMask` (0–1):
- Morphing **to** excited: mask grows from 0 → 1, face morphs to happy shape
- Morphing **from** excited: mask shrinks from 1 → 0, face morphs from happy to target

---

## Blink Transform (`_applyBlink`)

Applied after expression generation, before rendering.

For each eye:
- `hScale = 1 - amount × (1 - BLINK_MIN_H_RATIO)`
- New height = `old.h × hScale` (minimum 2px)
- Y re-centered: `y = centerY - newH / 2`
- Width expands: `newW = old.w + amount × BLINK_EXPAND_PX × 2`
- X shifts: `x = old.x - amount × BLINK_EXPAND_PX`
- All cuts, slopes, radius scale with `hScale`

`amount` ranges 0 (open) to 1 (fully closed).

---

## Ellipse Lookup Table

The excited expression uses a black ellipse mask over each eye. Computing `sqrtf()` per pixel per frame is expensive on MCUs without FPU.

### Precomputation (in `begin()`)

```cpp
for y = 0 to EXCITED_MASK_RY:
    ratio = 1 - y² / RY²
    _ellipseLut[y] = RX × sqrt(ratio)
```

This creates a 13-entry table (`EXCITED_MASK_RY + 1` = 12 + 1 = 13). Entry `y` stores the half-width of the ellipse at that vertical offset.

### Runtime (`_fillEllipse`)

For each row `y` of the ellipse:
1. Map `y` to lookup index: `lutY = |y| × RY / ry`
2. Look up full half-width: `_ellipseLut[lutY]`
3. Scale by mask: `x = lutValue × mask`
4. Draw `drawBox(cx - x, cy + y, 2x, 1)`

No `sqrtf` at runtime — only integer multiply/divide and a float scale by `mask`.

---

## Bitmap Parsing (`_drawBitmapIrisoled`)

Irisoled bitmaps are 128×64 horizontal XBM: 16 bytes per row, MSB on the left, stored in PROGMEM.

U8g2's `drawXBM()` expects a different bit orientation, so RobotEye parses manually:

```
for each row y:
    for each byte (0–15):
        byte = PROGMEM read
        if byte == 0x00: skip (all black)
        if byte == 0xFF: drawBox(x, y, 8, 1) (all white, batch)
        else: drawPixel for each set bit
```

This optimization skips fully-black bytes and batches fully-white bytes, only doing per-pixel rendering for mixed bytes.

On AVR, `RE_PGM_READ_BYTE` maps to `pgm_read_byte()`. On other platforms, it's a direct pointer dereference.

---

## Platform Compatibility

### Random Seed

```cpp
if (seed != 0) {
    randomSeed(seed);              // user-specified
} else {
    #if defined(ESP32)
        randomSeed(esp_random());  // hardware true random
    #elif defined(ESP8266)
        randomSeed(ESP.getCycleCount());
    #else
        randomSeed(millis());      // fallback
        for (i=0; i<16; i++) random();  // warm-up
    #endif
}
```

### PROGMEM

```cpp
#if defined(__AVR__)
    #include <avr/pgmspace.h>
    #define RE_PGM_READ_BYTE(addr) pgm_read_byte(addr)
#else
    #define RE_PGM_READ_BYTE(addr) (*(const uint8_t*)(addr))
#endif
```

### U8g2 Dependency

The library only uses these U8g2 methods:
- `getDisplayWidth()`, `getDisplayHeight()`
- `setDrawColor()`, `drawRBox()`, `drawBox()`, `drawPixel()`
- `drawLine()`, `setFont()`, `drawStr()`, `getStrWidth()`
- `clearBuffer()`, `sendBuffer()` (full buffer)
- `firstPage()`, `nextPage()` (page buffer)

No protocol-specific calls. Any U8g2-supported display (I2C, SPI, parallel) works.
