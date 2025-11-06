#include <Arduino.h>
#include <Wire.h>
#include <ESP32Servo.h>
#include <SparkFun_VL53L1X.h>
#include <Adafruit_NeoPixel.h>

/* ========= Pin Map ========= */
#define SERVO_PIN   40
#define SDA_PIN     41
#define SCL_PIN     42
#define BUZZER_PIN  39
#define RGB_PIN     38
#define BUTTON_PIN  37
#define NUM_PIXELS  1

/* ========= Objects ========= */
Servo myservo;
SFEVL53L1X distanceSensor;
Adafruit_NeoPixel ledStrip(NUM_PIXELS, RGB_PIN, NEO_GRB + NEO_KHZ800);

/* ========= Shared Variables ========= */
int distanceMM = 0;
bool objectDetected = false;
bool sweeping = false;       // controlled by button
bool systemReady = false; 

/* ========= Sync ========= */
SemaphoreHandle_t distanceMutex;
SemaphoreHandle_t buttonSem;

/* ========= Buzzer setup ========= */
#define BUZZER_CHANNEL 4   // use separate LEDC timer

/* ========= Task Prototypes ========= */
void StartupTask(void *pvParameters);
void ServoTask(void *pvParameters);
void ToFTask(void *pvParameters);
void BuzzerTask(void *pvParameters);
void LEDTask(void *pvParameters);
void ButtonTask(void *pvParameters);

/* ========= Helper ========= */
void changeLED(uint8_t r, uint8_t g, uint8_t b) {
  ledStrip.setPixelColor(0, ledStrip.Color(r, g, b));
  ledStrip.show();
}

/* ========= ISR ========= */
volatile unsigned long lastPress = 0;
const unsigned long debounce = 250;
void IRAM_ATTR buttonISR() {
  unsigned long now = millis();
  if (now - lastPress > debounce) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(buttonSem, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    lastPress = now;
  }
}

/* ========= Setup ========= */
void setup() {
  Serial.begin(115200);
  Serial.println("=== FreeRTOS Radar System (Final Version) ===");

  // Servo
  myservo.setPeriodHertz(50);
  myservo.attach(SERVO_PIN, 500, 2400);
  myservo.write(90);
  Serial.println("Servo ready.");

  // Buzzer
  ledcSetup(BUZZER_CHANNEL, 2000, 8);
  ledcAttachPin(BUZZER_PIN, BUZZER_CHANNEL);
  ledcWriteTone(BUZZER_CHANNEL, 0);
  Serial.println("Buzzer ready.");

  // LED
  ledStrip.begin();
  ledStrip.setBrightness(80);
  ledStrip.clear();
  ledStrip.show();
  changeLED(0, 0, 0);
  Serial.println("RGB LED ready.");

  // Button
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, FALLING);

  // ToF
  Wire.begin(SDA_PIN, SCL_PIN);
  Serial.println("Initializing ToF...");
  if (distanceSensor.begin() != 0) {
    Serial.println("ToF not found!");
    while (1);
  }
  Serial.println("ToF sensor ready.");

  distanceMutex = xSemaphoreCreateMutex();
  buttonSem = xSemaphoreCreateBinary();

  // --- Tasks ---
  xTaskCreate(StartupTask, "Startup Task", 4096, NULL, 3, NULL);
  xTaskCreate(ServoTask,   "Servo Task",   4096, NULL, 2, NULL);
  xTaskCreate(ToFTask,     "ToF Task",     4096, NULL, 1, NULL);
  xTaskCreate(BuzzerTask,  "Buzzer Task",  2048, NULL, 1, NULL);
  xTaskCreate(LEDTask,     "LED Task",     2048, NULL, 1, NULL);
  xTaskCreate(ButtonTask,  "Button Task",  2048, NULL, 2, NULL);
}

void loop() {
  // handled by FreeRTOS
}

/* ========= TASK DEFINITIONS ========= */

// ----- Startup Task -----
void StartupTask(void *pvParameters) {
  Serial.println("Running startup animation...");
  // Flash red/green 3 times while buzzing and sweeping once
  for (int i = 0; i < 3; i++) {
    changeLED(255, 0, 0);
    ledcWriteTone(BUZZER_CHANNEL, 2000);
    vTaskDelay(400 / portTICK_PERIOD_MS);
    changeLED(0, 255, 0);
    ledcWriteTone(BUZZER_CHANNEL, 0);
    vTaskDelay(400 / portTICK_PERIOD_MS);
  }

  // Servo sweep right-left, then stop in middle
  for (int angle = 0; angle <= 180; angle += 5) {
    myservo.write(angle);
    vTaskDelay(15 / portTICK_PERIOD_MS);
  }
  for (int angle = 180; angle >= 0; angle -= 5) {
    myservo.write(angle);
    vTaskDelay(15 / portTICK_PERIOD_MS);
  }
  myservo.write(90);

  changeLED(0, 255, 0);
  ledcWriteTone(BUZZER_CHANNEL, 0);
  systemReady = true;
  Serial.println("System ready. Waiting for button press.");

  vTaskDelete(NULL); // delete startup task after done
}

// ----- Servo Task -----
void ServoTask(void *pvParameters) {
  int angle = 0;
  bool forward = true;

  while (true) {
    if (sweeping && systemReady) {
      myservo.write(angle);
      if (forward) angle += 5;
      else         angle -= 5;
      if (angle >= 180) forward = false;
      if (angle <= 0)   forward = true;
    } else {
      myservo.write(90);
    }

    vTaskDelay(25 / portTICK_PERIOD_MS);
    taskYIELD();
  }
}

// ----- ToF Task -----
void ToFTask(void *pvParameters) {
  while (true) {
    distanceSensor.startRanging();
    while (!distanceSensor.checkForDataReady()) vTaskDelay(1);
    int d = distanceSensor.getDistance();
    distanceSensor.clearInterrupt();
    distanceSensor.stopRanging();

    if (xSemaphoreTake(distanceMutex, portMAX_DELAY) == pdTRUE) {
      distanceMM = d;
      objectDetected = (distanceMM <= 100); // ≤10 cm
      xSemaphoreGive(distanceMutex);
    }

    Serial.print("Distance: ");
    Serial.print(distanceMM);
    Serial.println(" mm");

    vTaskDelay(200 / portTICK_PERIOD_MS);
  }
}

// ----- Buzzer Task -----
void BuzzerTask(void *pvParameters) {
  while (true) {
    if (objectDetected)
      ledcWriteTone(BUZZER_CHANNEL, 3000);
    else
      ledcWriteTone(BUZZER_CHANNEL, 0);
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

// ----- LED Task -----
void LEDTask(void *pvParameters) {
  bool ledOn = false;
  while (true) {
    if (!systemReady) {
      vTaskDelay(100 / portTICK_PERIOD_MS);
      continue;
    }

    if (objectDetected) {
      ledOn = !ledOn;
      if (ledOn) changeLED(255, 0, 0);
      else       changeLED(0, 0, 0);
      vTaskDelay(150 / portTICK_PERIOD_MS);
    } else if (sweeping) {
      ledOn = !ledOn;
      if (ledOn) changeLED(0, 255, 0);
      else       changeLED(0, 0, 0);
      vTaskDelay(400 / portTICK_PERIOD_MS);
    } else {
      changeLED(0, 255, 0);
      vTaskDelay(500 / portTICK_PERIOD_MS);
    }
  }
}

// ----- Button Task -----
void ButtonTask(void *pvParameters) {
  while (true) {
    if (xSemaphoreTake(buttonSem, portMAX_DELAY)) {
      if (systemReady) {
        sweeping = !sweeping;
        Serial.print("Sweeping: ");
        Serial.println(sweeping ? "ON" : "OFF");
      }
    }
  }
}
