#include <stdint.h>
// #include <TimeLib.h>

union BytesToFloat {
  float f;
  uint8_t b[4]; // Assuming the 2 bytes are part of a 4-byte structure
};

void procesa_paquete( uint8_t *rxpacket, int16_t rssi, int8_t snr ){
  // IeeeBBeeeeee
// globales
  // DynamicJsonDocument mqttBuffer(512);
  // JsonObject mqtt_root = mqttBuffer.to<JsonObject>();
  
  mqtt_root["rs"] = rssi;
  mqtt_root["sn"] = snr;
  
  
  String datacall = "/weatherstation/updateweatherstation.php?";
  datacall += DATACALL0;

  String key, varray;
  mqtt_root["vi"] = String(rxpacket[24] / 255.0 + 3.3, 2);   //misma cuenta que piltri
  
  uint32_t seconds_25;
  memcpy(&seconds_25, &rxpacket[0], 4);

  Serial.printf("seconds25 = %ld\n", seconds_25);  

  uint16_t raw;
  float16 raw_f16;

  key = "tempf";
    memcpy(&raw, rxpacket + 4, 2); 
    raw_f16.setBinary(raw);
    varray = String(raw_f16.toDouble(), 2);
    datacall += "&"+key+"="+varray;
    mqtt_root["t"] = varray;

  key = "dewptf";
    memcpy(&raw, rxpacket + 6, 2); 
    raw_f16.setBinary(raw);
    varray = String(raw_f16.toDouble(), 2);    // datacall += "&"+key+"="+varray;
    mqtt_root["d"] = varray;
  
key = "humidity";
    varray = String(rxpacket[8]);
    datacall += "&"+key+"="+varray;
    mqtt_root["h"] = varray;

key = "UV";
    varray = String(rxpacket[9]);
    datacall += "&"+key+"="+varray;
    mqtt_root["U"] = varray;

key = "winddir";
  memcpy(&raw, rxpacket + 10, 2); 
  varray = String(raw);
  datacall += "&"+key+"="+varray;
  mqtt_root["wd"] = varray;

key = "windspeedmph";
  memcpy(&raw, rxpacket + 12, 2); 
  raw_f16.setBinary(raw);varray = String(raw_f16.toDouble(), 2);
  datacall += "&"+key+"="+varray;
  mqtt_root["w"] = varray;

key = "windgustmph";
  memcpy(&raw, rxpacket + 14, 2); 
  raw_f16.setBinary(raw);
  varray = String(raw_f16.toDouble(), 2);
  datacall += "&"+key+"="+varray;
  mqtt_root["wg"] = varray;


key = "baromin";
  memcpy(&raw, rxpacket + 16, 2); 
  varray = String(raw/1000.0, 3); 
  datacall += "&"+key+"="+varray;
  mqtt_root["b"] = varray;

key = "solarradiation";
  memcpy(&raw, rxpacket + 18, 2); 
  raw_f16.setBinary(raw);
  varray = String(raw_f16.toDouble(), 1); 
  datacall += "&"+key+"="+varray;
  mqtt_root["s"] = varray;

key = "dailyrainin";
  memcpy(&raw, rxpacket + 20, 2); 
  raw_f16.setBinary(raw);
  varray = String(raw_f16.toDouble(), 2); 
  datacall += "&"+key+"="+varray;
  mqtt_root["dr"] = varray;
    
key = "rainin";
  key = "windspeedmph";
  memcpy(&raw, rxpacket + 22, 2); 
  raw_f16.setBinary(raw);
  varray = String(raw_f16.toDouble(), 2); 
  datacall += "&"+key+"="+varray;
  mqtt_root["r"] = varray;

  
  Serial.println(datacall);
  
  String rta, rta_mqtt;
  if(WiFi.status() == WL_CONNECTED){
//     HTTPClient http;
//     String serverPath = "http://weatherstation.wunderground.com" + datacall;
//     Serial.println(serverPath);
//     http.begin(serverPath.c_str());

// // Send HTTP GET request
//     int httpResponseCode = http.GET();
    
//     if (httpResponseCode > 0) {
//       // Serial.print("HTTP Response code: ");
//       // Serial.println(httpResponseCode);
//       rta = "ok";//http.getString();
//       Serial.println(rta);
//       // display_message(0, rta);
//     }
//     else {
//       Serial.print("Error code: ");
//       Serial.println(httpResponseCode);
//       rta = "failed";//http.getString();
//       Serial.println(rta);
// 		  // display_message(0, rta);
//     }
//     // Free resources
//     http.end();
//*******************************************************************************/
//luego del http vamos con el mqtt
    char buffer[512];
    serializeJson(mqtt_root, buffer);
    Serial.println(buffer);
    // idea: mandar el paquetito a mqtt
    // char crudos[120];
    // for (int i=0;i<size;i++) sprintf(crudos+2*i, "%02x", payload[i]);
    // crudos[2*size]=0;
    if (mqttClient.connect(MQTT_CLIENT)) {
      mqttClient.publish(TOPIC, buffer);
      rta_mqtt = "ok";
      // display_message(false, "mqtt ok!");
    }else rta_mqtt = "failed";

    // display_message(false, "mqtt buu!");
  }//fin si wifi está conectado
  else {
    Serial.println("WiFi Disconnected");
    display_message(0, "wifi loss, intentando reconnect");
    WiFi.reconnect();
  }
  imprimir_OLED(rta, rta_mqtt);
}

uint8_t checksum(uint8_t* payload, int payloadLength){
  uint8_t s = 0;
  for( int i = 0; i < payloadLength; i++ )
    s += payload[i];
  
  return s;
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