#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <esp_gap_ble_api.h>

#define SERVICE_UUID    "4FAFC201-1FB5-459E-8FCC-C5C9C331914B"
#define CHAR_UUID_FSR1  "AA010000-1FB5-459E-8FCC-C5C9C331914B"
#define CHAR_UUID_FSR2  "AA020000-1FB5-459E-8FCC-C5C9C331914B"
#define CHAR_UUID_FSR3  "AA030000-1FB5-459E-8FCC-C5C9C331914B"
#define CHAR_UUID_FSR4  "AA040000-1FB5-459E-8FCC-C5C9C331914B"
#define CHAR_UUID_FSR5  "AA050000-1FB5-459E-8FCC-C5C9C331914B"
#define CHAR_UUID_TEMP  "AA060000-1FB5-459E-8FCC-C5C9C331914B"
#define CHAR_UUID_BSOC  "AA070000-1FB5-459E-8FCC-C5C9C331914B"

#define BLE_MTU_REQUEST 512

const uint8_t FSR_PINS[5] = {
  36,  // FSR 0  (VP  / ADC1_CH0)
  39,  // FSR 1  (VN  / ADC1_CH3)
  34,  // FSR 2  (ADC1_CH6)
  35,  // FSR 3  (ADC1_CH7)
  32,  // FSR 4  (ADC1_CH4)
};

const uint8_t DS18B20_PIN = 23;

const uint8_t LED_PIN = 19;

const uint8_t VBAT_PIN = 33;

const float   VCC           = 3.3f;
const float   R_FIXED       = 100000.0f;
const int     ADC_MAX       = 4095;
const int     ADC_SAMPLES   = 8;



struct CalPoint { float resistance; float grams; };

// Calibration table for 10 kg FSR (resistance in Ω → grams)
const CalPoint CAL_10KG[] = {
  {1000000.0f,   0.0f},
  { 100000.0f,  10.0f},
  {  30000.0f,  50.0f},
  {  10000.0f, 200.0f},
  {   5000.0f, 500.0f},
  {   2000.0f,1500.0f},
  {    500.0f,4500.0f},
  {    200.0f,9000.0f}
};
const int CAL_10KG_LEN = sizeof(CAL_10KG) / sizeof(CalPoint);

// Calibration table for 20 kg FSR
const CalPoint CAL_20KG[] = {
  {1000000.0f,    0.0f},
  { 100000.0f,   20.0f},
  {  30000.0f,  100.0f},
  {  10000.0f,  400.0f},
  {   5000.0f, 1000.0f},
  {   2000.0f, 3000.0f},
  {    500.0f, 9000.0f},
  {    200.0f,18000.0f}
};
const int CAL_20KG_LEN = sizeof(CAL_20KG) / sizeof(CalPoint);

struct BattPoint { float voltage; float soc; };

const BattPoint BATT_CURVE[] = {
  {3.00f,   0.0f},
  {3.30f,   5.0f},
  {3.50f,  10.0f},
  {3.60f,  20.0f},
  {3.70f,  40.0f},
  {3.75f,  50.0f},
  {3.80f,  60.0f},
  {3.85f,  70.0f},
  {3.95f,  80.0f},
  {4.05f,  90.0f},
  {4.20f, 100.0f}
};
const int BATT_CURVE_LEN = sizeof(BATT_CURVE) / sizeof(BattPoint);

float resistanceToGrams(float resistance, const CalPoint* table, int len) {
  if (resistance >= table[0].resistance) return table[0].grams;
  if (resistance <= table[len - 1].resistance) return table[len - 1].grams;
  for (int i = 0; i < len - 1; i++) {
    if (resistance <= table[i].resistance && resistance >= table[i + 1].resistance) {
      float t = (table[i].resistance - resistance) /
                (table[i].resistance - table[i + 1].resistance);
      return table[i].grams + t * (table[i + 1].grams - table[i].grams);
    }
  }
  return 0.0f;
}

OneWire oneWire(DS18B20_PIN);

DallasTemperature tempSensor(&oneWire);

unsigned long lastRunTime = 0;

unsigned long ledLastEvent = 0;

uint8_t ledPhase = 0;

bool bleConnected = false;

const unsigned long RUNTIME_INTERVAL_MS = 1000;

char BUF[512];

float TEMPERATURE[1];

int RAW_FSR[5];

float GRAMS[5];

float VBAT = 0.0;

float VBAT_PERCENT = 0.0;

int readADCOversampled(uint8_t pin) {
  long sum = 0;
  for (int i = 0; i < ADC_SAMPLES; i++) {
    sum += analogRead(pin);
    delayMicroseconds(200);
  }
  return (int)(sum / ADC_SAMPLES);
}

float adcToResistance(int raw) {
  if (raw <= 0)       return 1e9f;
  if (raw >= ADC_MAX) return 0.0f;
  float vout = (raw / (float)ADC_MAX) * VCC;
  return R_FIXED * (VCC - vout) / vout;
}

float voltageToSOC(float v) {
  if (v >= BATT_CURVE[BATT_CURVE_LEN-1].voltage) return 100.0f;
  if (v <= BATT_CURVE[0].voltage) return 0.0f;
  for (int i = 0; i < BATT_CURVE_LEN - 1; i++) {
    if (v >= BATT_CURVE[i].voltage && v <= BATT_CURVE[i+1].voltage) {
      float t = (v - BATT_CURVE[i].voltage) / (BATT_CURVE[i+1].voltage - BATT_CURVE[i].voltage);
      return BATT_CURVE[i].soc + t * (BATT_CURVE[i+1].soc - BATT_CURVE[i].soc);
    }
  }
  return 0.0f;
}

void updateLED() {
  static bool prevConnected = false;

  unsigned long now = millis();

  if (prevConnected != bleConnected) {
      ledPhase = 0;
      ledLastEvent = now;
      prevConnected = bleConnected;
  }

  if (!bleConnected) {
    switch (ledPhase) {
      case 0:
        digitalWrite(LED_PIN, HIGH);
        ledLastEvent = now;
        ledPhase = 1;
        break;
      case 1:
        if (now - ledLastEvent >= 100) {
          digitalWrite(LED_PIN, LOW);
          ledLastEvent = now;
          ledPhase = 2;
        }
        break;
      case 2:
        if (now - ledLastEvent >= 5900) {
          ledPhase = 0;
        }
        break;
    }
  } else {
    switch (ledPhase) {
      case 0:
        digitalWrite(LED_PIN, HIGH);
        ledLastEvent = now;
        ledPhase = 1;
        break;
      case 1:
        if (now - ledLastEvent >= 80) {
          digitalWrite(LED_PIN, LOW);
          ledLastEvent = now;
          ledPhase = 2;
        }
        break;
      case 2:
        if (now - ledLastEvent >= 120) {
          digitalWrite(LED_PIN, HIGH);
          ledLastEvent = now;
          ledPhase = 3;
        }
        break;
      case 3:
        if (now - ledLastEvent >= 80) {
          digitalWrite(LED_PIN, LOW);
          ledLastEvent = now;
          ledPhase = 4;
        }
        break;
      case 4:
        if (now - ledLastEvent >= 5720) {
          ledPhase = 0;
        }
        break;
    }
  }
}

void takeReadings() {
  float t = tempSensor.getTempCByIndex(0);
  if (t != DEVICE_DISCONNECTED_C) {
    TEMPERATURE[0] = t;
  } else {
    Serial.println("[TEMP] Sensor disconnected or not found");
  }
  tempSensor.requestTemperatures();

  for (int i = 0; i < 5; i++) {
    RAW_FSR[i] = ADC_MAX - readADCOversampled(FSR_PINS[i]);
    float resistance = adcToResistance(RAW_FSR[i]);
    if(i == 0) {
      GRAMS[i] = resistanceToGrams(resistance, CAL_20KG, CAL_20KG_LEN);
    }else {
      GRAMS[i] = resistanceToGrams(resistance, CAL_10KG, CAL_10KG_LEN);
    }
  }

  int vbat_adc = readADCOversampled(VBAT_PIN);

  VBAT = ( (float)vbat_adc * 6.6f ) / 4095.0f;

  VBAT_PERCENT = ( VBAT - 3.0f ) / ( 4.2f - 3.0f );
  VBAT_PERCENT *= 100.0f;

  // VBAT_PERCENT = voltageToSOC(VBAT);
}

void serializeData()
{
  BUF[0] = '\0';
  sprintf(
    BUF, 
    "{\"raw\":[%d,%d,%d,%d,%d],\"grams\":[%.2f,%.2f,%.2f,%.2f,%.2f],\"temp\":[%.2f],\"vbat\":%.2f,\"bsoc\":%.1f}", 
    RAW_FSR[0], 
    RAW_FSR[1], 
    RAW_FSR[2], 
    RAW_FSR[3], 
    RAW_FSR[4], 
    GRAMS[0], 
    GRAMS[1], 
    GRAMS[2], 
    GRAMS[3], 
    GRAMS[4], 
    TEMPERATURE[0], 
    VBAT, 
    VBAT_PERCENT
  );
}

BLEServer* pServer = nullptr;
BLECharacteristic* pCharFSR[5] = { nullptr, nullptr, nullptr, nullptr, nullptr };
BLECharacteristic* pCharTemp   = nullptr;
BLECharacteristic* pCharBSOC   = nullptr;

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pSvr) override {
    bleConnected = true;
    Serial.println("[BLE] Client connected");
  }
  void onDisconnect(BLEServer* pSvr) override {
    bleConnected = false;
    Serial.println("[BLE] Client disconnected — restarting advertising");
    pSvr->startAdvertising();
  }
};

class SecurityCallbacks : public BLESecurityCallbacks {
  uint32_t onPassKeyRequest() override { return 0; }  // not used in Just Works
  void onPassKeyNotify(uint32_t pass_key) override {
    Serial.printf("[BLE] Passkey for pairing: %06d\n", pass_key);
  }
  bool onConfirmPIN(uint32_t pass_key) override { return true; }
  bool onSecurityRequest() override { return true; }
  void onAuthenticationComplete(esp_ble_auth_cmpl_t auth) override {
    if (auth.success) {
      Serial.println("[BLE] Pairing successful — link encrypted");
    } else {
      Serial.printf("[BLE] Pairing failed, reason: %d\n", auth.fail_reason);
    }
  }
};

void notifyString(BLECharacteristic* ch, const char* str) {
  if (!bleConnected || ch == nullptr) return;
  ch->setValue((uint8_t*)str, strlen(str));
  ch->notify();
}

void notifyFString(BLECharacteristic* ch, float val) {
  if (!bleConnected || ch == nullptr) return;
  char sBuf[8];
  snprintf(sBuf, sizeof(sBuf), "%.1f", val);
  ch->setValue((uint8_t*)sBuf, strlen(sBuf));
  ch->notify();
}

void setup() {
  Serial.begin(115200);

  Serial.println("\n[BOOT] Insole Pressure Monitor");

  pinMode(LED_PIN, OUTPUT);

  digitalWrite(LED_PIN, LOW);

  // ADC resolution
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  // DS18B20
  tempSensor.begin();
  tempSensor.setResolution(12); // 12-bit = 0.0625 °C resolution
  tempSensor.setWaitForConversion(false); // Non-blocking
  tempSensor.requestTemperatures();

  Serial.println("[TEMP] DS18B20 initialised");

  // BLE
  BLEDevice::init("Insole Monitor v1.0");

  // ── BLE Security: "Just Works" pairing with encryption ──────
  BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);   // require encrypted link

  esp_ble_auth_req_t authReq = ESP_LE_AUTH_REQ_SC_BOND; // Secure Connections + bonding
  // esp_ble_io_cap_t   iocap   = ESP_IO_CAP_NONE;          // no display/keyboard = Just Works
  esp_ble_io_cap_t iocap = ESP_IO_CAP_OUT;   // device can "display" a passkey (we hardcode it)
  uint8_t keySize            = 16;
  uint8_t authOption         = ESP_BLE_ONLY_ACCEPT_SPECIFIED_AUTH_DISABLE;
  uint8_t initKey  = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
  uint8_t respKey  = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;

  esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &authReq, sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &keySize, sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_ONLY_ACCEPT_SPECIFIED_SEC_AUTH, &authOption, sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &initKey, sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &respKey, sizeof(uint8_t));

  uint32_t passkey = 123456;   //6-digit pairing PIN
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, &passkey, sizeof(uint32_t));

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());
  BLEDevice::setSecurityCallbacks(new SecurityCallbacks());

  BLEService* pService = pServer->createService(BLEUUID(SERVICE_UUID), 40);

  const char* fsrUUIDs[5] = { CHAR_UUID_FSR1, CHAR_UUID_FSR2, CHAR_UUID_FSR3, CHAR_UUID_FSR4, CHAR_UUID_FSR5 };

  for (int i = 0; i < 5; i++) {
    pCharFSR[i] = pService->createCharacteristic(fsrUUIDs[i], BLECharacteristic::PROPERTY_NOTIFY);
    pCharFSR[i]->addDescriptor(new BLE2902());
    pCharFSR[i]->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);
  }

  pCharTemp = pService->createCharacteristic(CHAR_UUID_TEMP, BLECharacteristic::PROPERTY_NOTIFY);
  pCharTemp->addDescriptor(new BLE2902());
  pCharTemp->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);

  pCharBSOC = pService->createCharacteristic(CHAR_UUID_BSOC, BLECharacteristic::PROPERTY_NOTIFY);
  pCharBSOC->addDescriptor(new BLE2902());
  pCharBSOC->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);

  pService->start();

  BLEAdvertising* pAdv = BLEDevice::getAdvertising();
  pAdv->addServiceUUID(SERVICE_UUID);
  pAdv->setScanResponse(true);
  pAdv->setMinPreferred(0x06);
  BLEDevice::startAdvertising();
  Serial.println("[BLE] Advertising as 'Insole Monitor v1.0' (7 characteristics)");

  BUF[0] = '\0';
}

void loop() {
  updateLED();

  if (millis() - lastRunTime >= RUNTIME_INTERVAL_MS) {
    takeReadings();

    serializeData();

    if (bleConnected) {
      for(int i = 0; i < 5; i++) {
        notifyFString(pCharFSR[i], GRAMS[i]);
      }
      notifyFString(pCharTemp, TEMPERATURE[0]);
      notifyFString(pCharBSOC, VBAT_PERCENT);
    }

    Serial.printf("Data: %s\t\tSize: %d\n", BUF, strlen(BUF));

    lastRunTime = millis();
  }

}
