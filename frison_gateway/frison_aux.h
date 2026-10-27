void VextON();
void VextOFF();
uint8_t calcular_checksum_xor(uint8_t* datos, uint8_t longitud);

void onReceiveFunction(void) {
  // This is a callback function that will be activated on UART RX events
  size_t available = Serial1.available();

  Serial.printf("recibidos %d datos\n", available);
  if(available==26){
    for(int i=0;i<available;i++){
      txpacket[i]= Serial1.read();
      Serial.printf("%02x", txpacket[i]);
    }
    Serial.println();
    // sincronizo el próximo arranque de la raspi para que calce con la hora
    uint32_t seconds_25; //segundos desde el ano nuevo 2025 utc?
    memcpy(&seconds_25, &txpacket[0], 4);
    Serial.printf("seconds_25 = %d\n",seconds_25);
    //redondeo al minuto, esto es necesario solo en la etapa de simulación
    // después va a quedar igual no se preocupen
    seconds_25-=seconds_25%60;
    Serial.printf("seconds_25 = %d\n",seconds_25);
    memcpy(&txpacket[0], &seconds_25, 4);

    uint16_t seconds_offset;
    memcpy(&seconds_offset, &txpacket[24], 2);
    
    Serial.printf("seconds_offset = %d\n",seconds_offset);
    // acordarse de modificar el tsleep en el tiempo inicial
    tsleep=TSLEEP - seconds_offset + 60; // próximo arranque va a ser una hora más 60 + lo que se demore toda la gilada
    
    Serial.printf("tsleep = %d\n", tsleep);

    


    
    // agrego checksum
    txpacket[24] = (uint8_t) (0.255 *  (voltaje - 3300));  //porque lo había pisado
    txpacket[25]=calcular_checksum_xor(txpacket,25);
    for(int i =0;i<26;i++) Serial.printf("%x-",txpacket[i]);
    Serial.println();

    if(lora_idle){
      Radio.Send(txpacket, 26);
      lora_idle = false;
    }
    while(lora_idle==false) Radio.IrqProcess();
    millis0 = millis() + 3000; // dejo 3 segundos
    received_packet=true;

  }else{
    // paquete falso (no debería ocurrir, pero pasa cuando baja la tensión de la batería)
    digitalWrite(LED, HIGH);  
    delay(20);
    digitalWrite(LED, LOW); 
    for(int i=0;i<available;i++)  Serial1.read();
  }
}



void apaga(){
	
	Radio.Sleep();
	pinMode(RADIO_DIO_1,ANALOG);
	pinMode(RADIO_NSS,ANALOG);
	pinMode(RADIO_RESET,ANALOG);
	pinMode(RADIO_BUSY,ANALOG);
	pinMode(LORA_CLK,ANALOG);
	pinMode(LORA_MISO,ANALOG);
	pinMode(LORA_MOSI,ANALOG);
	esp_sleep_enable_timer_wakeup(tsleep/NBOOTS*1000*(uint64_t)1000);
  //delay(2000); // los 3 segundos que agrego por las dudas de un siguiente dato ya
  // deberían ser suficientes para apagar la raspy
  VextOFF();
  

  rtc_data.bootCount++;
  Serial.printf("apagando, tsleep = %d\n", tsleep/NBOOTS);
	esp_deep_sleep_start();
 }

void OnTxDone( void )
{
	Serial.println("TX done......");
  lora_idle = true;
  
  pinMode(LED ,OUTPUT);
  digitalWrite(LED, HIGH);  
  delay(50);
  digitalWrite(LED, LOW); 
}

void OnTxTimeout( void )
{
    Radio.Sleep( );
    Serial.println("TX Timeout......");
    lora_idle = true;
}



void VextON(void)
{
  pinMode(Vext,OUTPUT);
  digitalWrite(Vext, LOW);
  
}

void VextOFF(void) //Vext default OFF
{
  pinMode(Vext,OUTPUT);
  digitalWrite(Vext, HIGH);
}


uint8_t calcular_checksum_xor(uint8_t* datos, uint8_t longitud) {
    const uint8_t clave_secreta[] = {'t','o','m','i'};
    const uint8_t clave_len = 4;
    
    uint8_t checksum = 0x5F;  // Semilla inicial fija

    // /*se hace al final, justo antes del checksum*/
    // uint32_t seconds_25;
    // memcpy(&seconds_25, &datos[0], 4);    
    // if (raspi_on)
    //   seconds_25 -= NBOOTS/2;
    // else
    //   seconds_25 += 1;//rtc_data.bootCount%11-5;

    // memcpy(&datos[0], &seconds_25, 4);
    

    
    // Primera pasada: XOR datos con clave
    for(uint8_t i = 0; i < longitud; i++) {
        checksum ^= datos[i];
        checksum ^= clave_secreta[i % clave_len];
    }        
    return checksum;
}

