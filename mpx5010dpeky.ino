/****************************************************
 * HEALTH SCREENING SYSTEM
 * ESP32-S3 + MPX5010DP
 * OLED 0.96" SSD1306
 *
 * SDA : GPIO40
 * SCL : GPIO41
 *
 * RGB LED
 * Green = Ready
 * Red = Pressure
 ****************************************************/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#ifndef RGB_BUILTIN
#define RGB_BUILTIN 48
#endif

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

Adafruit_SSD1306 display(
SCREEN_WIDTH,
SCREEN_HEIGHT,
&Wire,
OLED_RESET
);

const int SENSOR_PIN = 4;

// ADC
const float ADC_VREF = 3.3;
const int ADC_MAX = 4095;

// Divider
const float R1 = 10000.0;
const float R2 = 15000.0;
const float DIVIDER_RATIO = (R1 + R2) / R2;

// Filter

const int FILTER_SIZE = 20;

float samples[FILTER_SIZE];

float sampleSum = 0;

int sampleIndex = 0;

// Zero calibration

float zeroVoltage = 0;

// Pressure

float pressure_kPa = 0;

float pressure_cm = 0;

float pressure_mmHg = 0;

float sensorVoltage = 0;

int rawADC = 0;

// Status

bool pressureDetected = false;

const float THRESHOLD = 0.10;

unsigned long lastDisplay = 0;


//====================================================
// RGB
//====================================================

void LED_GREEN()
{
    neopixelWrite(RGB_BUILTIN,0,25,0);
}

void LED_RED()
{
    neopixelWrite(RGB_BUILTIN,25,0,0);
}

void LED_OFF()
{
    neopixelWrite(RGB_BUILTIN,0,0,0);
}

//====================================================
// Moving Average
//====================================================

float movingAverage(float value)
{
    sampleSum -= samples[sampleIndex];

    samples[sampleIndex] = value;

    sampleSum += value;

    sampleIndex++;

    if(sampleIndex>=FILTER_SIZE)
        sampleIndex=0;

    return sampleSum/FILTER_SIZE;
}

//====================================================

float readVoltage()
{
    rawADC = analogRead(SENSOR_PIN);

    float pinVoltage =
    rawADC * ADC_VREF / ADC_MAX;

    return pinVoltage * DIVIDER_RATIO;
}

//====================================================

void splash()
{

display.clearDisplay();

display.setTextSize(2);
display.setCursor(6,8);
display.println("HEALTH");

display.setCursor(6,28);
display.println("SCREENING");

display.display();

delay(1800);

display.clearDisplay();

display.setTextSize(1);

display.setCursor(12,5);
display.println("Initializing System");

display.drawRect(10,35,108,10,WHITE);

display.display();

for(int i=0;i<=104;i+=4)
{

display.fillRect(12,37,i,6,WHITE);

display.display();

delay(45);

}

delay(500);

}

//====================================================

void calibrate()
{

display.clearDisplay();

display.setCursor(0,0);

display.println("Sensor Calibration");

display.println("");

display.println("Do NOT apply");

display.println("pressure...");

display.display();

LED_GREEN();

float sum=0;

int count=0;

unsigned long t=millis();

while(millis()-t<5000)
{

sum+=readVoltage();

count++;

delay(10);

}

zeroVoltage=sum/count;

display.clearDisplay();

display.setCursor(0,15);

display.println("Calibration");

display.println("Completed!");

display.display();

delay(1000);

}

//====================================================

void setup()
{

Serial.begin(115200);

analogReadResolution(12);

analogSetPinAttenuation(
SENSOR_PIN,
ADC_11db
);

pinMode(RGB_BUILTIN,OUTPUT);

Wire.begin(40,41);

display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

display.setRotation(2);    // Rotate OLED 180°
display.clearDisplay();
display.setTextColor(SSD1306_WHITE);
display.display();

display.clearDisplay();

display.setTextColor(WHITE);

for(int i=0;i<FILTER_SIZE;i++)
samples[i]=0;

splash();

calibrate();

LED_GREEN();

}


//==============================
// Dashboard
//==============================
void drawDashboard(float kPa,float cm,float mmHg){
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0);
  display.println("HEALTH SCREENING");
  display.drawLine(0,10,127,10,SSD1306_WHITE);

  display.setCursor(0,14);
  display.print("P:");
  display.setTextSize(2);
  display.setCursor(18,14);
  display.print(kPa,2);
  display.print("k");

  display.setTextSize(1);
  display.setCursor(0,38);
  display.print(cm,1);
  display.print(" cmH2O");

  int bar=(int)(kPa*20);
  if(bar<0) bar=0;
  if(bar>118) bar=118;
  display.drawRect(4,50,120,8,SSD1306_WHITE);
  display.fillRect(5,51,bar,6,SSD1306_WHITE);

  display.setCursor(0,60);
  if(kPa>THRESHOLD){
    display.print("PRESSURE");
  }else{
    display.print("READY");
  }
  display.display();
}

//==============================
// Main Loop
//==============================
void loop(){

  sensorVoltage = readVoltage();
  sensorVoltage = movingAverage(sensorVoltage);

  pressure_kPa = (sensorVoltage-zeroVoltage)/0.45;
  if(pressure_kPa<0) pressure_kPa=0;

  pressure_cm = pressure_kPa*10.1972;
  pressure_mmHg = pressure_kPa*7.50062;

  pressureDetected = pressure_kPa>THRESHOLD;

  if(pressureDetected) LED_RED();
  else LED_GREEN();

  drawDashboard(pressure_kPa,pressure_cm,pressure_mmHg);

  Serial.print("ADC:");
  Serial.print(rawADC);
  Serial.print(" V:");
  Serial.print(sensorVoltage,3);
  Serial.print(" kPa:");
  Serial.print(pressure_kPa,2);
  Serial.print(" cmH2O:");
  Serial.println(pressure_cm,2);

  delay(100);
}
