/// @dir groupRelay
/// Relay packets from one net group to another to extend the range.
// 2011-01-11 <jc@wippler.nl> http://opensource.org/licenses/mit-license.php

#include <JeeLib.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>

#define IN_GROUP  100
#define IN_FREQ   RF12_433MHZ
#define IN_NODE   31

#define OUT_GROUP  99
#define OUT_FREQ   RF12_433MHZ
#define OUT_NODE   31

byte buf[68];
MilliTimer timer;

typedef struct {
           int supplyV;        // Supply voltage
} Payload;

Payload tinytx;

 long readVcc() {
   bitClear(PRR, PRADC); ADCSRA |= bit(ADEN); // Enable the ADC
   long result;
   // Read 1.1V reference against Vcc
   #if defined(__AVR_ATtiny84__) 
    ADMUX = _BV(MUX5) | _BV(MUX0); // For ATtiny84
   #else
    ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);  // For ATmega328
   #endif 
   delay(2); // Wait for Vref to settle
   ADCSRA |= _BV(ADSC); // Convert
   while (bit_is_set(ADCSRA,ADSC));
   result = ADCL;
   result |= ADCH<<8;
   result = 1126400L / result; // Back-calculate Vcc in mV
   ADCSRA &= ~ bit(ADEN); bitSet(PRR, PRADC); // Disable the ADC to save power
   return result;
} 


void setup () {
    Serial.begin(9600);
    delay(2000);
    Serial.print("\n[groupRelay]");
    rf12_initialize(IN_NODE, IN_FREQ, IN_GROUP);
    Serial.print("\nInitialized\n");
}

void loop () {
    if (!rf12_recvDone() || rf12_crc != 0)
        return; // nothing to do
    
    // make copies, because rf12_* will change in next rf12_recvDone
    byte hdr = rf12_hdr, len = rf12_len;
    memcpy(buf, (void*) rf12_data, len);
    //buf[0] = OUT_GROUP;
    //buf[1] = rf12_buf[1];
    
    Serial.print("Have data ");
    Serial.print(len);
    Serial.print(" bytes ");
    for (int x = 0; x < len; x++)
    {
      Serial.print(int(rf12_buf[x])); Serial.print(" ");
    }
    Serial.print("Going to send on \n");
    // switch to outgoing group
    rf12_initialize(2, OUT_FREQ, OUT_GROUP);
    
    // send our packet, once possible
     while (!rf12_canSend())
     rf12_recvDone();
     rf12_sendStart(0, buf, len); 
     rf12_sendWait(2);           // Wait for RF to finish sending while in standby mode
     
     tinytx.supplyV = readVcc(); // Get supply voltage
     rf12_initialize(9, OUT_FREQ, OUT_GROUP);

     while (!rf12_canSend())
     rf12_recvDone();
     rf12_sendStart(0, &tinytx, sizeof tinytx); 
     rf12_sendWait(2);           // Wait for RF to finish sending while in standby mode

  
    // switch back to incoming group      
    rf12_initialize(IN_NODE, IN_FREQ, IN_GROUP);
    Serial.println("Back waiting for incoming\n");
}
