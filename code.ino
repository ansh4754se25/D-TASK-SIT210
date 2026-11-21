#include <Wire.h>
#include <Servo.h>
#include <DHT.h>
#include <U8g2lib.h>

// ---------------- OLED ----------------
U8G2_SSD1306_128X64_NONAME_1_HW_I2C oled(
  U8G2_R0, U8X8_PIN_NONE
);

// ---------------- PINS ----------------
#define DHT_PIN       2
#define TRIG_PIN      3
#define ECHO_PIN      4
#define IR_PIN        5
#define PIR_PIN       6
#define RELAY_PIN     7
#define BUZZER_PIN    8
#define SERVO_PIN     9
#define TOUCH_PIN     10

// ---------------- SETTINGS ----------------
#define DHT_TYPE DHT11

const int DOOR_OPEN_STATE = HIGH;  // IR HIGH = door open
const int RELAY_ON_STATE = LOW;    // Active-LOW relay module
const int TOUCH_ACTIVE_STATE = HIGH;

const int SERVO_OPEN_ANGLE = 90;
const int SERVO_CLOSED_ANGLE = 0;

const float TEMP_LIMIT = 35.0;

const int OBSTACLE_DISTANCE = 20;
const int OBSTACLE_CLEAR_DISTANCE = 24;

// Timings
const unsigned long DHT_INTERVAL = 2000;
const unsigned long OLED_INTERVAL = 200;
const unsigned long SERIAL_INTERVAL = 500;
const unsigned long ULTRASONIC_INTERVAL = 100;
const unsigned long TOUCH_DEBOUNCE = 300;
const unsigned long IR_FILTER_TIME = 80;

// ---------------- OBJECTS ----------------
DHT dht(DHT_PIN, DHT_TYPE);
Servo doorServo;

// ---------------- VARIABLES ----------------
float temperature = 0;
float humidity = 0;
float distance = -1;

bool dhtValid = false;
bool doorOpen = false;
bool lightOn = false;
bool motionDetected = false;
bool obstacleDetected = false;
bool buzzerOn = false;

bool touchLastReading = LOW;
bool touchStableState = LOW;

int irCandidate = LOW;

unsigned long lastTouchTime = 0;
unsigned long lastIRChangeTime = 0;
unsigned long lastDHTTime = 0;
unsigned long lastOLEDTime = 0;
unsigned long lastSerialTime = 0;
unsigned long lastUltrasonicTime = 0;

// ---------------- DOOR / IR ----------------
void checkDoor() {
  int reading = digitalRead(IR_PIN);

  if (reading != irCandidate) {
    irCandidate = reading;
    lastIRChangeTime = millis();
  }

  if (millis() - lastIRChangeTime >= IR_FILTER_TIME) {
    doorOpen = (irCandidate == DOOR_OPEN_STATE);
  }
}

// ---------------- RELAY ----------------
// IR sensor is the ONLY controller of the relay.
void controlLight() {
  lightOn = doorOpen;

  digitalWrite(
    RELAY_PIN,
    lightOn ? RELAY_ON_STATE : !RELAY_ON_STATE
  );
}

// ---------------- TOUCH / SERVO ----------------
void checkTouch() {
  bool reading = digitalRead(TOUCH_PIN);

  if (reading != touchStableState) {
    if (millis() - lastTouchTime >= TOUCH_DEBOUNCE) {
      touchStableState = reading;
      lastTouchTime = millis();

      if (touchStableState == TOUCH_ACTIVE_STATE) {
        doorOpen = !doorOpen;

        if (doorOpen) {
          doorServo.write(SERVO_OPEN_ANGLE);
        } else {
          doorServo.write(SERVO_CLOSED_ANGLE);
        }
      }
    }
  }
}

// ---------------- ULTRASONIC ----------------
void readUltrasonic() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(5);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);

  digitalWrite(TRIG_PIN, LOW);

  unsigned long duration = pulseIn(ECHO_PIN, HIGH, 25000UL);

  if (duration == 0) {
    distance = -1;
    obstacleDetected = false;
    return;
  }

  distance = duration * 0.0343 / 2.0;

  if (distance >= 2 && distance <= OBSTACLE_DISTANCE) {
    obstacleDetected = true;
  }
  else if (distance > OBSTACLE_CLEAR_DISTANCE) {
    obstacleDetected = false;
  }
}

// ---------------- BUZZER ----------------
// Ultrasonic sensor is the ONLY controller of the buzzer.
void controlBuzzer() {
  if (obstacleDetected && !buzzerOn) {
    tone(BUZZER_PIN, 2000);
    buzzerOn = true;
  }
  else if (!obstacleDetected && buzzerOn) {
    noTone(BUZZER_PIN);
    buzzerOn = false;
  }
}

// ---------------- PIR ----------------
// PIR is used only for motion detection.
void readPIR() {
  motionDetected = (digitalRead(PIR_PIN) == HIGH);
}

// ---------------- DHT11 ----------------
void readDHT() {
  if (millis() - lastDHTTime >= DHT_INTERVAL) {
    lastDHTTime = millis();

    float newTemperature = dht.readTemperature();
    float newHumidity = dht.readHumidity();

    if (!isnan(newTemperature) && !isnan(newHumidity)) {
      temperature = newTemperature;
      humidity = newHumidity;
      dhtValid = true;
    } else {
      dhtValid = false;
    }
  }
}

// ---------------- RASPBERRY PI COMMUNICATION ----------------
// One JSON object per line, sent over USB serial.
void sendToRaspberryPi() {
  if (millis() - lastSerialTime >= SERIAL_INTERVAL) {
    lastSerialTime = millis();

    Serial.print("{");

    Serial.print("\"temperature\":");
    if (dhtValid) Serial.print(temperature, 1);
    else Serial.print("null");

    Serial.print(",\"humidity\":");
    if (dhtValid) Serial.print(humidity, 1);
    else Serial.print("null");

    Serial.print(",\"door\":\"");
    Serial.print(doorOpen ? "OPEN" : "CLOSED");
    Serial.print("\"");

    Serial.print(",\"motion\":");
    Serial.print(motionDetected ? "true" : "false");

    Serial.print(",\"distance\":");
    if (distance >= 0) Serial.print(distance, 1);
    else Serial.print("null");

    Serial.print(",\"obstacle\":");
    Serial.print(obstacleDetected ? "true" : "false");

    Serial.print(",\"relay\":");
    Serial.print(lightOn ? "true" : "false");

    Serial.print(",\"buzzer\":");
    Serial.print(buzzerOn ? "true" : "false");

    Serial.print(",\"temperature_warning\":");
    Serial.print((dhtValid && temperature >= TEMP_LIMIT) ? "true" : "false");

    Serial.println("}");
  }
}

// ---------------- OLED DISPLAY ----------------
void updateOLED() {
  if (millis() - lastOLEDTime < OLED_INTERVAL) return;
  lastOLEDTime = millis();

  oled.firstPage();

  do {
    oled.setFont(u8g2_font_6x10_tf);

    oled.drawStr(0, 9, "SMART REFRIGERATOR");
    oled.drawHLine(0, 12, 128);

    char line[32];

    if (dhtValid) {
      snprintf(line, sizeof(line), "Temp: %.1f C", temperature);
      oled.drawStr(0, 24, line);

      snprintf(line, sizeof(line), "Humidity: %.1f%%", humidity);
      oled.drawStr(0, 35, line);
    } else {
      oled.drawStr(0, 24, "Temp: Sensor Error");
      oled.drawStr(0, 35, "Humidity: --");
    }

    snprintf(line, sizeof(line), "Door: %s",
             doorOpen ? "OPEN" : "CLOSED");
    oled.drawStr(0, 46, line);

    snprintf(line, sizeof(line), "PIR:%s LED:%s",
             motionDetected ? "YES" : "NO",
             lightOn ? "ON" : "OFF");
    oled.drawStr(0, 57, line);

    if (dhtValid && temperature >= TEMP_LIMIT) {
      oled.drawStr(82, 24, "HOT!");
    }

    if (buzzerOn) {
      oled.drawStr(82, 35, "ALERT!");
    }

  } while (oled.nextPage());
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  pinMode(IR_PIN, INPUT);
  pinMode(PIR_PIN, INPUT);
  pinMode(TOUCH_PIN, INPUT);

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(TRIG_PIN, LOW);
  digitalWrite(RELAY_PIN, !RELAY_ON_STATE);
  noTone(BUZZER_PIN);

  doorServo.attach(SERVO_PIN);
  doorServo.write(SERVO_CLOSED_ANGLE);

  dht.begin();

  Wire.begin();
  Wire.setClock(100000);

  oled.setI2CAddress(0x3C << 1);
  oled.begin();

  // Initialize IR filter from the sensor's actual starting reading.
  irCandidate = digitalRead(IR_PIN);
  doorOpen = (irCandidate == DOOR_OPEN_STATE);
  lastIRChangeTime = millis();

  // Initialize touch state without triggering the servo.
  touchLastReading = digitalRead(TOUCH_PIN);
  touchStableState = touchLastReading;

  lastDHTTime = millis() - DHT_INTERVAL;
  lastOLEDTime = millis();
  lastSerialTime = millis();
  lastUltrasonicTime = millis();

  readDHT();
  controlLight();

  Serial.println("{\"status\":\"SMART_REFRIGERATOR_STARTED\"}");
}

// ---------------- MAIN LOOP ----------------
void loop() {
  checkTouch();
  checkDoor();
  readPIR();

  if (millis() - lastUltrasonicTime >= ULTRASONIC_INTERVAL) {
    lastUltrasonicTime = millis();
    readUltrasonic();
  }

  controlLight();
  controlBuzzer();

  readDHT();

  updateOLED();
  sendToRaspberryPi();
}