// Settings
#define DEVICE_ID 0x1 // 4 bit device id
#define SENSOR_DELAY 600000 // ten minutes
#define MSG_LEN 4 // bytes
//#define DEBUG_SERIAL
#define CRYPTO
//#define DEBUG_CRYPTO
//#define BLINK_WR // blink with internal LED after data transmission
#define DHT_11 // Temperature and humidity sensor
#define FC37 // Rain Sensor

#include <Wire.h>
#ifdef CRYPTO
#include <AES.h>
#endif
#include <VirtualWire.h>
#ifdef DHT_11
#include <DHT.h>
#endif

// Wireless Transmitter 
#define WT_ATTEMPTS 3
#define WT_RF_PIN 5

// DHT-11 temperature and humidity sensor
#ifdef DHT_11
#define DHTTYPE DHT11
#define DHTPIN A1
DHT dht(DHTPIN, DHTTYPE);
#endif

/* FC-37 Connection pins:
Arduino     Rain Sensor
  A0         Analog A0
  5V           VCC
  GND          GND
  - If the Sensor Board is completely soaked; "case 0" will be activated and " Flood " will be sent to the serial monitor.
  - If the Sensor Board has water droplets on it; "case 1" will be activated and " Rain Warning " will be sent to the serial monitor.
  - If the Sensor Board is dry; "case 2" will be activated and " Not Raining " will be sent to the serial monitor. 
*/
// lowest and highest sensor readings:
#ifdef FC37
#define FC37_MIN 0 // sensor minimum
#define FC37_MAX 1024 // sensor maximum
#define FC37_PIN A2
#endif

#ifdef BLINK_WR
#define LED_PIN 13
#define BLINK_DELAY 1000
#endif

#ifdef CRYPTO
// Cryptography
#define CRMSG_LEN 16 // bytes
AES aes;
byte *key = (unsigned char*)"****************";
unsigned long long int my_iv = 00000000;
#endif

#ifdef BLINK_WR
void blinkInternal() {
  digitalWrite(LED_PIN, HIGH);
  delay(BLINK_DELAY);
  digitalWrite(LED_PIN, LOW);
}
#endif

#ifdef FC37
// 0 - Flood
// 1 - Rain Warning
// 2 - Sensor Dry
int getFC37Measurement() {
  return map(analogRead(FC37_PIN), FC37_MIN, FC37_MAX, 0, 3);
}
#endif

void setup() {
  vw_set_tx_pin(WT_RF_PIN);
  vw_setup(2000);
  
#ifdef DHT_11
  dht.begin();
#endif  
}

void loop() {

  unsigned long msg = 0;
  msg |= DEVICE_ID;
  // message format: deviceId humidity  temperature
  //           bits         4        7            6

  float humidity = 0;
  float temperature = 0;
  int rain = 3; // invalid value
  
#ifdef DHT_11
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  humidity = dht.readHumidity();
  temperature = dht.readTemperature();
  
  if (!isnan(temperature) && !isnan(humidity)) {
    msg <<= 7;
    msg |= (int) humidity;
    msg <<= 6;
    msg |= (int) temperature;
  }

#endif

#ifdef FC37
  rain = getFC37Measurement();

  msg <<= 2;
  msg |= (int) rain;  
#endif

  msg <<= 13;

#ifdef DEBUG_SERIAL
  unsigned long savedMsg = msg;
#endif

  byte txmsg[MSG_LEN];
  for (int i = MSG_LEN - 1; i >= 0; i--) {
    txmsg[i] = msg & 0xFF;
    msg >>= 8;
  }
  
#ifdef CRYPTO
  aes.iv_inc();

  byte iv [N_BLOCK] ;
  byte cipher [CRMSG_LEN];
  
  aes.set_IV(my_iv);
  aes.get_IV(iv);
  aes.do_aes_encrypt(txmsg, MSG_LEN + 1, cipher, key, 128, iv);

#ifdef DEBUG_CRYPTO
  byte check [CRMSG_LEN];
  aes.set_IV(my_iv);
  aes.get_IV(iv);
  aes.do_aes_decrypt(cipher, CRMSG_LEN, check, key, 128, iv);
  
#ifdef DEBUG_SERIAL
  Serial.print("Decrypt: ");
  for (int i = 0; i < MSG_LEN; i++)
  {
    Serial.print(check[i], BIN);
  }
  Serial.println();
#endif
#endif

  vw_send(cipher, CRMSG_LEN);
#else
  vw_send(txmsg, MSG_LEN);
#endif

  vw_wait_tx();

#ifdef BLINK_WR
  blinkInternal();
#endif

#ifdef DEBUG_SERIAL
  Serial.print("Message: ");
  Serial.print(savedMsg, BIN);
  Serial.print(", DeviceId: ");
  Serial.print(DEVICE_ID);
  Serial.print(", Temperature (*C): ");
  Serial.print(temperature);
  Serial.print(", Humidity (%): ");
  Serial.print(humidity);
  Serial.println();
#endif

  delay(SENSOR_DELAY);

}


