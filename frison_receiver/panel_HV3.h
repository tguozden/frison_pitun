
void timer_OLED(int segundos){
  fdisplay.setColor(BLACK);
  fdisplay.fillRect(32,94,32,10);
  fdisplay.setColor(WHITE);
  
  
  // fdisplay.setFont(ArialMT_Plain_16);
  
  fdisplay.setTextAlignment(TEXT_ALIGN_RIGHT);
  fdisplay.drawString(64, 94,  String(segundos)+"s");
  // fdisplay.drawString(32, 80,  "dale topu");
  // fdisplay.drawString(64, 70,  "dale topu");
  fdisplay.display();


}


void drawArrow(int centerX, int centerY, int length, float angleRadians) {
    // Calculate arrow head size (proportional to length)
    int headLength = 6;
    float headAngle = PI / 4;  // 30 degrees for arrow head
    
    // Calculate end point of main line
    
    int endX = centerX + length/2 * cos(angleRadians);
    int endY = centerY + length/2 * sin(angleRadians);
    
    int startX = centerX - length/2 * cos(angleRadians);
    int startY = centerY - length/2 * sin(angleRadians);
    // Draw main line
    fdisplay.drawLine(startX, startY, endX, endY);
    
    // Calculate and draw left arrow head
    float leftAngle = angleRadians + PI - headAngle;
    int leftHeadX = endX + headLength * cos(leftAngle);
    int leftHeadY = endY + headLength * sin(leftAngle);
    fdisplay.drawLine(endX, endY, leftHeadX, leftHeadY);
    
    // Calculate and draw right arrow head
    float rightAngle = angleRadians + PI + headAngle;
    int rightHeadX = endX + headLength * cos(rightAngle);
    int rightHeadY = endY + headLength * sin(rightAngle);
    fdisplay.drawLine(endX, endY, rightHeadX, rightHeadY);
}

void imprimir_OLED(String rta, String rta_mqtt){    //const JsonObject& root, 

  const float MPH2KPH = 1.609;
  const float F2CA = 5./9.;
  const float F2CB = 32.;
  
  fdisplay.clear();
  fdisplay.display();
  fdisplay.screenRotate(ANGLE_90_DEGREE);
  fdisplay.setFont(ArialMT_Plain_16);
  
  fdisplay.setTextAlignment(TEXT_ALIGN_RIGHT);
  
  String varray;

  
  varray = String(mqtt_root["w"].as<float>()* MPH2KPH     ,0);
  fdisplay.drawString(24, 0, varray);   
  
  varray = String(mqtt_root["wg"].as<float>()* MPH2KPH     ,0);
  fdisplay.drawString(24, 18, varray);   

  fdisplay.setFont(ArialMT_Plain_10);
  fdisplay.drawString(33, 4,  "k");   
  fdisplay.drawString(33, 11,  "p");   
  fdisplay.drawString(33, 21,  "h");   
  /*flechita !!*/
  drawArrow(51, 17, 24, (90+mqtt_root["wd"].as<int>() )*PI/180.);
  // drawArrow(51, 17, 24, 80 *PI/180.);
  
  /*temperatura ! */
  // fdisplay.setFont(ArialMT_Plain_10);


  
  // fdisplay.drawString(32, 34, "temp");
  // fdisplay.drawString(64, 34, "hum");
  varray = String((mqtt_root["t"].as<float>() - F2CB)*F2CA   ,1);
  fdisplay.drawString(32, 40, varray + "C");
  
  varray = String( mqtt_root["h"].as<int>() );
  fdisplay.drawString(64, 40, varray + "\%");

  /*lluvia dailyrainin*/
  varray = String(mqtt_root["r"].as<float>()/.254, 0);
  fdisplay.drawString(64, 56, varray+" mm");
  

  varray = String(mqtt_root["s"].as<float>(),    0);
  /*luz*/
  fdisplay.drawString(64, 68, varray+"W UV "+String(mqtt_root["U"].as<int>()) );
  /*respuestas*/
  fdisplay.drawString(64, 80, "wg:" + rta + " mqtt:" + rta_mqtt);
  /*voltaje*/
  varray = String(mqtt_root["vi"].as<float>(), 2);
  fdisplay.drawString(32, 94,  varray + "V");
  timer_OLED(0);
  /*señales intensidad*/
  fdisplay.drawString(23, 107, "wifi" );
  fdisplay.drawString(23, 118, String( WiFi.RSSI() ) );
  
  fdisplay.drawString(64, 107,  "L O R A");
  fdisplay.drawString(46, 118,  mqtt_root["rs"]);
  fdisplay.drawString(64, 118,  mqtt_root["sn"]);


  
  fdisplay.display();

}




void display_message(bool clear, String mensaje){

    if ((Col > 50) || clear)
    {
      fdisplay.clear();
      fdisplay.display();
      Col=0;
    }
    fdisplay.drawString(0, Col, mensaje);
    Col+=10;
    fdisplay.display();
}



//#############################################################################################
void WIFISetUp(void)
{
	// Set WiFi to station mode and disconnect from an AP if it was previously connected
	WiFi.disconnect(true);
	delay(100);
	//WiFi.mode(WIFI_STA);
	//WiFi.setAutoConnect(true);
	WiFi.begin(WIFI_SSID, WIFI_PASS);//fill in "Your WiFi SSID","Your Password"
  // WiFi.begin("bc811a", "nachiketas");//fill in "Your WiFi SSID","Your Password"
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
	delay(100);

	byte count = 0;
	while(WiFi.status() != WL_CONNECTED && count < 5)
	{
		count ++;
    display_message(0, "Connecting...");
    delay(1000);
		// fdisplay.drawString(0, 0, "Connecting...");
		// fdisplay.display();
	}

	//fdisplay.clear();
	if(WiFi.status() == WL_CONNECTED)
	{
//    wifiConnected=true;
    display_message(0, "Connecting...OK.");
		// fdisplay.drawString(0, 0, "Connecting...OK.");
		// fdisplay.display();
//		delay(500);
	}
	else
	{
		// fdisplay.clear();
    display_message(0, "Connecting...failed.");
		// fdisplay.drawString(0, 0, "Connecting...Failed");
		// fdisplay.display();
		//while(1);
	}
	// display_message(0, "WIFI Setup done");
	// fdisplay.display();
	delay(500);
}



