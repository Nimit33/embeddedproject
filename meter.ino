#include <SPI.h>
#include <Wire.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
// for wifi
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_RESET     -1
#define SCREEN_ADDRESS 0x3C

constexpr int ledPin = 16;
constexpr int buzzPin = 14;
constexpr int trigPin = 13;
constexpr int echoPin = 15;
constexpr int btnPin = 12;

constexpr int BUZZ_DURATION = 500;
constexpr int BUZZ_GAP = 100;
constexpr int DELAY = 250;
constexpr int DETECT_DISTANCE = 5; // 5 cm
constexpr int WAIT = 5 * 1000;  // 5 seconds
constexpr int THANK_YOU = 3 * 1000; // 3 seconds
constexpr float RATE = 1.f;  // 1 rs / sec

// Wi-Fi connection
const char* ssid = "realme123";
const char* password = "987654321";
String serverName = "http://192.168.144.219:5000";
// unsigned long lastTime = 0;
// unsigned long timerDelay = 5000;

enum States {
  IDLE, WAITING, ENGAGED, PAYMENT, PAID, UNPAID
} current_state, last_state;

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

void sendStatus(char code){
  if(WiFi.status()== WL_CONNECTED){
    WiFiClient client;
    HTTPClient http;
    http.begin(client, serverName+"/status");
    http.addHeader("Content-Type", "application/json");
    char* body = "{\"parked\" : 0}";
    body[12] = code; 

    int httpResponseCode = http.POST(body);
    http.end();
  }
  else {
    Serial.println("WiFi Disconnected");
  }
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
  last_state = current_state;
  current_state = IDLE;
}

void gotoWaiting() {
  lightOn();
  buzzOnce();
  last_state = current_state;
  current_state = WAITING;
}

void gotoEngaged() {
  lightOn();
  buzzTwice();
  last_state = current_state;
  current_state = ENGAGED;
}

void gotoPayment() {
  lightOn();
  buzzThrice();
  last_state = current_state;
  current_state = PAYMENT;
}

void gotoPaid() {
  lightOff();
  buzzTwice();
  last_state = current_state;
  current_state = PAID;
}

void gotoUnpaid() {
  lightOff();
  buzzOn();
  last_state = current_state;
  current_state = UNPAID;
}

void stateIdle() {
  printScreen(
    "   PARK\n\n"
    "   HERE"
  );

  if(last_state != IDLE){
    sendStatus('0');
  }

  if (isCarNear()) {
    gotoWaiting();
  }
}

void stateWaiting() {
  if(last_state != WAITING){
    sendStatus('1');
  }

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

  if ( isBtnPressed() ) {
    buzzOff();
    gotoIdle();
  }
}

void setup() {
  Serial.begin(9600);
  // WiFi setup
  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());

  setupPins();
  setupScreen();
  
  sendStatus('0');
  gotoIdle();
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
