#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_RESET     -1
#define SCREEN_ADDRESS 0x3C

constexpr int ledPin = 16;
constexpr int buzzPin = 14;
constexpr int trigPin = 13;
constexpr int echoPin = 15;
constexpr int btnPin = 12;

constexpr int BUZZ_DURATION = 100;
constexpr int BUZZ_GAP = 100;
constexpr int DELAY = 250;
constexpr int DETECT_DISTANCE = 5; // 5 cm
constexpr int WAIT = 5 * 1000;  // 5 seconds
constexpr int THANK_YOU = 3 * 1000; // 3 seconds
constexpr float RATE = 1.f;  // 1 rs / sec

enum States {
  IDLE, WAITING, ENGAGED, PAYMENT, PAID, UNPAID
} current_state;

int led_state = LOW;

int cm = 0;

float cost = 0.f;

int wait_count = 0;

Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setupSerial() {
  Serial.begin(38400);
}

void setupPins() {
  // Sensor
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // LED Pins
  pinMode(ledPin, OUTPUT);

  // Buzzer
  pinMode(buzzPin, OUTPUT);

  // Button
  pinMode(btnPin, INPUT_PULLUP);
}

void setupScreen() {
  display.begin(SCREEN_ADDRESS, true);
  display.setTextSize(2);
  display.setTextColor(SH110X_WHITE);

  display.display();
  delay(1000);
}

int getDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(5);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  int duration = pulseIn(echoPin, HIGH);
 
  return (duration/2) * 0.0343;
}

bool isCarNear() {
  return cm < DETECT_DISTANCE;
}

bool isWaitEnough() {
  constexpr int WAIT_TARGET = (WAIT / DELAY);
  if (wait_count++ ==  WAIT_TARGET) {
    wait_count = 0;
    return true;
  }

  return false;
}

void printScreen(const char* msg) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print(msg);
  display.display();
}

void lightOn() {
  digitalWrite(ledPin, HIGH);
  led_state = HIGH;
}

void lightOff() {
  digitalWrite(ledPin, LOW);
  led_state = LOW;
}

void lightToggle() {
  if (led_state == LOW) {
    lightOn();
  } else {
    lightOff();
  }
}

void buzzOn() {
  digitalWrite(buzzPin, HIGH);
}

void buzzOff() {
  digitalWrite(buzzPin, LOW);
}

void buzzOnce() {
  buzzOn();
  delay(BUZZ_DURATION);
  buzzOff();
}

void buzzTwice() {
  buzzOnce();
  delay(BUZZ_GAP);
  buzzOnce();
}

void buzzThrice() {
  buzzOnce();
  delay(BUZZ_GAP);
  buzzOnce();
  delay(BUZZ_GAP);
  buzzOnce();
}

bool isBtnPressed() {
  return digitalRead(btnPin) == LOW;
}

void gotoIdle() {
  lightOff();
  wait_count = 0;
  cost = 0.f;
  current_state = IDLE;
}

void gotoWaiting() {
  lightOn();
  buzzOnce();
  current_state = WAITING;
}

void gotoEngaged() {
  lightOn();
  buzzTwice();
  current_state = ENGAGED;
}

void gotoPayment() {
  lightOn();
  buzzThrice();
  current_state = PAYMENT;
}

void gotoPaid() {
  lightOff();
  buzzTwice();
  current_state = PAID;
}

void gotoUnpaid() {
  lightOff();
  buzzOn();
  current_state = UNPAID;
}

void stateIdle() {
  printScreen(
    "   PARK\n\n"
    "   HERE"
  );

  if (isCarNear()) {
    gotoWaiting();
  }
}

void stateWaiting() {
  printScreen(
    "  WAITING"
  );

  if (isWaitEnough()) {
    gotoEngaged();
  }

  if (not isCarNear()) {
    gotoIdle();
  }
}

void stateEngaged() {

  constexpr float inc = RATE * DELAY / 1000.f;

  cost += inc;

  char msg[30];
  sprintf(
    msg,
    "  ENGAGED\n"
    "----------\n"
    "Rs %.2f",
    cost
  );

  printScreen(msg);

  if ( not isCarNear() ) {
    gotoUnpaid();
  }

  if (isBtnPressed()) {
    gotoPayment();
  }
}

void statePayment() {
  char msg[20];
  sprintf(
    msg,
    "    PAY\n"
    "----------\n"
    "Rs %.2f",
    cost
  );
  printScreen(msg);

  if ( not isCarNear() ) {
    gotoUnpaid();
  }

  if ( isBtnPressed() ) {
    gotoPaid();
  }
}

void statePaid() {
  printScreen(
    "   THANK\n\n"
    "    YOU"
  );
  delay(THANK_YOU);
  gotoIdle();
}

void stateUnpaid() {
  lightToggle();
  printScreen(
    "  WARNING\n\n"
    "  UNPAID"
  );
}

void setup() {
  setupPins();
  setupScreen();
}

void loop() {
  cm = getDistance();

  switch (current_state) {
  case IDLE:
    stateIdle();
    break;
  case WAITING:
    stateWaiting();
    break;
  case ENGAGED:
    stateEngaged();
    break;
  case PAYMENT:
    statePayment();
    break;
  case PAID:
    statePaid();
    break;
  case UNPAID:
    stateUnpaid();
    break;
  }
  
  delay(DELAY);
}
