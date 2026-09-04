#define WIFI_SSID  "Fortin"//"bc811a" //  "TP-Link_BA62" ////
#define WIFI_PASS "Fortin123" //"nachiketas" // "25555288"//
#define DATACALL0 "acompletar"
                  //  ID=IELBOL22&PASSWORD=0OlkrAr3&dateutc=now&action=updateraw"
                  // "ID=IDEPAR23&PASSWORD=yQUiCRuM&dateutc=now&action=updateraw";
#define TOPIC  "frison"
#define MQTT_CLIENT "fortin"
///////////////////////////////
/* otto 3 - 103 sin checksum   */
//////////////////////////////

#include "LoRaWan_APP.h"
#include "Arduino.h"
#include "ArduinoJson.h"
#include <HTTPClient.h>
#include <PubSubClient.h>
#include "WiFi.h"
#include "HT_SSD1306Wire.h"

#include "float16.h"




#define RF_FREQUENCY                                926470000 // Hz   frisón y bolsón los mismos

#define TX_OUTPUT_POWER                             10        // dBm

#define LORA_BANDWIDTH                              0         // [0: 125 kHz,
                                                              //  1: 250 kHz,
                                                              //  2: 500 kHz,
                                                              //  3: Reserved]
#define LORA_SPREADING_FACTOR                       8         // [SF7..SF12]
#define LORA_CODINGRATE                             3         // [1: 4/5,
                                                              //  2: 4/6,
                                                              //  3: 4/7,
                                                              //  4: 4/8]
#define LORA_PREAMBLE_LENGTH                        8         // Same for Tx and Rx
#define LORA_SYMBOL_TIMEOUT                         0         // Symbols
#define LORA_FIX_LENGTH_PAYLOAD_ON                  false
#define LORA_IQ_INVERSION_ON                        false


#define RX_TIMEOUT_VALUE                            1000
#define BUFFER_SIZE                                 30 // Define the payload size here


uint8_t rxpacket[BUFFER_SIZE];
DynamicJsonDocument mqttBuffer(512);
JsonObject mqtt_root = mqttBuffer.to<JsonObject>();

static RadioEvents_t RadioEvents;
int16_t rssi,rxSize;
bool lora_idle = true;

int Npaq = 5401;
int Ntip = 0;
unsigned long tiempo_RX = 0;

int Col=0;                      // columna inicial del display oled
// String mensaje;
SSD1306Wire  fdisplay(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED); // addr , freq , i2c group , resolution , rst

NetworkClient wifiClient;
PubSubClient mqttClient(wifiClient);

#include "panel_HV3.h" 
#include "fineoffsetlib.h"  //funciones de recepcion, va acá porque depende de mil cosas





void setup() {
  fdisplay.init();
  fdisplay.clear();
  fdisplay.display();

  WIFISetUp();

  fdisplay.setFont(ArialMT_Plain_10);
  display_message(false,"Frec. " + String((float)RF_FREQUENCY/1000000));
  mqttClient.setServer("broker.emqx.io", 1883);

  Serial.begin(115200);
  Mcu.begin(HELTEC_BOARD,SLOW_CLK_TPYE);

  rssi=0;

  RadioEvents.RxDone = OnRxDone;
  Radio.Init( &RadioEvents );
  Radio.SetChannel( RF_FREQUENCY );
  Radio.SetRxConfig( MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                              LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                              LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                              0, true, 0, 0, LORA_IQ_INVERSION_ON, true );

  pinMode(LED, OUTPUT);
}



void loop()
{
  if(lora_idle)
  {
    lora_idle = false;
    Serial.println("into RX mode");
    Radio.Rx(0);
  }
  Radio.IrqProcess( );

  delay(10);
  
  timer_OLED((millis()-tiempo_RX)/1000);
}

void OnRxDone( uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr )
{

  rssi=rssi;
  rxSize=size;
  memcpy(rxpacket, payload, size );

  Radio.Sleep( );
  Serial.printf("\r\nreceived packet with rssi %d , length %d\r\n",rssi,rxSize);
  // for (int i =0; i<size; i++) Serial.printf("i%d: %02x, ",i,payload[i]);
  Serial.println();
  if(size == 32)
    if(rxpacket[31]==calcular_checksum_xor(rxpacket,31)){
      digitalWrite(LED, HIGH);
      delay(20);
      digitalWrite(LED, LOW);
      Serial.println(rxpacket[29],rxpacket[30]);

      procesa_paquete(rxpacket, rssi, snr );


    }
  // if(size == 18 && payload[0] == 2<<4 | 4 ){
  //   uint8_t checksum = 0;
  //   for (unsigned n = 0; n < 16; ++n) {
  //     checksum += rxpacket[n];
  //   }
  //   if (checksum == rxpacket[16]){
  //     Serial.println("checksum match");
  //     procesa_paquete(rxpacket, rssi, snr );
  //   }
  //   else{
  //     Serial.println("checksum failure");
  //   }

  // }

  tiempo_RX = millis(); //lap del cronómetro

  lora_idle = true;
}