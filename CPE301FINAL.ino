#include <Wire.h>
#include <RTClib.h>
#include <LiquidCrystal.h>
#include <Stepper.h>
#include <dht11.h>

#define WATER_LEVEL_SENSOR A5
#define WATER_LEVEL_POWER 6
#define VENT_STEPPER_PIN1 8
#define VENT_STEPPER_PIN2 9
#define VENT_STEPPER_PIN3 10
#define VENT_STEPPER_PIN4 11
#define START_BUTTON_PIN 18
#define STOP_BUTTON_PIN 19
#define RESET_BUTTON_PIN 2
#define DHT_PIN 7
#define RED_LED 47
#define YELLOW_LED 49
#define GREEN_LED 51
#define BLUE_LED 53

#define STEPS_PER_REV 2048
#define VENT_SPEED 10
#define LCD_COLUMNS 16
#define LCD_ROWS 2
#define WATER_LEVEL_THRESHOLD 60
#define TEMP_THRESHOLD 15

int speedPin=40;
int dir1=41;
int dir2=42;
int mSpeed=90;

RTC_DS3231 rtc;
LiquidCrystal lcd(13, 12, 5, 4, 3, 8); // Adjust the pin numbers based on your setup
Stepper stepper(STEPS_PER_REV, VENT_STEPPER_PIN1, VENT_STEPPER_PIN3, VENT_STEPPER_PIN2, VENT_STEPPER_PIN4);
dht11 DHT;

void reportStateTransition(const char *state);

enum SystemState {
  DISABLED,
  IDLE,
  ERROR,
  RUNNING
};

SystemState currentState = IDLE;
SystemState lastReportedState = DISABLED;

void setup() {
  Serial.begin(9600);

  lcd.begin(LCD_COLUMNS, LCD_ROWS);
  stepper.setSpeed(VENT_SPEED);

  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, let's set the time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  pinMode(RED_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BLUE_LED, OUTPUT);

  pinMode(WATER_LEVEL_POWER, OUTPUT);
  digitalWrite(WATER_LEVEL_POWER, LOW);

  pinMode(speedPin, OUTPUT);
  pinMode(dir1, OUTPUT);
  pinMode(dir2, OUTPUT);

  attachInterrupt(digitalPinToInterrupt(START_BUTTON_PIN), START_Function, RISING);
  attachInterrupt(digitalPinToInterrupt(STOP_BUTTON_PIN), STOP_Function, RISING);
  attachInterrupt(digitalPinToInterrupt(RESET_BUTTON_PIN), RESET_Function, RISING);
}

void loop() {
  if (currentState != lastReportedState) {
    reportStateTransition(("Transition to " + getStateName(currentState)).c_str());
    lastReportedState = currentState;
  }

  switch (currentState) {
    case DISABLED:
      LED_ON(YELLOW_LED);
      break;

    case IDLE:
      monitorAndReportTemperatureHumidity();
      monitorWaterLevel();
      LED_ON(GREEN_LED);
      break;

    case ERROR:
      LED_ON(RED_LED);
      digitalWrite(dir1, LOW);
      digitalWrite(dir2, HIGH);
      analogWrite(speedPin, 0);
      delay(25);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("ERROR:");
      lcd.setCursor(0, 1);
      lcd.print("Water Level Low");
      break;

    case RUNNING:
      monitorAndReportTemperatureHumidity();
      if (DHT.temperature < TEMP_THRESHOLD) {
        currentState = IDLE;
        reportStateTransition("Temperature below threshold. Transition to IDLE.");
      } 
      else {
        digitalWrite(dir1, LOW);
        digitalWrite(dir2, HIGH);
        analogWrite(speedPin, 255);
        delay(25);
        analogWrite(speedPin, mSpeed);
        LED_ON(BLUE_LED);
        monitorWaterLevel();
      }
      break;
  }
}

String getStateName(SystemState state) {
  switch (state) {
    case DISABLED: return "DISABLED";
    case IDLE: return "IDLE";
    case ERROR: return "ERROR";
    case RUNNING: return "RUNNING";
    default: return "UNKNOWN";
  }
}

void START_Function(){
  if(currentState == DISABLED){
    currentState = RUNNING;
  }
}

void STOP_Function(){
  if(currentState != DISABLED){
    currentState = DISABLED;
  }
}

void RESET_Function(){
  if(currentState == ERROR){
    currentState = IDLE;
  }
}

void LED_ON(int LED_PIN){
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(BLUE_LED, LOW);
  digitalWrite(RED_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(LED_PIN, HIGH);
}

void monitorAndReportTemperatureHumidity() {
  float chk = DHT.read(DHT_PIN);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Temp: ");
  lcd.print(DHT.temperature);
  lcd.print(" C");
  lcd.setCursor(0, 1);
  lcd.print("Humidity: ");
  lcd.print(DHT.humidity);
  lcd.print(" %");
}

void reportStateTransition(const char *state) {
  DateTime now = rtc.now();
  Serial.print("State Transition - ");
  Serial.print(state);
  Serial.print(" at ");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.println(now.second(), DEC);
}

void monitorWaterLevel(){
  digitalWrite(WATER_LEVEL_POWER, HIGH);
  int waterLevel = analogRead(WATER_LEVEL_SENSOR);
  digitalWrite(WATER_LEVEL_POWER, LOW);
  Serial.print("Water level: ");
  Serial.println(waterLevel);

  if(waterLevel < WATER_LEVEL_THRESHOLD){
    currentState = ERROR;
    reportStateTransition("Water Level LOW");
  }
}