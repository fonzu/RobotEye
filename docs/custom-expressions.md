# Custom Expressions

RobotEye supports 8 user expression slots (`EXPR_USER_1` … `EXPR_USER_8`) and also allows replacing any built-in expression. This is done via **expression generator functions**.

---

## How It Works

Every expression — including all 24 built-ins — is produced by a generator function with this signature:

```cpp
FaceParams myGenerator(const FaceParams& base, int16_t lcy, int16_t rcy);
```

**Parameters:**
- `base` — the base face (NORMAL geometry + drift + global offset already applied). Copy and modify this.
- `lcy` — center Y coordinate of the left eye
- `rcy` — center Y coordinate of the right eye

**Returns:** a `FaceParams` describing the left and right eye for this expression.

The library stores function pointers in an internal table (`_exprGens[]`). When rendering, it calls the registered generator for the current expression.

---

## EyeParams Fields

```cpp
struct EyeParams {
    int16_t x, y;           // top-left corner
    uint8_t w, h;           // width, height
    uint8_t r;              // corner radius
    uint8_t topCut;         // cut N pixels from top (horizontal)
    uint8_t bottomCut;      // cut N pixels from bottom (horizontal)
    uint8_t topSlopeL;      // top-left slope cut depth
    uint8_t topSlopeR;      // top-right slope cut depth
    uint8_t bottomSlopeL;   // reserved (unused)
    uint8_t bottomSlopeR;   // reserved (unused)
};
```

### Drawing Order (internal)

For each eye, the renderer:
1. Draws a white rounded rectangle (`x, y, w, h, r`)
2. Cuts `topCut` pixels from the top (black rectangle)
3. Cuts `bottomCut` pixels from the bottom (black rectangle)
4. Cuts the top slope using `topSlopeL` / `topSlopeR` (per-line precise black boxes)

The slope works as follows:
- `mn = min(topSlopeL, topSlopeR)`: first `mn` rows are fully black
- `mx = max(topSlopeL, topSlopeR)`: rows `mn` to `mx` form a triangle
- If `topSlopeR > topSlopeL`: triangle on the right (high-left, low-right)
- If `topSlopeL > topSlopeR`: triangle on the left (low-left, high-right)

---

## Example 1: Simple Custom Expression

A "squint" expression that cuts both top and bottom:

```cpp
#include <RobotEye.h>

FaceParams mySquint(const FaceParams& base, int16_t, int16_t) {
    FaceParams f = base;
    f.left.topCut    = f.right.topCut    = 8;   // cut 8px from top
    f.left.bottomCut = f.right.bottomCut = 8;   // cut 8px from bottom
    return f;
}
```

Register and use:

```cpp
void setup() {
    u8g2.begin();
    eye.begin();
    eye.setExprGenerator(RobotEye::EXPR_USER_1, mySquint);
}

void loop() {
    eye.update();
    eye.setExpression(RobotEye::EXPR_USER_1);  // smooth morph to your expression
}
```

---

## Example 2: Asymmetric Expression

One eye normal, one eye a thin line (custom wink):

```cpp
FaceParams myWink(const FaceParams& base, int16_t lcy, int16_t) {
    FaceParams f = base;
    // Left eye: 4px horizontal line centered
    f.left.h = 4;
    f.left.r = 2;
    f.left.y = lcy - 2;
    // Right eye: unchanged (from base)
    return f;
}
```

---

## Example 3: Centered Height Change

When changing height, keep the eye centered by using `lcy` / `rcy`:

```cpp
FaceParams myTall(const FaceParams& base, int16_t lcy, int16_t rcy) {
    FaceParams f = base;
    uint8_t newH = 40;
    f.left.h  = f.right.h = newH;
    f.left.y  = lcy - newH / 2;   // re-center vertically
    f.right.y = rcy - newH / 2;
    f.left.r  = f.right.r = 12;   // adjust radius proportionally
    return f;
}
```

---

## Example 4: Replacing a Built-in

You can override any built-in expression. For example, make HAPPY use a different cut:

```cpp
FaceParams myHappy(const FaceParams& base, int16_t, int16_t) {
    FaceParams f = base;
    f.left.bottomCut = f.right.bottomCut = 16;  // deeper cut than default 20
    return f;
}

void setup() {
    eye.begin();
    eye.setExprGenerator(RobotEye::EXPR_HAPPY, myHappy);  // override built-in
}
```

To restore the default, pass `nullptr`:

```cpp
eye.setExprGenerator(RobotEye::EXPR_HAPPY, nullptr);  // restore built-in
```

---

## Tips

1. **Always start from `base`** — it contains the correct screen-centered coordinates and current drift.
2. **Use `lcy` / `rcy`** when changing height to keep eyes centered.
3. **All fields interpolate** — during morph transitions, every field of `EyeParams` linearly interpolates. Design your expression so intermediate values look natural.
4. **Slope values are absolute pixels** — not ratios. If you scale the eye height, also scale the slope if you want the same visual angle.
5. **`r` is clamped** — the renderer clamps `r` to `min(w/2, h/2)` automatically.
6. **User slots default to normal** — unregistered slots render the base face (NORMAL).
7. **No dynamic allocation** — generators are pure functions, called once per frame during the calculation phase.

---

## Testing

Use the `DEBUG_SKIP_BOOT` flag to skip the boot animation and test your expression immediately:

```cpp
// in robot_eye.hpp:
static constexpr bool DEBUG_SKIP_BOOT = true;
```

Then in loop:

```cpp
void loop() {
    eye.update();
    if (millis() > 1000) eye.setExpression(RobotEye::EXPR_USER_1);
}
```
