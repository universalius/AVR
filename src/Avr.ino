#include <Arduino.h>
#include <ArduinoLog.h>
#include <WiFi.h>
#include <iostream>
#include <ESP32Time.h>
#include <ESP32Servo.h>
#include "OneButton.h"

#define PIN_BUTTON 3
int servoPin = 5;
int gridPowerPin = 6;       // input with external pullup resistor
int invertorPowerPin = 10;  // input with external pullup resistor
int mainOutputLedPin = 8;

int pos = 0; // variable to store the servo position

int powerOnDelay = 1 * 30 * 1000; // 5 minutes in milliseconds
int idleDelay = 2 * 60 * 1000;    // 1 minute in milliseconds

int angles[3] = {80, 45, 0}; // Angles for grid, neutral, and inverter positions

int angleIndex = 0;
int gridAngleIndex = 0;
int neutralAngleIndex = 1;
int invertorAngleIndex = 2;

bool isAvrStarted = false;
bool isEmergency = false;
bool mainOutputPowerOn = false;

unsigned long processAvrPrevMillis = 0; // Store the last time the LED was toggled
unsigned long esp32PowerOnMillis = millis();

const long interval = 1000; // Interval at which to toggle the LED (1 second)

Servo myservo; // create servo object to control a servo
// 16 servo objects can be created on the ESP32

OneButton button(PIN_BUTTON, true, false); // create a OneButton object for the button on pin 4, active LOW, no pullup

void setup()
{
  Serial.begin(115200); // make sure your Serial Monitor is also set at this baud rate.
  Log.begin(LOG_LEVEL_VERBOSE, &Serial);

  pinMode(gridPowerPin, INPUT);     // external pullup resistor
  pinMode(invertorPowerPin, INPUT); // external pullup resistor
  pinMode(mainOutputLedPin, OUTPUT);

  // Allow allocation of all timers
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  myservo.setPeriodHertz(50); // standard 50 hz servo
  myservo.attach(servoPin, 500, 2400);
  // myservo.attach(servoPin, 500, 2500); // attaches the servo on pin 21 to the servo object
  //  using default min/max of 1000us and 2000us
  //  different servos may require different min/max settings
  //  for an accurate 0 to 180 sweep

  // pinMode(PIN_BUTTON, INPUT);

  button.attachClick(handleButtonClick, &button);
  button.attachDoubleClick(handleButtonDoubleClick, &button);
  button.attachLongPressStop(handleButtonLongPressStop, &button);

  button.setLongPressIntervalMs(2000);

  moveSwitcherToAngle(neutralAngleIndex);
}

void switchOnMainOutput(int index, bool withDelay = false)
{
  moveSwitcherToAngle(neutralAngleIndex);
  mainOutputPowerOn = false;
  digitalWrite(mainOutputLedPin, LOW);

  if (withDelay)
  {
    delay(powerOnDelay);
  }

  moveSwitcherToAngle(index);
  mainOutputPowerOn = true;
  digitalWrite(mainOutputLedPin, HIGH);

  Log.infoln("Switched on main output with angle index: %d", index);
}

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
      else
      {
        moveSwitcherToAngle(neutralAngleIndex);
      }

      return true;
    }
  }

  moveSwitcherToAngle(neutralAngleIndex);

  return false;
}

void handleButtonClick(void *oneButton)
{
  Serial.print(((OneButton *)oneButton)->getPressedMs());
  Serial.println("\t - handleButtonClick()");

  if (isEmergency)
  {
    return;
  }

  angleIndex++;
  if (angleIndex >= 3)
  {
    angleIndex = gridAngleIndex;
  }

  moveSwitcherToAngle(angleIndex);
}

void handleButtonDoubleClick(void *oneButton)
{
  Serial.print(((OneButton *)oneButton)->getPressedMs());
  Serial.println("\t - handleButtonDoubleClick()");

  if (checkForEmergency())
  {
    isEmergency = true;
  }
}

void handleButtonLongPressStop(void *oneButton)
{
  Serial.print(((OneButton *)oneButton)->getPressedMs());
  Serial.println("\t - handleButtonLongPressStop()");

  if (isEmergency)
  {
    return;
  }

  isAvrStarted = true;
}

bool isGridPowerOn()
{
  bool powerOn = digitalRead(gridPowerPin) == LOW; // Assuming LOW means power is on due to external pullup
  Log.infoln("Grid power is %s", powerOn ? "ON" : "OFF");
  return powerOn;
}

bool isInvertorPowerOn()
{
  bool powerOn = digitalRead(invertorPowerPin) == LOW; // Assuming LOW means power is on due to external pullup
  Log.infoln("Inverter power is %s", powerOn ? "ON" : "OFF");
  return powerOn;
}

void moveSwitcherToAngle(int index)
{
  angleIndex = index;
  myservo.write(angles[index]);
  delay(1000); // Wait for the servo to reach the position

  Log.infoln("Moved switcher to angle: %d", angles[index]);
}

void blinkMainOutputLed(int delayTime)
{
  digitalWrite(mainOutputLedPin, HIGH); // Turn the LED on
  delay(delayTime);                     // Wait for the specified delay time
  digitalWrite(mainOutputLedPin, LOW);  // Turn the LED off
  delay(delayTime);                     // Wait for the specified delay time
}

void test()
{
  for (pos = 0; pos <= 180; pos += 10)
  { // goes from 0 degrees to 180 degrees
    // in steps of 1 degree
    myservo.write(pos); // tell servo to go to position in variable 'pos'
    delay(250);         // waits 15ms for the servo to reach the position
  }
  for (pos = 180; pos >= 0; pos -= 10)
  {                     // goes from 180 degrees to 0 degrees
    myservo.write(pos); // tell servo to go to position in variable 'pos'
    delay(250);         // waits 15ms for the servo to reach the position
  }
}

void test1()
{
  while (true)
  {
    myservo.write(0); // Move to 0 degrees
    delay(2000);
    myservo.write(90); // Move to 90 degrees
    delay(2000);
    myservo.write(180); // Move to 180 degrees
    delay(2000);

    myservo.write(90); // Move to 0 degrees
    delay(2000);
  }
}

void processAvrTask(bool withDelay = false)
{
  if (!isAvrStarted)
  {
    return;
  }

  unsigned long currentMillis = millis();
  if (currentMillis - processAvrPrevMillis >= interval)
  {
    processAvrPrevMillis = currentMillis;

    if (isEmergency)
    {
      return;
    }

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
    else
    {
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
    }
  }
}

void processIdleTask()
{
  unsigned long currentMillis = millis();
  if (currentMillis - esp32PowerOnMillis >= idleDelay)
  {
    if (!isEmergency && !isAvrStarted)
    {
      Log.infoln("ESP32 has been powered on for %d minutes. Starting AVR process.", powerOnDelay / 60000);
      isAvrStarted = true;
    }
  }
}

void loop()
{
  if (isEmergency)
  {
    blinkMainOutputLed(500);
  }

  button.tick();

  processAvrTask();

  processIdleTask();

  // if (digitalRead(PIN_BUTTON) == LOW)
  // {
  //   delay(20);
  //   if (digitalRead(PIN_BUTTON) == LOW)
  //   {
  //     myservo.write(angles[angleIndex]);

  //     angleIndex++;
  //     if (angleIndex >= 3)
  //     {
  //       angleIndex = 0;
  //     }

  //     delay(1000);
  //   }
  //   while (digitalRead(PIN_BUTTON) == LOW)
  //     ;
  //   delay(20);
  //   while (digitalRead(PIN_BUTTON) == LOW)
  //     ;
  // }

  // myservo.write(0);
  //  delay(5000);
  //  myservo.write(45); // Move to 0 degrees
  //  delay(5000);
  //  myservo.write(80); // Move to 0 degrees
  //  delay(5000);
  //  test1();
}
