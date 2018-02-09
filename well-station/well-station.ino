// Settings
#define DEVICE_ID 0x0 // 4 bit device id
#define DELAY 600000 // ten minutes
#define MSG_LEN 4 // bytes
//#define DEBUG_SERIAL
#define CRYPTO
//#define DEBUG_CRYPTO
//#define URM37
#define SR04
//#define BLINK_WR // blink with internal LED after data transmission

#include <Wire.h>
#ifdef CRYPTO
#include <AES.h>
#endif
#ifdef URM37
#include "URMSerial.h" // Ultrasonic URM37
#endif
#include <VirtualWire.h>

// URM37 Pinout
// # Connection:
// #       VCC (Arduino)   -> Pin 1 VCC (URM37 V3.2)
// #       GND (Arduino)   -> Pin 2 GND (URM37 V3.2)
// #       VCC (Arduino)   -> Pin 7 NC  (URM37 V3.2)
// #       Pin 6 (Arduino) -> Pin 9 TXD (URM37 V3.2)
// #       Pin 7 (Arduino) -> Pin 8 RXD (URM37 V3.2)

// HC-SR04 Pinout
// VCC  -> +5V
// TRIG ->  D9
// ECHO ->  D8
// GND  ->  GND

// URM37 PINs
#ifdef URM37
#define URM37_PIN_RX 7
#define URM37_PIN_TX 6
#define URM37_MT_TEMPERATURE 2
#endif

// HC-SR04 Pins
#ifdef SR04
#define SR04_TRIG 9
#define SR04_ECHO 8
#endif

#ifdef URM37
// URM37 Measurement Type
#define URM37_MT_DISTANCE 1
#define URM37_MT_TEMPERATURE 2
// URM37 Measurement Return Codes
#define URM37_RET_DISTANCE 1
#define URM37_RET_TEMPERATURE 2
#define URM37_RET_ERROR 3
#define URM37_RET_NOTREADY 4
#define URM37_RET_TIMEOUT 5
// URM37 Measurement Error Results
#define URM37_RES_ERROR -1
#define URM37_RES_NOTREADY -2
#define URM37_RES_TIMEOUT -3
#endif

#ifdef BLINK_WR
#define LED_PIN 13
#define BLINK_DELAY 500
#endif

// Wireless Transmitter 
#define WT_ATTEMPTS 3
#define WT_RF_PIN 5

#ifdef URM37
// Ultrasonic URM37
URMSerial urm;
#endif

#ifdef CRYPTO
// Cryptography
#define CRMSG_LEN 16 // bytes
AES aes;
byte *key = (unsigned char*)"****************";
unsigned long long int my_iv = 00000000;
#endif

#ifdef BLINK_WR
void blinkInternal()
{
  digitalWrite(LED_PIN, HIGH);
  delay(BLINK_DELAY);
  digitalWrite(LED_PIN, LOW);
  delay(BLINK_DELAY);
}
#endif

#ifdef URM37  
int getURM37Measurement(int type)
{
  int urm_value;
  switch(urm.requestMeasurementOrTimeout(type, urm_value))
  {
  case URM37_RET_DISTANCE:
    return urm_value;
    break;
  case URM37_RET_TEMPERATURE:
    return urm_value;
    break;
  case URM37_RET_ERROR:
    return URM37_RES_ERROR;
    break;
  case URM37_RET_NOTREADY:
    return URM37_RES_NOTREADY;
    break;
  case URM37_RET_TIMEOUT:
    return URM37_RES_TIMEOUT;
    break;
  } 
}
#endif

#ifdef SR04

unsigned int impulseTime = 0; 
unsigned int distance_sm = 0;

int getSR04Measurement() {
  // measure cycle delay
  delay(100);
  
  // 10ms Trigger impulse
  digitalWrite(SR04_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(SR04_TRIG, HIGH); 
  delayMicroseconds(10);
  digitalWrite(SR04_TRIG, LOW);

  // measure impulse length and convert to mm
  impulseTime = pulseIn(SR04_ECHO, HIGH);
  distance_sm = (impulseTime / 2.76233) / 2;
  
  return distance_sm;
}
#endif

void setup() {

#ifdef DEBUG_SERIAL
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }
#endif

#ifdef BLINK_WR
  pinMode(13, OUTPUT);
#endif
  
#ifdef URM37
  urm.begin(URM37_PIN_RX, URM37_PIN_TX, 9600); // RX Pin, TX Pin, Baud Rate
#endif

#ifdef SR04
  pinMode(SR04_TRIG, OUTPUT);
  pinMode(SR04_ECHO, INPUT);
#endif
  
  vw_set_tx_pin(WT_RF_PIN);
  vw_setup(2000);
}

void loop() {
  
#ifdef URM37  
  int distance = getURM37Measurement(URM37_MT_DISTANCE);
#endif
#ifdef SR04
  int distance = getSR04Measurement();
#endif  
  
  // message format: deviceId distance
  //           bits         4       12
  unsigned long msg = 0;
  msg |= DEVICE_ID;
  msg <<= 12;
  msg |= (distance < 20 || distance > 4000) ? 0 : distance; // 20-4000mm sensor working range
  msg <<= 16;
  msg |= 0;

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
  Serial.print(", Distance: ");
  Serial.println(distance);
#endif

  delay(DELAY);
}

