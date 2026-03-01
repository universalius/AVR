How to build PlatformIO based project
=====================================

1. [Install PlatformIO Core](https://docs.platformio.org/page/core.html)
2. Download [development platform with examples](https://github.com/platformio/platform-espressif32/archive/develop.zip)
3. Extract ZIP archive
4. Run these commands:

```shell
# Change directory to example
$ cd platform-espressif32/examples/arduino-wifiscan

# Build project
$ pio run

# Upload firmware
$ pio run --target upload

# Build specific environment
$ pio run -e quantum

# Upload firmware for the specific environment
$ pio run -e quantum --target upload

# Clean build files
$ pio run --target clean
```

How to debug ESP32 S3
1. Connect to usb port that supports JTAG debug
2. Download tool from https://zadig.akeo.ie/downloads/#google_vignette
3. Open it, click Options -> List All Devices -> Select USB JTAG/serial debug unit (Interface 2)
4. Change driver to libusbK with arrows and click install button.
5. Add to platformio.ini
```
debug_tool = esp-builtin
debug_init_break = tbreak setup
```
Can be usefull https://community.platformio.org/t/cannot-run-builtin-debugger-on-esp32-s3-board/36384/4

Reuirements
1. When power is on move switcher to neutral position 0
    - wait for 5 mins if analog signal on power invertor pin is absent enable default avr process
2. Long press on button to activate default avr process
    - look on pin for analog signal - grid power. If has some signal more then 1V then set swither to pisition 1.
    - look on pin for analog signal - invertor. If has some signal more then 1V then set swither to pisition 2. 
3. Single button click moves switcher for next position.  1 -> 0 -> 2 . Works only when power first on.
4. Double button click launch test process.
    - move switcher for each position 1 -> 0 -> 2 and check if analog signals are present on both inputs
    - if analog signals present on both inputs it means emergency. Block all actions till power reset.
