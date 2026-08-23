# Configuration

All tunable parameters are `static constexpr` inside the `RobotEye` class. They are compile-time constants — no RAM consumption, type-checked, and do not pollute the global namespace.

To change a value, edit `robot_eye.hpp` and recompile.

---

## Build Macros

These are `#define` at the top of `robot_eye.hpp`, not class members.

### `RE_ENABLE_BITMAP_EXPR`

```cpp
#define RE_ENABLE_BITMAP_EXPR   // defined by default
```

- **Defined**: Warning / Left Signal / Right Signal / Mode expressions use Irisoled bitmaps (requires Irisoled library)
- **Commented out**: No Irisoled dependency. Those 4 expressions show "not showing" text.

### `RE_PAGE_BUFFER`

```cpp
// #define RE_PAGE_BUFFER   // commented by default
```

- **Commented (default)**: Full framebuffer mode. Uses `width × height / 8` bytes RAM (1024 B for 128×64). Fast — clear, draw, transmit once per frame.
- **Uncommented**: Page buffer mode. Uses `width` bytes RAM (128 B for 128×64). Slower — draws 8 passes per frame (one page each). Suitable for low-RAM MCUs like AVR.

The calculation layer (`_computeFrame`) runs once per frame regardless of mode. The draw layer (`_drawFrame`) runs multiple times in page mode but reads only precomputed values — no state changes.

---

## Screen & Base Geometry

### Screen Fallback

| Constant | Default | Description |
|----------|---------|-------------|
| `SCREEN_W_DEFAULT` | 128 | Fallback width if U8g2 returns 0 |
| `SCREEN_H_DEFAULT` | 64 | Fallback height if U8g2 returns 0 |
| `BITMAP_W` | 128 | Fixed width for Irisoled bitmaps |
| `BITMAP_H` | 64 | Fixed height for Irisoled bitmaps |

Actual screen size is read from U8g2 in `begin()` via `getDisplayWidth()` / `getDisplayHeight()`.

### Base Eye Geometry

The NORMAL expression anchor. All other expressions transform from this.

| Constant | Default | Description |
|----------|---------|-------------|
| `BASE_EYE_W` | 32 | Single eye width (px) |
| `BASE_EYE_H` | 32 | Single eye height (px) |
| `BASE_EYE_R` | 10 | Single eye corner radius (px) |
| `BASE_EYE_GAP` | 8 | Gap between the two eyes (px) |

Total eye pair width = `2 × BASE_EYE_W + BASE_EYE_GAP` = 72px.

### Global Position Fine-Tune

Applied after auto-centering. Default all zero.

| Constant | Default | Description |
|----------|---------|-------------|
| `OFFSET_LEFT` | 0 | Shift eyes left (px) |
| `OFFSET_RIGHT` | 0 | Shift eyes right (px) |
| `OFFSET_UP` | 0 | Shift eyes up (px) |
| `OFFSET_DOWN` | 0 | Shift eyes down (px) |

Internal formula: `dx = OFFSET_RIGHT - OFFSET_LEFT`, `dy = OFFSET_DOWN - OFFSET_UP`.

Example: shift eyes left by 2px → set `OFFSET_LEFT = 2`.

---

## Blink System

| Constant | Default | Description |
|----------|---------|-------------|
| `BLINK_MIN_H_RATIO` | 0.06f | Remaining height ratio when fully closed (≈2px) |
| `BLINK_CLOSE_MS` | 200 | Closing animation duration (ms) |
| `BLINK_HOLD_MIN_MS` | 30 | Minimum hold at fully closed (ms) |
| `BLINK_HOLD_MAX_MS` | 60 | Maximum hold at fully closed (ms) |
| `BLINK_OPEN_MS` | 100 | Opening animation duration (ms) |
| `BLINK_EXPAND_PX` | 4 | Single-side horizontal expand when closing (total +8px) |
| `BLINK_INTERVAL_MIN` | 7000 | Minimum interval between auto-blinks (ms) |
| `BLINK_INTERVAL_MAX` | 12000 | Maximum interval between auto-blinks (ms) |

Blink is a compound transform: height compression (center Y fixed) + horizontal expansion. All `EyeParams` fields (including cuts, slopes, radius) scale with height.

**Expression-dependent blink intervals** (hardcoded in `_randBlinkInterval()`):
- NORMAL / SURPRISED: 7000–12000 ms
- ANGRY / HAPPY: 2000–6000 ms
- All others: `BLINK_INTERVAL_MIN` – `BLINK_INTERVAL_MAX`

---

## Half-Blink System (Idle Squint)

A slow, symmetrical height micro-shrink for "liveliness." No width expansion.

| Constant | Default | Description |
|----------|---------|-------------|
| `HALF_BLINK_CHANCE` | 30 | Trigger probability per check (%) |
| `HALF_BLINK_PHASE_MS` | 150 | Single phase duration (shrink 150 + expand 150 = 300ms total) |
| `HALF_BLINK_HEIGHT_RATIO` | 0.70f | Minimum height ratio after shrink |
| `HALF_BLINK_INTERVAL_MIN` | 4000 | Minimum check interval (ms) |
| `HALF_BLINK_INTERVAL_MAX` | 8000 | Maximum check interval (ms) |

Only triggers in BOOT_IDLE, on non-special expressions, when blink and transition are both idle. Can be interrupted by `setExpression()` at any time.

---

## Gaze System (Random Glancing)

NORMAL idle randomly glances in one of 4 directions, holds, then auto-returns to center.

State machine: `IDLE → TO_DIR → HOLDING → TO_CENTER → IDLE`

| Constant | Default | Description |
|----------|---------|-------------|
| `GAZE_TRANS_MS` | 600 | Direction transition duration (ms), vector interpolation |
| `GAZE_HOLD_MIN_MS` | 1000 | Minimum hold at direction (ms) |
| `GAZE_HOLD_MAX_MS` | 2000 | Maximum hold at direction (ms) |
| `GAZE_INTERVAL_MIN` | 4000 | Minimum check interval (ms) |
| `GAZE_INTERVAL_MAX` | 7000 | Maximum check interval (ms) |
| `GAZE_CHANCE` | 30 | Trigger probability per check (%) |

Only triggers when `currentExpr == EXPR_NORMAL` and no transition is active.

### Gaze Eye Morphology (Look_Left / Look_Right)

| Constant | Default | Description |
|----------|---------|-------------|
| `GAZE_EYE_OFFSET_PX` | 14 | Facing eye offset in direction (px) |
| `GAZE_EYE_SCALE` | 1.3f | Facing eye scale factor |
| `GAZE_MIN_CENTER_DIST` | 4 | Minimum center distance between eyes (px), prevents overlap |

For LOOK_LEFT: left eye offsets left 14px + scales 1.3×; right eye offsets left 14px only.
For LOOK_RIGHT: mirror.

---

## Random Drift

NORMAL eyes slowly drift within a small range for "living" feel.

| Constant | Default | Description |
|----------|---------|-------------|
| `DRIFT_AMPLITUDE` | 12 | Raw random range (±12), clamped by MAX below |
| `DRIFT_MAX_X` | 4 | Absolute X boundary (px), target clamped at generation |
| `DRIFT_MAX_Y` | 4 | Absolute Y boundary (px), target clamped at generation |
| `DRIFT_INTERVAL_MS` | 5000 | New target generation interval (ms) |
| `DRIFT_SPEED` | 0.40f | Per-frame approach ratio (0–1). Higher = faster drift |

Targets are clamped at generation time (not runtime collision check). Set `DRIFT_MAX_X = 0` and `DRIFT_MAX_Y = 0` to disable drift.

---

## Morph Transition

| Constant | Default | Description |
|----------|---------|-------------|
| `MORPH_TRANS_MS` | 500 | Total morph transition duration (ms) |

All `EyeParams` fields (x, y, w, h, r, topCut, bottomCut, topSlopeL/R, bottomSlopeL/R) interpolate linearly with `easeInOutQuad` easing.

---

## Boot Animation

Full flow: **Close(500ms) → to half-open(1500ms) → 3 blinks at half-open(300ms gap) → to full open(1500ms)** ≈ 5.5s

| Constant | Default | Description |
|----------|---------|-------------|
| `BOOT_CLOSE_MS` | 500 | Initial fully-closed duration (ms) |
| `BOOT_TO_HALF_MS` | 1500 | Closed → half-open transition (ms) |
| `BOOT_HALF_OPEN_RATIO` | 0.7f | Half-open height ratio (of normal) |
| `BOOT_HALF_BLINK_CNT` | 3 | Number of blinks at half-open |
| `BOOT_HALF_BLINK_GAP` | 300 | Gap between half-open blinks (ms) |
| `BOOT_TO_FULL_MS` | 1500 | Half-open → full-open transition (ms) |

Half-open blinks close from half-open height to minimum, then return to half-open (not fully closed).

---

## Expression-Specific Parameters

### Happy
| Constant | Default | Description |
|----------|---------|-------------|
| `HAPPY_BOTTOM_CUT_RATIO` | 0.625f | Bottom cut ratio (32×0.625=20px cut, keeps upper 12px) |

### Angry / Furious
| Constant | Default | Description |
|----------|---------|-------------|
| `ANGRY_HEIGHT_RATIO` | 0.7f | Visible height ratio (cut from top, bottom keeps normal rounded corners) |
| `ANGRY_SLOPE_RATIO` | 0.25f | Slope vertical span ratio (32×0.25=8px) |
| `FURIOUS_HEIGHT_RATIO` | 0.667f | Furious visible height ratio (2/3) |

Angry keeps full normal size (32×32, r=10) — only the top is cut with a slope. Bottom rounded corners are preserved.

### Sad
| Constant | Default | Description |
|----------|---------|-------------|
| `SAD_DOWN_OFFSET` | 12 | Overall down shift (px) |
| `SAD_SLOPE_HIGH_RATIO` | 0.5f | Slope high point ratio (outer side) |
| `SAD_SLOPE_LOW_RATIO` | 0.75f | Slope low point ratio (inner side) |

### Sleepy
| Constant | Default | Description |
|----------|---------|-------------|
| `SLEEPY_SLOPE_HIGH_RATIO` | 0.70f | Slope high point ratio |
| `SLEEPY_SLOPE_LOW_RATIO` | 0.80f | Slope low point ratio (gentler than sad) |

### Surprised
| Constant | Default | Description |
|----------|---------|-------------|
| `SURPRISED_INSET_PX` | 3 | Per-eye inward shift (px) |

### Excited
| Constant | Default | Description |
|----------|---------|-------------|
| `EXCITED_MASK_RX` | 25 | Ellipse mask horizontal radius (px) |
| `EXCITED_MASK_RY` | 12 | Ellipse mask vertical radius (px, < RX → flat ellipse) |

The ellipse is drawn in black over each eye center, masking the lower part of the happy shape to form crescent eyes. Radius participates in morph interpolation.

### Scared / Worried / Despair
| Constant | Default | Description |
|----------|---------|-------------|
| `SCARED_SLOPE_PX` | 5 | Scared slope vertical span (absolute px, not ratio) |
| `SCARED_UP_OFFSET` | 6 | Scared overall up shift (px) |
| `WORRIED_CORNER_RADIUS` | 8 | Worried corner radius (scared has r=0) |
| `DESPAIR_HEIGHT_RATIO` | 0.5f | Despair visible height ratio (slope absolute value preserved) |

### Focused
| Constant | Default | Description |
|----------|---------|-------------|
| `FOCUSED_HEIGHT_RATIO` | 0.5f | Height ratio, center unchanged, radius scaled |

### Alert
| Constant | Default | Description |
|----------|---------|-------------|
| `ALERT_WIDTH_RATIO` | 0.25f | Width ratio (32×0.25=8px) |
| `ALERT_HEIGHT_ADD` | 4 | Extra height (px), symmetric top/bottom |

### Blink Variants
| Constant | Default | Description |
|----------|---------|-------------|
| `BLINK_DOWN_OFFSET` | 10 | BLINK_DOWN down shift from center (px) |
| `BLINK_UP_OFFSET` | 10 | BLINK_UP up shift from center (px) |

### Disoriented
| Constant | Default | Description |
|----------|---------|-------------|
| `DISORIENTED_W` | 30 | Single eye width (px) |
| `DISORIENTED_H` | 30 | Single eye height (px) |
| `DISORIENTED_GAP_ADD` | 10 | Extra gap between eyes (px) |

### Look Variants
| Constant | Default | Description |
|----------|---------|-------------|
| `LOOK_DOWN_OFFSET` | 10 | LOOK_DOWN down shift (px) |
| `LOOK_UP_OFFSET` | 10 | LOOK_UP up shift (px) |

---

## Battery Icon

| Constant | Default | Description |
|----------|---------|-------------|
| `BATTERY_GRID_COUNT` | 6 | Number of internal segments |
| `BATTERY_W` | 44 | Total width including terminal (px) |
| `BATTERY_H` | 20 | Height (px) |
| `BATTERY_TERM_W` | 4 | Positive terminal bump width (px) |
| `BATTERY_TERM_H` | 8 | Positive terminal bump height (px) |

---

## Debug Switches

All default `false`. Set to `true` and recompile.

| Constant | Default | Description |
|----------|---------|-------------|
| `DEBUG_SKIP_BOOT` | false | Skip boot animation, enter NORMAL idle immediately |
| `DEBUG_FIXED_BLINK` | false | Blink interval fixed at 5000ms (no random) |
| `DEBUG_DISABLE_GAZE` | false | Disable random glancing |
| `DEBUG_DISABLE_DRIFT` | false | Disable random position drift |
| `DEBUG_DISABLE_HALF_BLINK` | false | Disable half-blink squint |
| `DEBUG_SHOW_GRID` | false | Draw 16px grid overlay for coordinate debugging |
