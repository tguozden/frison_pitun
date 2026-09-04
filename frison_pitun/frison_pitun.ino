/* Heltec Automation Ping Pong communication test example
 *
 * Function:
 * 1. Ping Pong communication in two esp32 device.
 * 
 * Description:
 * 1. Only hardware layer communicate, no LoRaWAN protocol support;
 * 2. Download the same code into two esp32 devices, then they will begin Ping Pong test each other;
 * 3. This example is for esp32 hardware basic test.
 *
 * HelTec AutoMation, Chengdu, China
 * 成都惠利特自动化科技有限公司
 * www.heltec.org
 *
 * this project also realess in GitHub:
 * https://github.com/Heltec-Aaron-Lee/WiFi_Kit_series
 * */

#include "LoRaWan_APP.h"
#include "Arduino.h"
#include "driver/rtc_io.h"
#include "float16.h"
#include <Adafruit_AHT10.h>


#define RF_FREQUENCY                                 926470000 // Hz
#define RF_FREQUENCY2                                914470000 // Hz

#define TX_OUTPUT_POWER                             14        // dBm

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

uint8_t packet[30];

static RadioEvents_t RadioEvents;
void OnTxDone( void );
void OnTxTimeout( void );
void OnRxDone( uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr );

typedef enum
{
    LOWPOWER,
    STATE_RX,
    STATE_TX
}States_t;

// int16_t txNumber;
States_t state;

int millis0 = millis() + 45000;
int voltaje;
Adafruit_AHT10 aht;
sensors_event_t temp,hum;
uint16_t raw;
float16 t16=12.34;
uint8_t ihum=123;

#include "frison_pitun.h"

void setup() {
    Serial.begin(115200);
    Mcu.begin(HELTEC_BOARD,SLOW_CLK_TPYE);
    
    RadioEvents.TxDone = OnTxDone;
    RadioEvents.TxTimeout = OnTxTimeout;
    RadioEvents.RxDone = OnRxDone;

    Radio.Init( &RadioEvents );
    Radio.SetChannel( RF_FREQUENCY );
    Radio.SetTxConfig( MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                                   LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                                   LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                                   true, 0, 0, LORA_IQ_INVERSION_ON, 3000 );

    Radio.SetRxConfig( MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                                   LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                                   LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                                   0, true, 0, 0, LORA_IQ_INVERSION_ON, true );
    state=STATE_RX;
		analogReadResolution(12);  // para leer el voltage batería
  
    // Set pin 37 as an output pin (used for ADC control):
    VextON();
    pinMode(37, OUTPUT);
    // Set pin 37 to HIGH (enable ADC control):
    digitalWrite(37, HIGH);
    delay(100);
    // Read the analog millivolts from pin 1:
    voltaje = analogReadMilliVolts(1)*490/100;

    packet[25] = (uint8_t) (0.255 *  (voltaje - 3300));  
    Serial.printf("volts  %d %d\n", voltaje, packet[25]);
    
    
    /* temperatura y humeda, espero que ande*/
    pinMode(LED, OUTPUT);
    digitalWrite(LED, HIGH);
    for(int i = 0; i<10 ; i++){ //parece ser que a veces precisa más tiempo el sensor
    
      delay(100);
      if(! aht.begin())  Serial.println("Could not find AHT10? Check wiring");
      else{
        i=10;
        aht.getEvent(&hum, &temp);// populate temp and humidity objects with fresh data
        t16=temp.temperature;
        ihum=hum.relative_humidity;
        
      }
    }
    Serial.printf("temp = %f, hum = %d\n",t16.toDouble(), (int)ihum);
    raw=t16.getBinary();
    memcpy(packet+26,&raw,2);
    memcpy(packet+28,&ihum,1);
    digitalWrite(LED, LOW);

}



void loop()
{	
  switch(state)
  {
    case STATE_TX:
      
      Serial.println("STATE_TX");
      
      Serial.printf("\r\nsending packet\r\n" );

      Radio.SetChannel( RF_FREQUENCY );
      Radio.Send(packet, 32 );
      state=LOWPOWER;
      break;
    case STATE_RX:
      Serial.println("STATE_RX");

      Radio.SetChannel( RF_FREQUENCY2 );  
      Radio.Rx( 0 );
      state=LOWPOWER;
			// millis0=millis()+45000;
      break;
    case LOWPOWER:
		  // Serial.println("LOWPOWER");
			// Serial.println(millis0-millis());
      Radio.IrqProcess( );
		  break;
    default:
      break;
  }
	delay(20);
	if(millis() > millis0) duermelora();
}


void OnTxDone( void )
{
  Serial.print("TX done......");
  state=STATE_RX;


	millis0=millis()+1000; //doy un segundo más por si viene otro paquete
  digitalWrite(LED, HIGH);  //y se apaga solo
}

void OnTxTimeout( void )
{
    Radio.Sleep( );
    Serial.print("TX Timeout......");
    state=STATE_TX;
}

void OnRxDone( uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr )
{
	Serial.printf("\r\nreceived packet with Rssi %d , length %d\r\n",rssi,size);
  if(size==26){
		
		if(payload[25]==calcular_checksum_xor(payload,25)){
			
			Serial.println("coincide checksum");
      Serial.printf("voltaje gateway (payload[24]) %d\n", payload[24]);

			memcpy(packet, payload, size-1);  //todos menos el último!!
			// packet[25]=0; //puse voltaje
			// packet[26]=0;temp
			// packet[27]=0;temp
			// packet[28]=0;humedad
      packet[29]=(uint8_t)rssi;
      packet[30]=snr;
      Serial.printf("rssi = %d, snr = %d;\n",packet[29],packet[30]);
			packet[31]=calcular_checksum_xor(packet,31);
      Serial.printf("voltaje recibido %d \n", packet[24]);  
			
	    Radio.Sleep( );
			state=STATE_TX;		
		}
		else Serial.println("no coincide check");
	}
 
}