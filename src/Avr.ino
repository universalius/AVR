#include <Arduino.h>
#include <ArduinoLog.h>
#include <WiFi.h>
#include <iostream>
#include <ESP32Time.h>
#include <ESP32Servo.h>
#include "OneButton.h"

#define PIN_BUTTON 4

Servo myservo; // create servo object to control a servo
// 16 servo objects can be created on the ESP32

OneButton button(PIN_BUTTON, true, false); // create a OneButton object for the button on pin 4, active LOW, no pullup

int pos = 0; // variable to store the servo position
int servoPin = 21;
int gridPowerPin = 22;
int invertorPowerPin = 23;
int mainOutputLedPin = 19;

int powerOnDelay = 5 * 60 * 1000; // 5 minutes in milliseconds

int angles[3] = {0, 45, 80};

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

void setup()
{
  Serial.begin(115200); // make sure your Serial Monitor is also set at this baud rate.
  Log.begin(LOG_LEVEL_VERBOSE, &Serial);

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

void loop()
{
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
      if (!isGridPowerOn() && angleIndex == gridAngleIndex)
      {
        switchOnMainOutput(invertorAngleIndex, true);
        return;
      }

      if (!isInvertorPowerOn() && angleIndex == invertorAngleIndex)
      {
        switchOnMainOutput(gridAngleIndex, true);
        return;
      }
    }
  }
}

void processIdleTask()
{
  unsigned long currentMillis = millis();
  if (currentMillis - esp32PowerOnMillis >= powerOnDelay)
  {
    if (!isAvrStarted)
    {
      isAvrStarted = true;
    }
  }
}

void switchOnMainOutput(int index, bool withDelay = false)
{
  if (withDelay)
  {
    delay(powerOnDelay);
  }

  moveSwitcherToAngle(index);
  mainOutputPowerOn = true;
  digitalWrite(mainOutputLedPin, HIGH);
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

      return true;
    }
  }

  moveSwitcherToAngle(1);

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

  moveSwitcherToAngle(angleIndex);
  angleIndex++;
  if (angleIndex >= 3)
  {
    angleIndex = gridAngleIndex;
  }
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
  return getVoltage(gridPowerPin) > 1.0; // Assuming a threshold of 1.0V to determine if grid power is on
}

bool isInvertorPowerOn()
{
  return getVoltage(invertorPowerPin) > 1.0; // Assuming a threshold of 1.0V to determine if inverter power is on
}

float getVoltage(int pin)
{
  // Read analog value
  int potValue = analogRead(pin);
  // Convert to voltage: (potValue / 4095.0) * 3.3V
  float voltage = (potValue / 4095.0) * 3.3;
  return voltage;
}

void moveSwitcherToAngle(int index)
{
  angleIndex = index;
  myservo.write(angles[index]);
  delay(2000); // Wait for the servo to reach the position
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