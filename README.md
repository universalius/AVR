# AVR ESP32 S3 Project

This project uses `pioarduino`.

## Usage
- Build and upload with PlatformIO using the `pioarduino` environment.
- Open `platformio.ini` to confirm board and upload settings.

## Debugging ESP32 S3
1. Connect to a USB port that supports JTAG debug.
2. Download Zadig from: https://zadig.akeo.ie/downloads/#google_vignette
3. Open Zadig and enable `Options -> List All Devices`.
4. Select `USB JTAG/serial debug unit (Interface 2)`.
5. Install the `libusbK` driver.
6. Add the following to `platformio.ini`:

```ini
debug_tool = esp-builtin
debug_init_break = tbreak setup
```

## Requirements
1. When power is on move switcher to neutral position 0
    - wait for 5 mins if power invertor or grid input is false enable default avr process
2. Long press on button to activate default avr process
    - look on pin - grid power. If false set swither to pisition 1.
    - look on pin - invertor. If false then set swither to pisition 2. 
3. Single button click moves switcher for next position.  1 -> 0 -> 2 . Works only when power first on.
4. Double button click launch test process.
    - move switcher for each position 1 -> 0 -> 2 and check both inputs
    - if false present on both inputs it means emergency. Block all actions till power reset.

## Links
- https://www.nologo.tech/product/esp32/esp32s3/esp32s3supermini/esp32S3SuperMini.html
- https://github.com/UnsignedArduino/ESP32-S3-Super-Mini-Test