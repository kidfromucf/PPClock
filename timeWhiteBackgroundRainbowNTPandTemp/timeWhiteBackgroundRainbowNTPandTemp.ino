#define FASTLED_ALLOW_INTERRUPTS 0
#include <FastLED.h>
#include <TimeLib.h> 
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#include <WiFiClientSecure.h>
#include <JSON_Decoder.h>
#include <DarkSkyWeather.h>

#define LED_PIN     2
#define NUM_LEDS    128
CRGB leds[NUM_LEDS];
CHSV colour( 0, 255, 128);
int counter=0;
uint8_t gHue = 0;

const char ssid[] = "Interface_WAP";
const char pass[] = "Meeker2018";

IPAddress timeServer; // time.nist.gov NTP server address
const char* ntpServerName = "pool.ntp.org";

const int timeZone = -7;     // Mountain Time Time

const int Digits[10][10] =
{
  {5,6,7,11,15,18,19,21,22,27}, //0-
  {18,19,20,21,22},             //1-
  {5,11,13,15,18,20,21,22},     //2-
  {11,13,15,18,19,20,21,22},    //3-
  {7,11,13,18,19,20,21,6,27},   //4-
  {7,11,13,15,18,19,20,22},     //5-
  {5,12,13,15,18,19,20,22},     //6-
  {11,14,15,20,21,22},          //7-
  {5,7,11,13,15,18,19,20,21,22},//8-
  {7,11,13,14,15,20,21,22},     //9-
};

// Dark Sky API Details, replace x's with your API key
String api_key = "398c5f529bb4b11e4a66982f1999287e"; // Obtain this from your Dark Sky account

// Set both longitude and latitude to at least 4 decimal places
String latitude =  "40.065482";
String longitude = "-105.187523";

String units = "us";  // See notes tab
String language = "en"; // See notes tab

WiFiUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets
unsigned long previousMillis = 0;
const long interval = 600000;

int currentTemp;

DS_Weather dsw; // Weather forecast library instance

void setup()
{
  FastLED.addLeds<WS2811, LED_PIN, GRB>(leds, NUM_LEDS);
  Serial.begin(9600);
  
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    yield();
  }

  WiFi.hostByName(ntpServerName, timeServer);
  Serial.print("IP number assigned by DHCP is ");
  Serial.println(WiFi.localIP());
  Serial.println("Starting UDP");
  Udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(Udp.localPort());
  Serial.println("waiting for sync");
  Serial.print("NTP Server: ");
  Serial.print(ntpServerName);
  Serial.print(" | NTP Server IP: ");
  Serial.println(timeServer);
  setSyncProvider(getNtpTime);
  setSyncInterval(60);

  checkCurrentWeather();
}

time_t prevDisplay = 0; // when the digital clock was displayed

void checkCurrentWeather()
{
  // Create the structures that hold the retrieved weather
  DSW_current *current = new DSW_current;
  DSW_minutely *minutely = new DSW_minutely;
  DSW_hourly *hourly = new DSW_hourly;
  DSW_daily  *daily = new DSW_daily;

  Serial.print("\nRequesting weather information from DarkSky.net... ");

  dsw.getForecast(current, minutely, hourly, daily, api_key, latitude, longitude, units, language);

  currentTemp = current->temperature;
  
  Serial.println("Weather from Dark Sky\n");
  Serial.println("############### Current weather ###############\n");
  Serial.print("Current temperature      : "); Serial.println(current->temperature);
  Serial.println();
  // Delete to free up space and prevent fragmentation as strings change in length
  delete current;
}

void displaynumber( int place , int number){
  for (int i = 0 ; i < 10 ; i++) {
    if (Digits[number/10][i] != 0) {
      leds[(Digits[number/10][i]+place)] = CRGB(255,255,255);;
    }
    if (Digits[number%10][i] != 0) {
      leds[(Digits[number%10][i]+28+place)] = CRGB(255,255,255);;
    }
  }
}

void loop(){
  unsigned long currentMillis = millis();

  EVERY_N_MILLISECONDS( 20 ) { gHue++; } // slowly cycle the "base color" through the rainbow
    
  //changes the colour of background every 10 cycles
  if (counter<10){
    counter++;
  }else{
    colour.hue = (colour.hue+1)%256;
    counter=0;
  }
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    checkCurrentWeather();
  }
  
  if ( minute()%10 == 0){
    // random colored speckles that blink in and fade smoothly
    fadeToBlackBy( leds, NUM_LEDS, 10);
    int pos = random16(NUM_LEDS);
    leds[pos] += CHSV( gHue + random8(64), 200, 255);
  }
  else if (second()<15){
    // sets background to rainbow colours
    for ( int i=0; i< 128;i++){
      colour.hue = (colour.hue+1)%256;
      leds[i]= colour;
    }
    colour.hue = (colour.hue+128)%256;
    displaynumber(42,currentTemp);
    leds[105] = CRGB(255,255,255);
    leds[106] = CRGB(255,255,255);
    leds[109] = CRGB(255,255,255);
  }
  else{
    // sets background to rainbow colours
    for ( int i=0; i< 128;i++){
      colour.hue = (colour.hue+1)%256;
      leds[i]= colour;
    }
    colour.hue = (colour.hue+128)%256;
  
    displaynumber(0,hour());
    displaynumber(70,minute());

    //display colons
    if ( second()%2 == 0 ){
      leds[63] = CRGB(255,255,255);
      leds[61] = CRGB(255,255,255);
    }
  }


  FastLED.show();

  if (timeStatus() != timeNotSet) {
    if (now() != prevDisplay) { //update the display only if time has changed
      prevDisplay = now();
      digitalClockDisplay();  
    }
  }
}

/*-------- NTP code ----------*/
const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

void digitalClockDisplay(){
  // digital clock display of the time
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(".");
  Serial.print(month());
  Serial.print(".");
  Serial.print(year()); 
  Serial.println(); 
}

void printDigits(int digits){
  // utility for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if(digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

time_t getNtpTime()
{
  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  sendNTPpacket(timeServer);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 5000) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
      yield();
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:                 
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
