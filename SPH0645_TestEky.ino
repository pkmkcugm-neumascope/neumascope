// ESP32-S3 Pneumonia Screening Device
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP_I2S.h>
#include <Adafruit_NeoPixel.h>

// ---------- Constants & Definitions ----------
#ifndef RGB_BUILTIN
#define RGB_BUILTIN 48 //[cite: 1]
#endif

#define SCREEN_WIDTH 128 //[cite: 1]
#define SCREEN_HEIGHT 64 //[cite: 1]

const int BCLK = 16; //[cite: 1]
const int WS   = 17; //[cite: 1]
const int DIN  = 18; //[cite: 1]

const int EXT_RGB_PIN = 6;
const int BUZZER_PIN = 21;

const float THRESHOLD_LOW = 1500.0;
const float THRESHOLD_HIGH = 7000.0;

// ---------- Enums & Globals ----------
enum SystemState {
  BOOT,
  PREPARATION,
  READY,      // Available for expansion (flows directly from PREPARATION to RECORDING)
  RECORDING,
  PROCESSING,
  RESULT,
  IDLE
};

SystemState currentState = BOOT;
unsigned long stateStartTime = 0;
bool stateFirstRun = true;

// Audio Processing Variables
int32_t currentSample = 0;
int16_t sampleCount = 0;
int64_t sumSquares = 0;

float currentRMS = 0.0;
float peakRMS = 0.0;
float sumAllRMS = 0.0;
uint32_t countAllRMS = 0;
float noiseFloor = 0.0;

// Notification Flags
uint8_t resultBeepCount = 0;
unsigned long lastBeepTime = 0;

// ---------- Objects ----------
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1); //[cite: 1]
I2SClass i2s; //[cite: 1]
Adafruit_NeoPixel extPixel(1, EXT_RGB_PIN, NEO_GRB + NEO_KHZ800);

// ---------- Helper Functions ----------

// Built-in RGB Helpers
void setBuiltInRGB(uint8_t r, uint8_t g, uint8_t b) {
  neopixelWrite(RGB_BUILTIN, r, g, b); //[cite: 1]
}

void setup() {
  Serial.begin(115200); //[cite: 1]
  delay(500); //[cite: 1]

  // Initialize Buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  noTone(BUZZER_PIN);

  // Initialize External RGB
  extPixel.begin();
  extPixel.clear();
  extPixel.show();

  // Initialize OLED
  Wire.begin(40, 41); //[cite: 1]
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { //[cite: 1]
    Serial.println("SSD1306 failed"); //[cite: 1]
    while (1); //[cite: 1]
  }
  display.setRotation(2); //[cite: 1]
  display.setTextColor(SSD1306_WHITE); //[cite: 1]

  // Initialize I2S
  i2s.setPins(BCLK, WS, -1, DIN); //[cite: 1]
  if (!i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT)) { //[cite: 1]
    Serial.println("I2S init failed"); //[cite: 1]
    while (1); //[cite: 1]
  }

  // Start System
  changeState(BOOT);
}

void loop() {
  switch (currentState) {
    case BOOT:        updateBoot(); break;
    case PREPARATION: updatePreparation(); break;
    case READY:       changeState(RECORDING); break; 
    case RECORDING:   updateRecording(); break;
    case PROCESSING:  updateProcessing(); break;
    case RESULT:      updateResult(); break;
    case IDLE:        updateIdle(); break;
  }
  
  checkSerialRestart();
}

// ---------- State Transitions & Handlers ----------

void changeState(SystemState newState) {
  currentState = newState;
  stateStartTime = millis();
  stateFirstRun = true;
  
  // Turn off hardware on transition to ensure clean states
  noTone(BUZZER_PIN);
  setBuiltInRGB(0, 0, 0);
  extPixel.clear();
  extPixel.show();
  display.clearDisplay(); //[cite: 1]
}

void checkSerialRestart() {
  if (Serial.available() > 0) {
    char c = Serial.read();
    if (c == 'Y' || c == 'y') {
      changeState(BOOT);
    } else if ((c == 'N' || c == 'n') && currentState == RESULT) {
      changeState(IDLE);
    }
  }
}

void updateBoot() {
  if (stateFirstRun) {
    display.clearDisplay(); //[cite: 1]
    display.setTextSize(1);
    display.setCursor(0, 10);
    display.println("UGM Pneumonia");
    display.println("Screening");
    display.println("\nInitializing...");
    display.display(); //[cite: 1]
    stateFirstRun = false;
  }

  if (millis() - stateStartTime >= 2000) {
    changeState(PREPARATION);
  }
}

void updatePreparation() {
  unsigned long elapsed = millis() - stateStartTime;
  int countdown = 10 - (elapsed / 1000);

  // Phase 1: First 7 Seconds
  if (elapsed < 7000) {
    // Continuous Tone
    tone(BUZZER_PIN, 2000);

    // Color Cycle: Red -> Yellow -> Green
    int colorPhase = (elapsed / 1000) % 3;
    if (colorPhase == 0) extPixel.setPixelColor(0, extPixel.Color(255, 0, 0));
    else if (colorPhase == 1) extPixel.setPixelColor(0, extPixel.Color(255, 255, 0));
    else extPixel.setPixelColor(0, extPixel.Color(0, 255, 0));
    extPixel.show();

    // OLED display
    display.clearDisplay(); //[cite: 1]
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("PREPARE");
    display.print("Screening starts in: ");
    display.println(countdown);
    
    // Progress bar (fills up)
    int barWidth = map(elapsed, 0, 10000, 0, 120);
    display.drawRect(4, 38, 120, 10, SSD1306_WHITE); //[cite: 1]
    display.fillRect(5, 39, barWidth, 8, SSD1306_WHITE); //[cite: 1]
    display.display(); //[cite: 1]
  } 
  // Phase 2: Last 3 Seconds
  else if (elapsed < 10000) {
    // Rapid beep (100ms ON / 100ms OFF)
    bool toggle = (elapsed / 100) % 2 == 0;
    
    if (toggle) {
      tone(BUZZER_PIN, 2000);
      extPixel.setPixelColor(0, extPixel.Color(0, 0, 255)); // Blue fast blink
    } else {
      noTone(BUZZER_PIN);
      extPixel.clear();
    }
    extPixel.show();

    // OLED display
    display.clearDisplay(); //[cite: 1]
    display.setTextSize(2); //[cite: 1]
    display.setCursor(0, 0);
    display.println("GET READY");
    display.setTextSize(3);
    display.setCursor(55, 25);
    display.print(countdown);
    display.display(); //[cite: 1]
  } 
  else {
    changeState(RECORDING);
  }
}

void updateRecording() {
  unsigned long elapsed = millis() - stateStartTime;
  int remaining = 20 - (elapsed / 1000);

  if (stateFirstRun) {
    peakRMS = 0;
    sumAllRMS = 0;
    countAllRMS = 0;
    noiseFloor = 0;
    sumSquares = 0;
    sampleCount = 0;
    stateFirstRun = false;
  }

  // Visuals: Built-in RGB Random Blinks (every 150ms)
  if ((elapsed / 150) % 2 == 0) {
    setBuiltInRGB(random(255), random(255), random(255));
  } else {
    setBuiltInRGB(0, 0, 0);
  }

  // Update OLED every ~250ms to prevent screen flicker and save cycles for I2S
  static unsigned long lastOledUpdate = 0;
  if (millis() - lastOledUpdate > 250) {
    display.clearDisplay(); //[cite: 1]
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Recording Audio");
    display.print(remaining);
    display.println(" s remaining");

    // Progress bar (decreases)
    int barWidth = map(20000 - elapsed, 0, 20000, 0, 120);
    display.drawRect(4, 38, 120, 10, SSD1306_WHITE); //[cite: 1]
    display.fillRect(5, 39, barWidth, 8, SSD1306_WHITE); //[cite: 1]
    display.display(); //[cite: 1]
    lastOledUpdate = millis();
  }

  // Audio Acquisition
  int32_t sample = 0;
  // Non-blocking read check
  if (i2s.readBytes((char *)&sample, sizeof(sample)) == sizeof(sample)) { //[cite: 1]
    sample >>= 14; // Valid data conversion[cite: 1]
    
    // Accumulate for RMS
    sumSquares += (int64_t)sample * sample;
    sampleCount++;

    // Calculate RMS every 256 samples (~16ms at 16kHz)
    if (sampleCount >= 256) {
      currentRMS = sqrt((float)sumSquares / 256.0);
      
      // Plotter output (Sample + RMS)
      Serial.print(sample);
      Serial.print(",");
      Serial.println(currentRMS);

      // Track Peak and Average
      if (currentRMS > peakRMS) peakRMS = currentRMS;
      sumAllRMS += currentRMS;
      countAllRMS++;

      // Compute Noise Floor during the first 1 second
      if (elapsed <= 1000) {
        noiseFloor = sumAllRMS / countAllRMS;
      }

      // Reset block accumulator
      sumSquares = 0;
      sampleCount = 0;
    }
  }

  // Exit condition
  if (elapsed >= 20000) {
    changeState(PROCESSING);
  }
}

void updateProcessing() {
  if (stateFirstRun) {
    display.clearDisplay(); //[cite: 1]
    display.setTextSize(1);
    display.setCursor(0, 20);
    display.println("Processing...");
    display.drawRect(4, 38, 120, 10, SSD1306_WHITE); //[cite: 1]
    display.fillRect(5, 39, 120, 8, SSD1306_WHITE); // Full bar[cite: 1]
    display.display(); //[cite: 1]
    stateFirstRun = false;
  }

  if (millis() - stateStartTime >= 1000) {
    changeState(RESULT);
  }
}

void updateResult() {
  unsigned long elapsed = millis() - stateStartTime;
  
  if (stateFirstRun) {
    float avgRMS = (countAllRMS > 0) ? (sumAllRMS / countAllRMS) : 0;
    
    // Determine Status
    String status = "";
    if (avgRMS < THRESHOLD_LOW) {
      status = "LOW SIGNAL";
    } else if (avgRMS > THRESHOLD_HIGH) {
      status = "HIGH SIGNAL";
    } else {
      status = "NORMAL";
    }

    // Print Serial Report
    Serial.println("================================");
    Serial.println("SCREENING COMPLETE");
    Serial.print("Average RMS    : "); Serial.println(avgRMS);
    Serial.print("Peak RMS       : "); Serial.println(peakRMS);
    Serial.print("Noise Floor    : "); Serial.println(noiseFloor);
    Serial.println("Recording Time : 20 s");
    Serial.print("Status         : "); Serial.println(status);
    Serial.println("================================");
    Serial.println("Restart? (Y/N)");

    // OLED Display
    display.clearDisplay(); //[cite: 1]
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("SCREENING");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE); //[cite: 1]
    
    display.setCursor(0, 15);
    display.print("Avg RMS: "); display.println((int)avgRMS);
    display.print("Peak   : "); display.println((int)peakRMS);
    
    display.setCursor(0, 40);
    display.print("Status:");
    display.setTextSize(2); //[cite: 1]
    display.setCursor(0, 50);
    display.println(status);
    display.display(); //[cite: 1]

    // Set External RGB to Solid Green
    extPixel.setPixelColor(0, extPixel.Color(0, 255, 0));
    extPixel.show();

    resultBeepCount = 0;
    lastBeepTime = millis();
    stateFirstRun = false;
  }

  // Handle 3 Beeps and 3 White Flashes Non-blocking
  if (resultBeepCount < 6) { 
    if (millis() - lastBeepTime >= 200) { // Toggle every 200ms
      if (resultBeepCount % 2 == 0) {
        tone(BUZZER_PIN, 2000);
        setBuiltInRGB(255, 255, 255); // White Flash
      } else {
        noTone(BUZZER_PIN);
        setBuiltInRGB(0, 0, 0);
      }
      resultBeepCount++;
      lastBeepTime = millis();
    }
  }
}

void updateIdle() {
  if (stateFirstRun) {
    display.clearDisplay(); //[cite: 1]
    display.setTextSize(2); //[cite: 1]
    display.setCursor(10, 15);
    display.println("System");
    display.setCursor(20, 35);
    display.println("Idle");
    
    display.setTextSize(1);
    display.setCursor(0, 55);
    display.println("Press RESET or Type Y");
    display.display(); //[cite: 1]
    stateFirstRun = false;
  }
}