# AVR Power Switch Controller

This project implements an AVR-style automatic transfer switch controller on an ESP32-S3 Super Mini using PlatformIO. The code monitors power inputs, drives a servo-driven switcher, and manages emergency protection and manual override behavior.

## Hardware and board

- Board: ESP32-S3 Super Mini
- Framework: Arduino / PlatformIO
- Servo output: pin 5
- Button input: pin 3
- Grid power input: pin 6
- Inverter power input: pin 9
- Main output LED: pin 8

The servo uses a 3-position model:

- index 0 = grid source
- index 1 = neutral
- index 2 = inverter source

The servo angles are defined as:

```cpp
int angles[3] = {0, 45, 80};
int gridAngleIndex = 0;
int neutralAngleIndex = 1;
int invertorAngleIndex = 2;
```

## Main logic

### 1. Startup

On boot, the sketch:

- starts serial logging
- configures the digital inputs and output LED
- allocates ESP32 PWM timers
- attaches the servo
- registers button handlers
- sets the switcher to the neutral position

```cpp
button.attachClick(handleButtonClick, &button);
button.attachDoubleClick(handleButtonDoubleClick, &button);
button.attachLongPressStop(handleButtonLongPressStop, &button);

moveSwitcherToAngle(neutralAngleIndex);
```

### 2. Manual button behavior

The control logic uses the `OneButton` library.

- Single click: advances to the next switch position
  - the actual code sequence is: 0 -> 1 -> 2 -> 0
  - the handler increments `angleIndex` and resets to `gridAngleIndex` when it exceeds 2

```cpp
void handleButtonClick(void *oneButton)
{
  angleIndex++;
  if (angleIndex >= 3)
  {
    angleIndex = gridAngleIndex;
  }

  moveSwitcherToAngle(angleIndex);
}
```

- Long press: starts the AVR process
  - sets `isAvrStarted = true`
  - no source check happens here; the AVR task handles the switching decision after that

- Double click: runs an emergency test
  - calls `checkForEmergency()`
  - if both inputs are active at the same time, the controller enters emergency mode

### 3. Automatic AVR process

The AVR logic is run in `processAvrTask()` every second.

This task only runs when `isAvrStarted` is true and emergency mode is not active.

#### If the main output is not currently on:

```cpp
if (!mainOutputPowerOn)
{
  if (isGridPowerOn())
  {
    switchOnMainOutput(gridAngleIndex, withDelay);
    return;
  }

  if (isInvertorPowerOn())
  {
    switchOnMainOutput(invertorAngleIndex, withDelay);
    return;
  }
}
```

The code prefers the grid source when the grid input is active. Otherwise it selects the inverter source if that input is active.

#### If the main output is already on:

```cpp
if (isGridPowerOn() && angleIndex != gridAngleIndex)
{
  switchOnMainOutput(gridAngleIndex, true);
  return;
}

if (!isGridPowerOn() && isInvertorPowerOn() && angleIndex != invertorAngleIndex)
{
  switchOnMainOutput(invertorAngleIndex, true);
  return;
}
```

This means:

- if grid power returns while the switch is on another position, it moves back to the grid position
- if grid power is absent and inverter power is available, it switches to the inverter position

### 4. Source detection

The sketch reads the physical inputs as active-low signals:

```cpp
bool isGridPowerOn()
{
  bool powerOn = digitalRead(gridPowerPin) == LOW;
  return powerOn;
}

bool isInvertorPowerOn()
{
  bool powerOn = digitalRead(invertorPowerPin) == LOW;
  return powerOn;
}
```

The comments note that an external pull-up resistor is expected, so `LOW` means the source is considered active.

### 5. Emergency behavior

The emergency check is designed to protect the system if both sources appear active at the same time.

```cpp
bool checkForEmergency()
{
  for (int i = 0; i < 2; i++)
  {
    moveSwitcherToAngle(i);

    if (isGridPowerOn() && isInvertorPowerOn())
    {
      Log.warning("Grid power is on and inverter power is on. Emergency situation, switcher is broken!");

      int prevIndex = i - 1;
      if (prevIndex >= 0)
      {
        moveSwitcherToAngle(prevIndex);
      }

      return true;
    }
  }

  moveSwitcherToAngle(1);
  return false;
}
```

If both inputs are high:

- the controller logs an emergency warning
- it moves to a previous safe position if possible
- sets `isEmergency = true`
- blocks further button actions and AVR processing

When emergency mode is active, the LED blinks at 500 ms intervals.

### 6. Idle startup delay

The project also contains an idle auto-start feature:

```cpp
void processIdleTask()
{
  if (currentMillis - esp32PowerOnMillis >= idleDelay)
  {
    if (!isEmergency && !isAvrStarted)
    {
      isAvrStarted = true;
    }
  }
}
```

With the current definitions:

```cpp
int idleDelay = 2 * 60 * 1000; // 2 minutes
```

the controller auto-starts the AVR routine after two minutes if no manual start occurred.

## Important implementation notes

This project currently contains a few details worth checking against the real hardware:

1. There is a mismatch between comments and code:
   - `powerOnDelay` is defined as `1 * 30 * 1000`, which is 30 seconds
   - the comment says "5 minutes"

2. The switcher is moved through neutral before changing to the target source in `switchOnMainOutput()`:

```cpp
moveSwitcherToAngle(neutralAngleIndex);
if (withDelay)
{
  delay(powerOnDelay);
}
moveSwitcherToAngle(index);
```

This creates a short pause in the neutral position before the final source selection.

3. The project uses a servo and manual delay calls; this is appropriate for a simple controller but may need tuning on real hardware.

## Build and upload

Use PlatformIO to build and upload the firmware for the `esp32-s3-SuperMini` environment.

```bash
pio run -e esp32-s3-SuperMini
pio run -e esp32-s3-SuperMini -t upload
```

## Useful references

- ESP32-S3 Super Mini board information: https://www.nologo.tech/product/esp32/esp32s3/esp32s3supermini/esp32S3SuperMini.html
- ESP32-S3 Super Mini test project: https://github.com/UnsignedArduino/ESP32-S3-Super-Mini-Test

## Debugging tips

The current project includes commented debug settings in `platformio.ini` for JTAG debugging.

```ini
debug_tool = esp-builtin
debug_init_break = tbreak setup
```

This can be enabled when needed to inspect startup behavior on the ESP32-S3 platform.