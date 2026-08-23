# Expressions

RobotEye has 37 expression slots total:

- **24 vector eye expressions** — drawn from geometry, fully morph-interpolatable
- **5 special expressions** — battery gauge (vector) + 4 bitmap icons (optional Irisoled)
- **8 user slots** — register your own generator functions

---

## Vector Eye Expressions (24)

All vector expressions are built by transforming the NORMAL base face. They all support smooth morph transitions.

### Base

| Enum | Description |
|------|-------------|
| `EXPR_NORMAL` | Normal open eyes. The baseline all other expressions transform from. 32×32 rounded rectangles, r=10, 8px gap. |

### Emotion

| Enum | Description | Geometry |
|------|-------------|----------|
| `EXPR_HAPPY` | Smiling eyes | Bottom cut = `BASE_EYE_H × 0.625` (20px), keeps upper curved arc |
| `EXPR_ANGRY` | Angry brows | Full normal size, top slope cut. Left eye: high-left/low-right; right eye: mirror. Bottom keeps normal rounded corners. |
| `EXPR_SAD` | Frown brows | Top slope (outer low, inner high) + overall down shift 12px |
| `EXPR_SURPRISED` | Surprised | Eyes move inward 3px each (gap shrinks by 6px) |
| `EXPR_SLEEPY` | Sleepy | Top slope gentler than sad (0.70/0.80 ratio), Y position unchanged |
| `EXPR_EXCITED` | Excited crescent | Happy shape + black ellipse mask at each eye center (RX=25, RY=12), forming crescent eyes |
| `EXPR_SCARED` | Scared | Mirrored angry slope (inner high, outer low) + up shift 6px + sharp corners (r=0) |
| `EXPR_FOCUSED` | Focused | Height × 0.5, center unchanged, radius scaled proportionally |
| `EXPR_WORRIED` | Worried | Scared slope shape + centered (no up shift) + rounded corners (r=8) |
| `EXPR_DESPAIR` | Despair | Scared shape (with up shift) + height × 0.5, slope absolute value unchanged |
| `EXPR_FURIOUS` | Furious | Same as angry but deeper cut (height × 2/3 vs angry's 0.7) |
| `EXPR_BORED` | Bored | Flat arc shifted down 20px, 14px high, top cut 7px |

### Alert / State

| Enum | Description | Geometry |
|------|-------------|----------|
| `EXPR_ALERT` | Alert narrow bar | Width × 0.25 (8px), height +4 (36px), center unchanged, r=w/2 |
| `EXPR_BLINK` | Blink line | 4px horizontal line centered at eye center |
| `EXPR_BLINK_DOWN` | Blink line low | 4px line, shifted down 10px from center |
| `EXPR_BLINK_UP` | Blink line high | 4px line, shifted up 10px from center |
| `EXPR_DISORIENTED` | Disoriented | 30×30 eyes, gap +10px, r=12 |

### Gaze / Look

| Enum | Description | Geometry |
|------|-------------|----------|
| `EXPR_LOOK_DOWN` | Look down | Both eyes shift down 10px |
| `EXPR_LOOK_UP` | Look up | Both eyes shift up 10px |
| `EXPR_LOOK_LEFT` | Look left | Left eye: offset left 14px + scale 1.3×; right eye: offset left 14px only |
| `EXPR_LOOK_RIGHT` | Look right | Right eye: offset right 14px + scale 1.3×; left eye: offset right 14px only |

### Wink

| Enum | Description | Geometry |
|------|-------------|----------|
| `EXPR_WINK_LEFT` | Left wink | Left eye = 4px line centered; right eye = normal |
| `EXPR_WINK_RIGHT` | Right wink | Right eye = 4px line centered; left eye = normal |

---

## Special Expressions (5)

### `EXPR_BATTERY` — Vector Battery Gauge

A segmented battery icon drawn with U8g2 primitives. Controlled by `setBatteryLevel(0–100)`.

- Total width: 44px (including terminal bump)
- Height: 20px
- Segments: 6
- Each segment ≈ 16.7% battery
- Lit segments = `round(percent × 6 / 100)`

### Bitmap Expressions (require `RE_ENABLE_BITMAP_EXPR` + Irisoled)

| Enum | Description |
|------|-------------|
| `EXPR_WARNING` | Warning triangle icon (Irisoled bitmap) |
| `EXPR_LEFT_SIGNAL` | Left turn signal arrow (Irisoled bitmap) |
| `EXPR_RIGHT_SIGNAL` | Right turn signal arrow (Irisoled bitmap) |
| `EXPR_MODE` | Mode/settings gear icon (Irisoled bitmap) |

These are 128×64 full-screen bitmaps stored in PROGMEM. RobotEye parses them pixel-by-pixel because U8g2's `drawXBM()` bit order is incompatible with Irisoled's horizontal XBM format.

**If `RE_ENABLE_BITMAP_EXPR` is not defined**, these expressions show the text "not showing" instead.

Bitmap expressions switch instantly (no morph) because bitmap and vector parameters cannot be interpolated.

---

## User Slots (8)

| Enum | Description |
|------|-------------|
| `EXPR_USER_1` … `EXPR_USER_8` | Empty slots for custom expressions |

By default, unregistered user slots render the normal face. Register a generator with `setExprGenerator()`.

See [Custom Expressions](custom-expressions.md) for details.

---

## Expression Switching Behavior

| Transition Type | Method | Duration | Notes |
|-----------------|--------|----------|-------|
| Morph | `setExpression(expr)` or `setExpression(expr, false)` | 500 ms | All EyeParams fields interpolate with ease-in-out |
| Blink switch | `setExpression(expr, true)` | ~300 ms | Close (200ms) → hold → open (100ms, new expression fades in) |
| Bitmap switch | any `setExpression()` to/from bitmap | instant | Cannot morph bitmap ↔ vector |

### Excited Special Case

`EXPR_EXCITED` has an extra ellipse mask that is not part of `EyeParams`. During morph transitions involving excited:
- **→ excited**: base face morphs to happy shape, ellipse mask grows from 0 to full radius
- **excited →**: base face morphs from happy to target, ellipse mask shrinks from full to 0

The mask radius participates in the interpolation, so crescent eyes form/dissolve smoothly.
