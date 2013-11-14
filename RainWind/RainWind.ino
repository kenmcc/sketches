//----------------------------------------------------------------------------------------------------------------------
// TinyTX_Rain_Gauge - An ATtiny84 Rainfall Monitor
// By Nathan Chantrell. For hardware design see http://nathan.chantrell.net/tinytx
//
// Using a tipping bucket rain gauge with a reed switch across D10 and GND
//
// Licenced under the Creative Commons Attribution-ShareAlike 3.0 Unported (CC BY-SA 3.0) licence:
// http://creativecommons.org/licenses/by-sa/3.0/
//
// Requires Arduino IDE with arduino-tiny core: http://code.google.com/p/arduino-tiny/
//----------------------------------------------------------------------------------------------------------------------

#include <JeeLib.h> // https://github.com/jcw/jeelib
#include <PinChangeInterrupt.h> // http://code.google.com/p/arduino-tiny/downloads/list
#include <avr/sleep.h>

#define myNodeID 3        // RF12 node ID in the range 1-30
#define network 99       // RF12 Network group
#define freq RF12_433MHZ  // Frequency of RFM12B module

#define USE_ACK           // Enable ACKs, comment out to disable
#define RETRY_PERIOD 5    // How soon to retry (in seconds) if ACK didn't come in
#define RETRY_LIMIT 5     // Maximum number of times to retry
#define ACK_TIME 10       // Number of milliseconds to wait for an ack

#define BUCKETMM 0.3      // How many mm of rain for each bucket tip
#define rainPin 10        // Reed switch across D10 (ATtiny pin 13) and GND
#define windSpeedPin 9    



//--------------------------------------------------------------------------------------------------
//Data Structure to be sent
//--------------------------------------------------------------------------------------------------

typedef struct {
           int rain;        // Rainfall
           int maxRainfallRate; // mm/hour
           int avgWindSpeed; // km/h
           int windGustMax; // km/h 
           byte windDir; // 0-15 clockwise from N
           int supplyV;        // Supply voltage
 } Payload;

Payload tinytx;

typedef struct RainData{
   double rainMM;
   int maxRainRate;
   int minRainRate;
}RainData;

typedef struct WindData{
   double avgSpeed;
}WindData;


ISR(BADISR_vect)
{
} 
//--------------------------------------------------------------------------------------------------
// Wait for an ACK
//--------------------------------------------------------------------------------------------------
 #ifdef USE_ACK
  static byte waitForAck() {
   MilliTimer ackTimer;
   while (!ackTimer.poll(ACK_TIME)) {
     if (rf12_recvDone() && rf12_crc == 0 &&
        rf12_hdr == (RF12_HDR_DST | RF12_HDR_CTL | myNodeID))
        return 1;
     }
   return 0;
  }
 #endif

//--------------------------------------------------------------------------------------------------
// Send payload data via RF
//-------------------------------------------------------------------------------------------------
 static void rfwrite(){
  #ifdef USE_ACK
   for (byte i = 0; i <= RETRY_LIMIT; ++i) {  // tx and wait for ack up to RETRY_LIMIT times
     rf12_sleep(-1);              // Wake up RF module
      while (!rf12_canSend())
      rf12_recvDone();
      rf12_sendStart(RF12_HDR_ACK, &tinytx, sizeof tinytx); 
      rf12_sendWait(2);           // Wait for RF to finish sending while in standby mode
      byte acked = waitForAck();  // Wait for ACK
      rf12_sleep(0);              // Put RF module to sleep
      if (acked) { return; }      // Return if ACK received
      
   Sleepy::loseSomeTime(RETRY_PERIOD * 1000);     // If no ack received wait and try again
   }
  #else
     rf12_sleep(-1);              // Wake up RF module
     while (!rf12_canSend())
     rf12_recvDone();
     rf12_sendStart(0, &tinytx, sizeof tinytx); 
     rf12_sendWait(2);            // Wait for RF to finish sending while in standby mode
     rf12_sleep(0);               // Put RF module to sleep
     return;
  #endif
 }

//--------------------------------------------------------------------------------------------------
// Read current supply voltage
//--------------------------------------------------------------------------------------------------
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
    
//########################################################################################################################

void setup() {
  
  Serial.begin(9600);
  Serial.println("Alive");
  PRR = bit(PRTIM1); // only keep timer 0 going
  
  ADCSRA &= ~ bit(ADEN); bitSet(PRR, PRADC); // Disable the ADC to save power

  rf12_initialize(myNodeID,freq,network); // Initialize RFM12 with settings defined above 
  rf12_sleep(0);                          // Put the RFM12 to sleep

  pinMode(rainPin, INPUT);     // set reed pin as input
  digitalWrite(rainPin, HIGH); // and turn on pullup
  attachPcInterrupt(rainPin,wakeUpRain,FALLING); // attach a PinChange Interrupt on the falling edge
  
  pinMode(windSpeedPin, INPUT);     // set reed pin as input
  digitalWrite(windSpeedPin, HIGH); // and turn on pullup
  attachPcInterrupt(windSpeedPin,wakeUpWind,FALLING); // attach a PinChange Interrupt on the falling edge
  
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);         // Set sleep mode
  sleep_mode();                                // Sleep now
  
}

#define MM_PER_TIP (0.2794 * 100) // take account of th
 
volatile unsigned long rain_count=0;
volatile unsigned long rain_last=0;
volatile unsigned long shortestRainInterval = 0xffffffff;
volatile unsigned long longestRainInterval = 0;


/* get the rain tips since last */ 
struct RainData getUnitRain(unsigned long timeGap)
{
  struct RainData data;
  unsigned long rainBucketTips=rain_count;
  data.rainMM =rainBucketTips * MM_PER_TIP;
  return data;
}

volatile unsigned long lastValidRainTime =0;
void wakeUpRain()
{
  Serial.println("RAIN!");
    long now = micros();
    long thisTime= now - rain_last;
    rain_last = now;
    if(thisTime>500) // only if it's a valid time
    {
      rain_count++;
      unsigned long interval = now - lastValidRainTime;
      if (interval > longestRainInterval)
      {
         longestRainInterval = interval;
      }
      else if (interval < shortestRainInterval)
      {
        shortestRainInterval = interval;
      }
    }
}


///////////////////////////////
////
////     WIND SPEED STUFF 
////
//////////////////////////////

#define WIND_FACTOR 2.4 // km/h per rotation per sec
#define TEST_PAUSE 60000
 
volatile unsigned long anemRotations=0;
volatile unsigned long anem_last=0;
volatile unsigned long anem_min=0xffffffff;
volatile unsigned long anem_max=0;
 
struct WindData getAvgWindspeed(unsigned long period)
{
  struct WindData data;
  // avg windspeed is (rotations x distance per rotation)/time in hours
  // km/h = rev/min * 2.4
  unsigned long reading=anemRotations;
  anemRotations=0;

  Serial.println("reading, period, revpermin");
  Serial.println(reading);
  Serial.println(period);
  
  // windspeed = num revs / revs/min * 2.4
  double revPerMin = reading / (period/1000/60);
  Serial.println(revPerMin);
  
  data.avgSpeed = (reading/ revPerMin) * WIND_FACTOR;
  return data;
}
 
double getGust()
{
  unsigned long reading=anem_min;
  anem_min=0xffffffff;
  double time=reading/1000000.0;
  return (1/(reading/1000000.0))*WIND_FACTOR;
}


void wakeUpWind(){
  Serial.println("Awake Wind");
    unsigned long now = micros(); // timestamp now
  long thisTime= now - anem_last; /* time since last count */
  anem_last = now; 
  if(thisTime > 500)
  {
    anemRotations++;
    // if this interval is shorter than previous, store that.
    if(thisTime<anem_min)
    {
      anem_min=thisTime;
    }
    if(thisTime > anem_max) // and record the minumum gust 
    {
      anem_max = thisTime;
    }
  }
}

volatile unsigned long lastReadTime = 0;

void loop() {

  Serial.println("loop");  
  unsigned long now = millis();
  unsigned long timeDiff = (now > lastReadTime)?now - lastReadTime:lastReadTime - now;
  unsigned long timeToSleep = 6000;
  //  only send back data every minute 
  if(timeDiff >= timeToSleep)
  {
   // first of all we get the units rain. in mm.   
    RainData rd = getUnitRain(timeDiff); // Send mm per tip as an integer (multiply by 0.01 at receiving end)
    tinytx.rain = rd.rainMM; // actually it's 100x mm
    
    WindData wd = getAvgWindspeed(timeDiff);  
    tinytx.avgWindSpeed = (int)(wd.avgSpeed*100);
    
    tinytx.windDir = 0;
    tinytx.windGustMax = 0;
      
    tinytx.supplyV = readVcc(); // Get supply voltage
  
   // rfwrite(); // Send data via RF
   Serial.print("MaxWindSpeed : ");
   Serial.println(tinytx.windGustMax);
   Serial.print("AvgWindSpeed : ");
   Serial.println(tinytx.avgWindSpeed);
   Serial.print("Rain : ");
   Serial.print(tinytx.rain);
    
    lastReadTime = now;
  }
  else
  {
    timeToSleep -= timeDiff;
  }
  Sleepy::loseSomeTime(timeToSleep);
  
  set_sleep_mode(SLEEP_MODE_PWR_DOWN); // Set sleep mode
  sleep_mode(); // Sleep now
  
}




