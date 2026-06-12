// --- Konfigurasi Pin dan Parameter ---
const int sensorPin = 34; // GPIO 34 untuk ESP32-U

const float V_REF = 3.3;         
const int ADC_RESOLUTION = 4095; 

// Update Voltage Divider R1 = 10k, R2 = 15k
const float R1 = 10000.0; 
const float R2 = 15000.0; 
const float VOLTAGE_DIVIDER_RATIO = (R1 + R2) / R2; // Hasilnya 1.666...

// Karena ESP32 membaca 0 saat idle, kita set kalibrasi dasar 0.2V
float zeroPressureVoltage = 0.20; 

void setup() {
  Serial.begin(115200);
  analogReadResolution(12); 
  delay(2000); 
  Serial.println("=== Memulai Testing Sensor MPX5010DP ===");
}

void loop() {
  int rawADC = analogRead(sensorPin);
  float pinVoltage = 0.0;
  float sensorVoltage = 0.0;
  float pressure_kPa = 0.0;
  float pressure_cmH2O = 0.0;

  // Filter Blind Spot ESP32
  if (rawADC == 0) {
    // Jika 0, asumsikan kondisi idle/tanpa tekanan
    sensorVoltage = zeroPressureVoltage; 
    pressure_kPa = 0.0;
    pressure_cmH2O = 0.0;
  } else {
    // Perhitungan normal saat ada tekanan
    pinVoltage = (rawADC / (float)ADC_RESOLUTION) * V_REF;
    sensorVoltage = pinVoltage * VOLTAGE_DIVIDER_RATIO;
    
    // Konversi Tekanan (kPa)
    pressure_kPa = (sensorVoltage - zeroPressureVoltage) / 0.45;
    
    // Cegah nilai minus kecil akibat fluktuasi
    if (pressure_kPa < 0) pressure_kPa = 0; 
    
    // Konversi ke cmH2O
    pressure_cmH2O = pressure_kPa * 10.197;
  }

  Serial.print("RawADC:");
  Serial.print(rawADC);
  Serial.print("\t V_Sensor:");
  Serial.print(sensorVoltage, 3);
  Serial.print("\t kPa:");
  Serial.print(pressure_kPa, 3);
  Serial.print("\t cmH2O:");
  Serial.println(pressure_cmH2O, 3);

  delay(100); 
}
