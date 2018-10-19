/*
 Name:		OledNtpClock.ino
 Created:	10/20/2018 2:48:52 AM
 Author:	flynn
*/

#include "WiFi.h"
#include "WiFiUdp.h"
#include "TimeLib.h"

#include "ArduinoJson.h"
#include "HTTPClient.h"

#include "Wire.h"
#include "Adafruit_SSD1306.h"
#include "Adafruit_GFX.h"

//WiFi config
char* ssid = "*";
char* password = "*";

const char* ntpServer = "pool.ntp.org";
long  gmtOffset_sec = 0;

WiFiUDP udp;

//Time
time_t Time;
time_t prevDisplay = 0;


//buffer 
// the response for the time is int the first 48 bits
#define NTP_PACKET_SIZE 48
byte packetBuffer[NTP_PACKET_SIZE];

//gmt offset api key
const char* timeZoneApiKey = "http://api.timezonedb.com/v2/get-time-zone?key=2ZVJKMES2VF7&format=json&fields=gmtOffset,dst&by=zone&zone=Australia/Melbourne";

bool isConnected = false;

//how many times sync fails in a row
int syncFailCount = 0;

// OLED i2c SSD1306 setup
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 oled(-1); // -1 for no reset pin

						   // 128 x 64 pixel display
#if (SSD1306_LCDHEIGHT != 64)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

						   //sends request to NTP server
unsigned long sendNTPpacket(const char* t_serverName)
{
	//turns server name into an ip 
	IPAddress NTPaddress;
	WiFi.hostByName(t_serverName, NTPaddress);

	//clear the buffer
	memset(packetBuffer, 0, NTP_PACKET_SIZE);

	//settings 
	packetBuffer[0] = 0b11100011;   // LI 11, Version 100, Mode 011 
	packetBuffer[1] = 0;     // Stratum, or type of clock
	packetBuffer[2] = 6;     // Polling Interval
	packetBuffer[3] = 0xEC;  // Peer Clock Precision
							 // 8 bytes of zero for Root Delay & Root Dispersion
	packetBuffer[12] = 49;
	packetBuffer[13] = 0x4E;
	packetBuffer[14] = 49;
	packetBuffer[15] = 52;

	//sends packet
	udp.beginPacket(NTPaddress, 123);
	udp.write(packetBuffer, NTP_PACKET_SIZE);
	udp.endPacket();
}

//returns unix time
time_t getNTPTime()
{
	// may need to discard/ clear buffer ??

	sendNTPpacket(ntpServer);

	uint32_t beginWait = millis();
	while (millis() - beginWait < 900)
	{
		if (udp.parsePacket() >= NTP_PACKET_SIZE)
		{
			//move incoming packet into the buffer
			udp.read(packetBuffer, NTP_PACKET_SIZE);

			//combine 4 bytes where the time is into one 32bit no#
			uint32_t NTPtime = ((packetBuffer[40] << 24) | (packetBuffer[41] << 16) | (packetBuffer[42] << 8) | packetBuffer[43]);
			Serial.print(": time sync");

			//reset fail counter
			syncFailCount = 0;

			//retune UNIX time (1970) with gmt offset
			return (NTPtime - 2208988800UL + gmtOffset_sec);
		}
	}
	//add a sync fail
	++syncFailCount;

	//returne the rtc time if it cant see the ntp reply
	Serial.print(": no NTP packet recived ");
	return 0;
}

//display time on serial
void digitalClockDisplay() {
	// digital clock display of the time
	//Serial.printf("\n %d:%02d:%02d", hour(), minute(), second());
	oled.clearDisplay();
	oled.setTextSize(2);
	oled.setTextColor(WHITE);
	oled.setCursor(20, 10);
	oled.printf("\n %d:%02d:%02d", hour(), minute(), second());
	oled.display();

}

//get the gmt offset
long getGMToffset(const char* t_apiKey)
{
	HTTPClient httpClient;
	httpClient.begin(t_apiKey);

	//check if there actully is any response
	if (httpClient.GET() > 0)
	{
		//creat buffer
		StaticJsonBuffer<200> jsonBuffer;

		//buffer will go out of scope and memory will be freed
		JsonObject& root = jsonBuffer.parseObject(httpClient.getString());

		//check if the response is ok
		if (root["status"] == "OK")
		{
			int gmtOffest = root["gmtOffset"];
			return gmtOffest;
		}
	}
	else
	{
		//return 0 if it was not able to get a response.
		return 0;
	}
}

//setup functions
void connectToWiFi()
{
	WiFi.disconnect();

	//set up WiFi
	WiFi.begin(ssid, password);
	Serial.printf("Connecting to: %s \n", ssid);
	while (WiFi.status() != WL_CONNECTED)
	{
		delay(250);
		Serial.print('.');
	}
	Serial.print("\n Connected");
}

void startUDP()
{
	udp.begin(123);
}

void setup()
{
	//set up display
	oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);
	oled.clearDisplay();
	oled.display();

	// display a line of text
	oled.setTextSize(1);
	oled.setTextColor(WHITE);
	oled.setCursor(27, 30);
	oled.print("Hello, world!");

	// update display with all of the above graphics
	oled.display();



	Serial.begin(115200);
	connectToWiFi();

	//set gmt offset
	gmtOffset_sec = getGMToffset(timeZoneApiKey);
	while (gmtOffset_sec == 0)
	{
		gmtOffset_sec = getGMToffset(timeZoneApiKey);
	}

	//set up the time sync
	setSyncProvider(getNTPTime);
	setSyncInterval(20);

}

void loop()
{
	if (now() != prevDisplay) { //update the display only if time has changed
		prevDisplay = now();
		digitalClockDisplay();

	}

	//look into fixing this with pointers so less variables are used.
	if (minute() == 0 && second() == 1)
	{
		long gmtbuff = getGMToffset(timeZoneApiKey);
		if (!gmtbuff)
			Serial.print("failed to get gmt offset");
		else
			gmtOffset_sec = gmtbuff;
	}

	//possable loss of wifi
	if (syncFailCount > 3)
	{
		//if (WiFi.status() == WL)
	}
}
