
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

void duermelora(){
  // Serial.println("into RX mode");
  // Radio.Rx(0);
  VextOFF();
  esp_sleep_enable_ext0_wakeup((gpio_num_t)RADIO_DIO_1, 1);  //1 = High, 0 = Low
  rtc_gpio_pullup_dis((gpio_num_t)RADIO_DIO_1);
  rtc_gpio_pulldown_en((gpio_num_t)RADIO_DIO_1);
  Serial.println("Going to sleep now");
  // digitalWrite(LED,LOW);
  // delay(1000);
  esp_deep_sleep_start();
  Serial.println("This will never be printed");
}

uint8_t calcular_checksum_xor(const uint8_t* datos, uint8_t longitud) {
    const uint8_t clave_secreta[] = {'t','o','m','i'};
    const uint8_t clave_len = 4;
    
    uint8_t checksum = 0x5F;  // Semilla inicial fija
    
    // Primera pasada: XOR datos con clave
    for(uint8_t i = 0; i < longitud; i++) {
        checksum ^= datos[i];
        checksum ^= clave_secreta[i % clave_len];
    }        
    return checksum;
}
