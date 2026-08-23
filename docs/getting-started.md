# Getting Started

## 1. Install Dependencies

### Arduino IDE

1. Open **Sketch → Include Library → Manage Libraries**
2. Search for **U8g2** and install it 
3. (Optional) Install **Irisoled** if you want Warning / Signal / Mode bitmap expressions

### PlatformIO

Add to `platformio.ini`:

```ini
lib_deps =
    olikraus/U8g2
    # optional:
    # https://github.com/your-username/Irisoled.git
```

## 2. Add the Library

Copy `robot_eye.hpp,RobotEye.h` and `robot_eye.cpp` into your project's `src/` folder (or into a library folder).

## 3. Hardware Setup

### I2C Display (most common)

```cpp
#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <RobotEye.h>

// Change pins to match your board
#define I2C_SDA_PIN  8
#define I2C_SCL_PIN  9

// SH1106 128x64 hardware I2C, full framebuffer
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
RobotEye eye(&u8g2);

void setup() {
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    u8g2.begin();
    u8g2.setContrast(200);
    eye.begin();
}

void loop() {
    eye.update();
}
```

### SPI Display

The library only uses U8g2's generic drawing API — it does not care about the bus protocol. Just change the U8g2 driver object:

```cpp
// 4-wire hardware SPI example
U8G2_SH1106_128X64_NONAME_F_4W_HW_SPI u8g2(U8G2_R0, /*cs=*/5, /*dc=*/16, /*reset=*/17);
```

Everything else stays the same.

### Other Display Sizes

RobotEye reads the actual screen dimensions from U8g2 in `begin()` and auto-centers the eyes. If your screen is not 128×64, no code changes are needed — the eyes will reposition automatically.

For very small screens, you may want to reduce `BASE_EYE_W` / `BASE_EYE_H` (see [Configuration](configuration.md)).

## 4. First Run

Upload the sketch. You should see:

1. **0 – 0.5 s**: Eyes fully closed (thin line)
2. **0.5 – 2.0 s**: Eyes smoothly open to 70% height (half-open)
3. **2.0 – ~3.5 s**: Three blinks at half-open height
4. **3.5 – 5.0 s**: Eyes smoothly open to full height
5. **After 5 s**: NORMAL idle — random blinking, glancing, drifting, half-blinks

## 5. Switching Expressions

```cpp
// Smooth morph transition (default, 500 ms)
eye.setExpression(RobotEye::EXPR_HAPPY);

// Blink transition: close → swap → fade in while opening
eye.setExpression(RobotEye::EXPR_ANGRY, true);

// Battery with adjustable level
eye.setExpression(RobotEye::EXPR_BATTERY);
eye.setBatteryLevel(75);   // 0–100
```

See [Expressions](expressions.md) for the full list.

## 6. Important: Call `update()` Every Frame

`update()` advances all state machines and renders one frame. Do **not** put `delay()` in your loop — it will freeze the animations. Aim for at least 30 FPS.

```cpp
void loop() {
    eye.update();
    // your other non-blocking code here
}
```

## 7. Next Steps

- Read the [API Reference](api-reference.md) for all public methods
- Browse [Expressions](expressions.md) to see what each expression looks like
- Tune parameters in [Configuration](configuration.md)
- Add your own expressions with [Custom Expressions](custom-expressions.md)
