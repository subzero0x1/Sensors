// Settings
#define MSG_LEN 4 // bytes
//#define DEBUG_SERIAL
#define CRYPTO
//#define DEBUG_CRYPTO
#define BLINK_WR // blink with LED after data transmission
#define BLINK_PWR // blink with LED when power is ON
#define DHT_11 // Temperature and humidity sensor
#define BMP85 // Temperature and pressure sensor
#define SENSOR_DELAY 600 // sec.

#include <Arduino.h>
#include <Wire.h>
#ifdef CRYPTO
#include <AES.h>
#endif
#include <VirtualWire.h>
#ifdef BMP85
#include <Adafruit_BMP085.h> // Barometer BMP085
#endif
#ifdef DHT_11
#include <DHT.h>
#endif

// Wireless Receiver 
#define WR_RF_PIN 12

// DHT-11 temperature and humidity sensor
#ifdef DHT_11
#define DHTTYPE DHT11
#define DHTPIN A0
#define DHT_11_ID 2
DHT dht(DHTPIN, DHTTYPE);
#endif

// Barometer BMP085 Pinout
// Connect VCC of the BMP085 sensor to 3.3V (NOT 5.0V!)
// Connect GND to Ground
// Connect SCL to i2c clock - on '168/'328 Arduino Uno/Duemilanove/etc thats Analog 5
// Connect SDA to i2c data - on '168/'328 Arduino Uno/Duemilanove/etc thats Analog 4
// EOC is not used, it signifies an end of conversion
// XCLR is a reset pin, also not used here
// Barometer BMP085
#ifdef BMP85
Adafruit_BMP085 bmp;
#define BMP85_ID 3
#endif

// Cryptography
#ifdef CRYPTO
#define CRMSG_LEN 16 // bytes
AES aes;
byte *key = (unsigned char*)"****************";
unsigned long long int my_iv = 00000000;
#endif

#ifdef CRYPTO
#define WR_MSG_LEN CRMSG_LEN
#define MSG_ARR decr
#else
#define WR_MSG_LEN MSG_LEN
#define MSG_ARR buf
#endif

#ifdef BLINK_WR
#define WR_LED_PIN 3
#define BLINK_DELAY 1000

void blinkWr()
{
  digitalWrite(WR_LED_PIN, HIGH);
  delay(BLINK_DELAY);
  digitalWrite(WR_LED_PIN, LOW);
  delay(BLINK_DELAY);
}
#endif

#ifdef BLINK_PWR
#define PWR_LED_PIN 2

void turnPwrLED()
{
  digitalWrite(PWR_LED_PIN, HIGH);
}
#endif

#ifdef DHT_11
unsigned long getDHT11Measurements() {
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  int h = (int) dht.readHumidity();
  int t = (int) dht.readTemperature();
  
  if (isnan(t) || isnan(h)) {
    return 0;
  }
  
  // message format: deviceId humidity  temperature
  //           bits         4       12           16
  unsigned long msg = 0;
  msg |= DHT_11_ID;
  msg <<= 12;
  msg |= h % 0xFFF;
  msg <<= 16;
  msg |= t % 0xFFFF;
    
#ifdef DEBUG_SERIAL
  Serial.print("DeviceId: ");
  Serial.print(DHT_11_ID);
  Serial.print(", Humidity (%): ");
  Serial.print(h);
  Serial.print(", Temperature (*C): ");
  Serial.println(t);
  Serial.println(msg, BIN);
  Serial.println(msg, DEC);
#endif
    
  return msg;  
}
#endif

#ifdef BMP85
unsigned long getBMP85Measurements() {
  byte temperature = (byte) bmp.readTemperature(); // temperature: *C
  int pressure = (int) (bmp.readPressure() * 0.00750063755419211); // pressure: mmHg = Pa * 0.00750063755419211

  // message format: deviceId temperature pressure
  //           bits         4           6       10
  unsigned long msg = 0;
  msg |= BMP85_ID;
  msg <<= 6;
  msg |= temperature % 0x3F;
  msg <<= 10;
  msg |= pressure % 0x3FF;
  msg <<= 12;
  
#ifdef DEBUG_SERIAL
  Serial.print("DeviceId: ");
  Serial.print(BMP85_ID);
  Serial.print(", Temperature (*C): ");
  Serial.print(temperature);
  Serial.print(", Pressure (mmHg): ");
  Serial.println(pressure);
  Serial.println(msg, BIN);
  Serial.println(msg, DEC);
#endif  

  return msg;
}
#endif

void setup() {
  int bmpInit = bmp.begin();
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }  
#ifdef DEBUG_SERIAL  
  if (!bmpInit) {
    Serial.println("Barometer BMP085 not found! Check wiring!");
  }
#endif

#ifdef BLINK_WR
  pinMode(WR_LED_PIN, OUTPUT);
#endif
  
  vw_set_rx_pin(WR_RF_PIN);
  vw_setup(2000);
  vw_rx_start();
  
#ifdef DHT_11
  dht.begin();
#endif
}

int sensorDelay = 0;

void loop() {
  turnPwrLED();
  
  uint8_t buf[VW_MAX_MESSAGE_LEN];
  uint8_t buflen = VW_MAX_MESSAGE_LEN;
  unsigned long msg = 0;

  if (vw_get_message(buf, &buflen))
  {
#ifdef DEBUG_SERIAL
    Serial.print("msg received, length: ");
    Serial.print(buflen);
    Serial.println();    
    for (int i = 0; i < buflen; i++)
    {
      Serial.print(buf[i], BIN);
    }
    Serial.println();    
#endif    
    if (buflen != WR_MSG_LEN) {
      return;
    }
#ifdef BLINK_WR
  blinkWr();
#endif

#ifdef CRYPTO
    byte iv [N_BLOCK];
    byte decr[CRMSG_LEN];
    aes.set_IV(my_iv);
    aes.get_IV(iv);
    aes.do_aes_decrypt(buf, CRMSG_LEN, decr, key, 128, iv);

#ifdef DEBUG_CRYPTO    
    Serial.print("Decrypt: ");
    for (int i = 0; i < MSG_LEN; i++)
    {
      Serial.print(decr[i], BIN);
    }
    Serial.println();
#endif
#endif
    
    for (int i = 0; i < MSG_LEN - 1; i++)
    {
      msg |= MSG_ARR[i];
      msg <<= 8;
    }
    msg |= MSG_ARR[MSG_LEN - 1];

    Serial.println(msg);
    
#ifdef DEBUG_SERIAL
    int deviceId = ((msg >> 28) & 0xF);
    Serial.print("DeviceId: ");
    Serial.print(deviceId);
    if (deviceId == 0) // Well Station
    {
      Serial.print(", Distance (mm): ");
      Serial.println((msg >> 16) & 0xFFF);
    }
    else if (deviceId == 1) // External Weather Station
    {
      Serial.print(", Humidity (%): ");
      Serial.print((msg >> 21) & 0x7F);
      Serial.print(", Temperature (*C): ");
      Serial.print((msg >> 15) & 0x3F);
      Serial.print(", Rain(0-rain, 1-warn, 2-dry): ");
      Serial.println((msg >> 13) & 0x3);    
    }
#endif
  }  
  
  if (sensorDelay == SENSOR_DELAY) {
    sensorDelay = 0;
    
#ifdef DHT_11
  Serial.println(getDHT11Measurements());
#endif

#ifdef BMP85
  Serial.println(getBMP85Measurements());
#endif
  } else {
    sensorDelay++;
  }
  
  delay(1000);
}
