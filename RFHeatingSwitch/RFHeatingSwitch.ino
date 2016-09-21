
/*
 */

// include the library code:
//#include <Servo.h> 
#include <SoftwareServo.h> 
#include <RCSwitch.h>
#include <avr/sleep.h>
#include <PinChangeInterrupt.h>

RCSwitch mySwitch = RCSwitch();
SoftwareServo myservo;  // create servo object to control a servo 

int pos = 0;    // variable to store the servo position 

#if not defined(__AVR_ATtiny85__)
int photoRPin = 0; 
int SERVOPIN = 9;
int RFINTERRUPT =0;
#else
int photoRPin = 0; 
int SERVOPIN = 1;
int RFINTERRUPT =0;

#endif

void wakeUp()
{
  #if not defined(__AVR_ATtiny85__)
  Serial.println("Awake");
  #endif
}
  

void setup() {
  mySwitch.enableReceive(RFINTERRUPT);  // Receiver on inerrupt 0 => that is pin #2
  #if not defined(__AVR_ATtiny85__)
  Serial.begin(9600);
  Serial.println("Ready");
  #endif
}

unsigned long lastReadTime = 0;

void sweep()
{
  #if not defined(__AVR_ATtiny85__)
Serial.println("Sweeping");
  #endif
  
  myservo.attach(SERVOPIN);  // attaches the servo on pin 9 to the servo object
  delay(250);
  myservo.write(180);
  for(int i = 0; i < 50; i++)
  {
    SoftwareServo::refresh();
    delay(25);
  }
  delay(500);
  myservo.write(0);
  for(int i = 0; i < 50; i++)
  {
    SoftwareServo::refresh();
    delay(25);
  }
  delay(500);
  myservo.detach();

 #if not defined(__AVR_ATtiny85__)
 Serial.println("Done");
 #endif

}

void loop() {
 
  if (mySwitch.available()) {
    
    int value = mySwitch.getReceivedValue();
    unsigned long now = millis();
      if (now - lastReadTime > 300)
      {
        long int val = mySwitch.getReceivedValue();
        int lightLevel;
        if (val == 7976963)
        {lightLevel=analogRead(photoRPin);
#if not defined(__AVR_ATtiny85__)
        Serial.println(lightLevel);
#endif
        if(lightLevel <= 150)
          {
#if not defined(__AVR_ATtiny85__)
              Serial.println("Turn On");
  #endif         
              sweep();
          }
#if not defined(__AVR_ATtiny85__)
          else
          {
            Serial.println("Already on");
          }
#endif
        }
        else if (val == 7976962)
        {
          lightLevel=analogRead(photoRPin);
#if not defined(__AVR_ATtiny85__)
          Serial.println(lightLevel);
#endif        
          if(lightLevel >= 150)
          {
#if not defined(__AVR_ATtiny85__)
              Serial.println("Turn Off");
#endif              
              sweep();
          }
 #if not defined(__AVR_ATtiny85__)
          else
          {
           Serial.println("Already off");
          }
  #endif         
        }
#if not defined(__AVR_ATtiny85__)
        else
        {
        Serial.print("Received ");
        Serial.println( val );
        }
 #endif
        
        lastReadTime = now;
      }

    }

    mySwitch.resetAvailable();
    delay(500);

}

