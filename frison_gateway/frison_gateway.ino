/*

*/
#include "LoRaWan_APP.h"
#include <Arduino.h>




// There are two ways to make this sketch work:
// By physically connecting the pins 4 and 5 and then create a physical UART loopback,
// Or by using the internal IO_MUX to connect the TX signal to the RX pin, creating the
// same loopback internally.
// #define USE_INTERNAL_PIN_LOOPBACK 0  // 1 uses the internal loopback, 0 for wiring pins 4 and 5 externally

// #define DATA_SIZE 26    // 26 bytes is a lower than RX FIFO size (127 bytes)
#define BAUD      9600  // Any baudrate from 300 to 115200
#define TEST_UART 1     // Serial1 will be used for the loopback testing with different RX FIFO FULL values
#define RXPIN     2     // GPIO 4 => RX for Serial1
#define TXPIN     3     // GPIO 5 => TX for Serial1

#define RF_FREQUENCY                                914470000 // Hz   frisón y bolsón los mismos

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
#define BUFFER_SIZE                                 26 // Define the payload size here

/*    boot counter  */
RTC_DATA_ATTR struct {
  uint32_t magic;
  int bootCount;
} rtc_data;
#define MAGIC_VALUE 0xDEADBEEF
RTC_DATA_ATTR uint8_t txpacket[BUFFER_SIZE];
#define NBOOTS 10
#define TSLEEP 3600

bool received_packet=false;
bool lora_idle=true;
bool raspi_on=false;
int millis0 = millis() + 45000; //45 segundos es lo que queda prendido, en caso de mensaje se acorta a 3 segundos
int voltaje;
RTC_DATA_ATTR int tsleep;

#include "frison_aux.h"

static RadioEvents_t RadioEvents;
void OnTxDone( void );
void OnTxTimeout( void );

// ##########################################################################
void setup() {
  // falta medir la batería
  // UART0 will be used to log information into Serial Monitor
  Serial.begin(115200);
  Mcu.begin(HELTEC_BOARD,SLOW_CLK_TPYE); // la radio?
  if (rtc_data.magic != MAGIC_VALUE) {
    // First boot or power loss reset
    Serial.println("no coincide magic value! encendido inicial");
    rtc_data.magic = MAGIC_VALUE;
    rtc_data.bootCount = 0;
    raspi_on=true;
    txpacket[0]=0;
    txpacket[1]=0;
    txpacket[2]=0;
    txpacket[3]=0;
    tsleep = TSLEEP;
  }
  /*!!!!!!!!!!!!!!!!
  
                  cada N = 11 ciclos      !!!!!!!!!!!!!!!!!!!!!!!!1
  !!!!!!!!!!!!!!!!!!!!                       
                                              */
  if(rtc_data.bootCount%NBOOTS==0) raspi_on=true; //cada NBOOTS ciclos prende la raspi

  RadioEvents.TxDone = OnTxDone;
  RadioEvents.TxTimeout = OnTxTimeout;
  
  Radio.Init( &RadioEvents );
  Radio.SetChannel( RF_FREQUENCY );
  Radio.SetTxConfig( MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                                  LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                                  LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                                  true, 0, 0, LORA_IQ_INVERSION_ON, 3000 ); 
  
  
  Serial.println("booting...");
  
  //2 pulsos para despertar el bridge
  for(int i=0;i<2;i++){
    uint8_t wupacket[5] = {1,0,1,0,1};
    if(lora_idle){
        Radio.Send(wupacket, 5);
        lora_idle = false;
    }
    while(lora_idle==false) Radio.IrqProcess();
    delay(100);
    Serial.println("despertando a pitún");
  }

  pinMode(LED ,OUTPUT);
  /* inicializo el medidor de batería*/
  // Set the resolution of the analog-to-digital converter (ADC) to 12 bits (0-4095):
  analogReadResolution(12);  // para leer el voltage batería
  // Set pin 37 as an output pin (used for ADC control):
  pinMode(37, OUTPUT);
  // Set pin 37 to HIGH (enable ADC control):
  digitalWrite(37, HIGH);
  delay(100);
  // Read the analog voltage in millivolts from pin 1:
  voltaje = analogReadMilliVolts(1)*490/100;
  Serial.printf("volts befor VextON: %d\n", voltaje);
  txpacket[24] = (uint8_t) (0.255 *  (voltaje - 3300));
    
  
  if (voltaje < 3800){
    // APAGAR SI EL VOLTAJE CAE POR DEBAJO DE ... (achico a 3.8V, frío afecta)
    for (int i =0; i<10; i++){
      digitalWrite(LED, HIGH);  
      delay(50);
      digitalWrite(LED, LOW); 
      delay(50);
    }
    // tsleep = 2*TSLEEP;
    apaga();
  }
  
  // si ciclo lectura raspi
  if(raspi_on){  
    Serial.println("prendo raspi");
    VextON();  
    delay(17500); //no borrar esto es lo que encontramos con nico: la raspy se demora en arrancar, mientras la salida serie no está fija. espero antes de arrancar el puerto serie. parece ser que 17 segundos sería más que suficiente (menos también)
    Serial.println("prendo puerto serie");
    Serial1.begin(BAUD, SERIAL_8N1, RXPIN, TXPIN);  // Rx = 4, Tx = 5 will work for ESP32, S2, S3 and C3
    Serial1.onReceive(onReceiveFunction, 1);  // sets a RX callback function for Serial 1
  }
  
}

void loop() {
  Radio.IrqProcess( );
  if(raspi_on==false){
    Serial.println("ciclo sin raspi, espero un toque y mando");
    delay(2000);
    // vuelvo a agregar checksum porque cambió el voltaje
    txpacket[25]=calcular_checksum_xor(txpacket,25);
    
    if(lora_idle){
      Radio.Send(txpacket, 26);
      lora_idle = false;
    }
    while(lora_idle==false) Radio.IrqProcess();
  
    apaga();
  }

  delay(10); // agrego : baja el consumo

  // si tiempo de espera cumplido nos vimos
  if(millis() > millis0 ){
    if(!received_packet){
      Serial.println("no llegó mensaje!");
      tsleep = 3600;
    }
    apaga();
  }
  
}



