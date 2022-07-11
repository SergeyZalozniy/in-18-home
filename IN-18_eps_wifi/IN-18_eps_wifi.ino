#include <Timezone.h>
#include <TimeLib.h>
#include <Time.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <WiFiUdp.h>
#include <FastLED.h>

#define kColorCount 24
CRGBArray<kColorCount> colors;

char ssid[] = "Zamioculcas";  //  your network SSID (name)
char pass[] = "cherkassy";       // your network password


unsigned int localPort = 2390;      // local port to listen for UDP packets

IPAddress timeServerIP; // time.nist.gov NTP server address
const char* ntpServerName = "time.nist.gov";
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

time_t lastRequestTime = 0;
time_t lastPrintTime = 0;

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;

void setup() {
  for (int i = 0; i < 5; i++) {
    colors[i] = CRGB(0, 0, 35);
  }

  for (int i = 5; i < 9; i++) {
    colors[i] = CRGB::Plaid;
  }

  for (int i = 9; i < 18; i++) {
    colors[i] = CRGB::LimeGreen;
  }

  for (int i = 18; i < 21; i++) {
    colors[i] = CRGB::MidnightBlue;
  }

  for (int i = 21; i < 24; i++) {
    colors[i] = CRGB(0, 0, 35);
  }
  
  Serial.begin(9600);

  // We start by connecting to a WiFi network
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  int count = 0;
  while (WiFi.status() != WL_CONNECTED) {
    count++;
    delay(150);
    Serial.print(".");
  }
  Serial.println("");

  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.println("Starting UDP");
  udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(udp.localPort());

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
}

void loop() {
  ArduinoOTA.handle();
  
  if( !WiFi.isConnected() ) {
    Serial.println( "Disconnected!" );
    WiFi.reconnect();
    WiFi.waitForConnectResult();
  } else {
    //get a random server from the pool
    WiFi.hostByName(ntpServerName, timeServerIP);
  }

  if (abs(millis() - lastRequestTime) >= (60) * 1000 || lastRequestTime == 0) {
    sendNTPpacket(timeServerIP); // send an NTP packet to a time server
    lastRequestTime = millis();
  }

  int cb = udp.parsePacket();
  if (cb) {
    Serial.print("packet received, length=");
    Serial.println(cb);
    // We've received a packet, read the data from it
    udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

//the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    Serial.print("Seconds since Jan 1 1900 = ");
    Serial.println(secsSince1900);

    // now convert NTP time into everyday time:
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;
    // subtract seventy years:
    unsigned long epoch = secsSince1900 - seventyYears;
    // print Unix time:
    setTime(epoch);
    Serial.println("$GPRMC," + PreZero(hour()) + PreZero(minute()) + PreZero(second()) + ",A,5553.6670,N,03437.9628,E,46.10,220.66," + PreZero(day()) + PreZero(month()) + PreZero(year()) + ",,*WEB");
  }  
}

time_t localTime() {
  TimeChangeRule usEDT = {"EDT", Last, Sun, Mar, 2, 180};  //UTC - 3 hours
  TimeChangeRule usEST = {"EST", First, Sun, Nov, 2, 120};   //UTC - 2 hours
  Timezone usEastern(usEDT, usEST);
  time_t utc = now();  //current time from the Time Library
  return usEastern.toLocal(utc);
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress& address) {
  Serial.println("sending NTP packet...");

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
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

String PreZero(int digit)
{
  switch (digit) {
    case 0:
      return "00";
    case 1:
      return "01";
    case 2:
      return "02";
    case 3:
      return "03";
    case 4:
      return "04";
    case 5:
      return "05";
    case 6:
      return "06";
    case 7:
      return "07";
    case 8:
      return "08";
    case 9:
      return "09";
    default:
      return String(digit);
  }
}
