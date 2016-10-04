#include <JeeLib.h> // https://github.com/jcw/jeelib
#include <PinChangeInterrupt.h>
#include <avr/sleep.h>
#define myNodeID 30        // RF12 node ID in the range 1-30
#define network 99       // RF12 Network group
#define freq RF12_433MHZ

#define rainPin 10
#define windPin 9

#define VANE_PWR 8
#define VANE_PIN 7

#define WIND_FACTOR 2.4
#define MM_PER_TIP (0.2794 * 100)

int sleeperCount =0;
boolean awake = true;
int rainTips = 0;
int windRevs = 0;

int avgWindSpeed = 0;
int maxWindSpeed = 0;

ISR(WDT_vect) { 
  sleeperCount++;
  Sleepy::watchdogEvent(); 
}
ISR(BADISR_vect)
{
}


typedef struct {
           int rain;        // Rainfall
           int avgWindSpeed; // km/h
           int windGustMax; // km/h 
           byte windDir; // 0-15 clockwise from N
           int supplyV;        // Supply voltage
 } Payload;
Payload tinytx;


void sleepfor(int secondsToSleep)
{
  int countTimeout = secondsToSleep/4;
  sleeperCount = 0;
  awake = false;
  avgWindSpeed = 0;
  maxWindSpeed = 0;
  
  while (sleeperCount < countTimeout)
  {
    rf12_sleep(125); // 4 seconds
    Serial.println("Awake after 4 seconds.");
    Serial.println("WINDS: \t WINDSPEED: ");
    // calculate the windspeed. 
    int winds = windRevs;
    windRevs = 0;
    
    int windSpeed = (winds * WIND_FACTOR)/4;
    Serial.print(winds);Serial.print("\t");Serial.println(windSpeed);
    if (windSpeed > maxWindSpeed)
    {
      maxWindSpeed = windSpeed;
    }
    avgWindSpeed += windSpeed;
    
    // and the rain.
  }
  avgWindSpeed = avgWindSpeed / sleeperCount;
  rf12_sleep(0);
  awake = true;
}


void setup(void){
  Serial.begin(9600);
  Serial.println("INITIALIZING....");
  delay(200);
  rf12_initialize(myNodeID,freq,network);
  rf12_sleep(0);                          // Put the RFM12 to sleep
  setupWeatherInts();
  
}

void loop(void) {
  // put your main code here, to run repeatedly: 
  if(awake)
  {
    Serial.println("Going to sleep....");
    sleepfor(60);
    Serial.println("Awake and ready to Send");
    tinytx.supplyV = readVcc(); // Get supply voltage
    tinytx.avgWindSpeed = (int)(avgWindSpeed*100);
    tinytx.windGustMax = maxWindSpeed*100;
    tinytx.rain = rainTips * MM_PER_TIP;
    getWindVane();
    
    rfwrite();
    
  }
}

static void rfwrite(){
  #if defined(__AVR_ATtiny84__)
     rf12_sleep(-1); // Wake up RF module
     while (!rf12_canSend())
     rf12_recvDone();
     rf12_sendStart(0, &tinytx, sizeof tinytx);
     rf12_sendWait(2); // Wait for RF to finish sending while in standby mode
     rf12_sleep(0); // Put RF module to sleep
     return;
  #else
     Serial.println("DUMMY SEND: ");
  #endif  
}

void setupWeatherInts()
{
  pinMode(rainPin, INPUT); // set reed pin as input
  digitalWrite(rainPin, HIGH); // and turn on pullup
  attachPcInterrupt(rainPin,rainTip,FALLING); // attach a PinChange Interrupt on the falling edge
  
  pinMode(windPin, INPUT); // set reed pin as input
  digitalWrite(windPin, HIGH); // and turn on pullup
  attachPcInterrupt(windPin, windTip,FALLING); // attach a PinChange Interrupt on the falling edge
  
  pinMode(VANE_PWR,OUTPUT);
  digitalWrite(VANE_PWR,LOW);
}

void rainTip(){delay(20);rainTips++;}
void windTip(){delay(20);windRevs++;}

long readVcc() {
   bitClear(PRR, PRADC); ADCSRA |= bit(ADEN); // Enable the ADC
   long result;
   // Read 1.1V reference against Vcc
   #if defined(__AVR_ATtiny84__)
    ADMUX = _BV(MUX5) | _BV(MUX0); // For ATtiny84
   #else
    ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1); // For ATmega328
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
static int vaneValues[] PROGMEM={66,84,92,127,184,244,287,406,461,600,631,702,786,827,889,946};
static int vaneDirections[] PROGMEM={1125,675,900,1575,1350,2025,1800,225,450,2475,2250,3375,0,2925,3150,2700};
 
double getWindVane()
{
  analogReference(DEFAULT);
  digitalWrite(VANE_PWR,HIGH);
  delay(100);
  for(int n=0;n<10;n++)
  {
    analogRead(VANE_PIN);
  }
 
  unsigned int reading=analogRead(VANE_PIN);
  digitalWrite(VANE_PWR,LOW);
  unsigned int lastDiff=2048;
 
  for (int n=0;n<16;n++)
  {
    int diff=reading-pgm_read_word(&vaneValues[n]);
    diff=abs(diff);
    if(diff==0)
       return pgm_read_word(&vaneDirections[n])/10.0;
 
    if(diff>lastDiff)
    {
      return pgm_read_word(&vaneDirections[n-1])/10.0;
    }
 
    lastDiff=diff;
   }
   return pgm_read_word(&vaneDirections[15])/10.0;
 
}
