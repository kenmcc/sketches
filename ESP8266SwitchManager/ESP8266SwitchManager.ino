/*
  Example for different sending methods
  
  https://github.com/sui77/rc-switch/
  
*/

#include <RCSwitch.h>

RCSwitch mySwitch = RCSwitch();

void setup() {

  Serial.begin(9600);
  
  pinMode(0, OUTPUT);
  // Transmitter is connected to Arduino Pin #10  
  mySwitch.enableTransmit(0); // ESP8266 = GPIO0

  // Optional set pulse length.
  // mySwitch.setPulseLength(320);
  
  // Optional set protocol (default is 1, will work for most outlets)
   mySwitch.setProtocol(5);
  
  // Optional set number of transmission repetitions.
   mySwitch.setRepeatTransmit(4);
 
 Serial.begin(9600); 
}

void loop() {

  /* See Example: TypeA_WithDIPSwitches */
//  mySwitch.switchOn("11111", "00010");
//  delay(1000);
//  mySwitch.switchOn("11111", "00010");
//  delay(1000);

  /* Same switch as above, but using decimal code */
Serial.println("Begin");
  mySwitch.send(6408834, 24);

  delay(200);  
  mySwitch.send(6653730, 24);
  delay(200);  
  mySwitch.send(6618194, 24);
  delay(200);  
  mySwitch.send(6314418, 24);
  delay(200);  
    mySwitch.send(6653730, 24);
  delay(200);  
  Serial.println("End");


  /* Same switch as above, but using binary code */
  //mySwitch.send("000000000001010100010001");
  //delay(1000);  
 // mySwitch.send("000000000001010100010100");
 // delay(1000);

  /* Same switch as above, but tri-state code */ 
 // mySwitch.sendTriState("00000FFF0F0F");
 // delay(1000);  
 // mySwitch.sendTriState("00000FFF0FF0");
//  delay(1000);

Serial.println("Sent");
  delay(5000);
}
