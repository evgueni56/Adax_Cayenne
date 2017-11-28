extern "C" {
#include "user_interface.h"
}
#include <SPI.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Time.h>
#include <stdlib.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <EEPROM.h>
#include <ESP8266WebServer.h>
#include <BlynkSimpleEsp8266.h>
#include <SimpleTimer.h>
#include <WidgetRTC.h>
//#include <OneButton.h>


// Data wire is plugged into port 2
#define ONE_WIRE_BUS 14

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

// arrays to hold device address
DeviceAddress insideThermometer;

// Button object on pin 0, low
//OneButton button(0, LOW);

float tempC, oldT =15.0;
char ssid[] = "Adax-Rasho"; // Name of the access point
char auth[] = "39e78a2de5b64d8dbb362ecdfa6703e5"; // Authentication key to Blynk
String t_ssdi, t_pw, st, content;

char epromdata[256];

const int Rellay = 12;
const int Led = 13;
int pinValue = 1;
int BlynkSTimeout = 0;

//Timer instantiate
BlynkTimer SleepTimer;
WidgetRTC rtc;
long OnTime = -1, OffTime= -1, this_second; // Times for the schedule in soconds of the day
int blink_timer; // IDs of the Simple timers
bool relay_status = false, OnSwitch = false;
float req_temp;
int blynk_relay_status = 0;

int i, j, numnets, buf_pointer, wifi_cause;
ESP8266WebServer server(80);
String qsid, qpass; //Holds the new credentials from AP

void setup()
{
	//Serial.begin(74880);

//	WiFi.softAPdisconnect(); // Cleanup - might be up from previuos session
	EEPROM.begin(512);
	EEPROM.get(0, epromdata);
	numnets = epromdata[0];
	Wire.begin();
	SetupTemeratureSensor();
	pinMode(Rellay, OUTPUT);
	pinMode(Led, OUTPUT);
	digitalWrite(Led, 1);
	digitalWrite(Rellay, 0);
	delay(500);
	// Serial.println("Starting...");

	int n = WiFi.scanNetworks(); //  Check if any WiFi in grange
	if (!n)
	{
		// No access points in range - just be a thermometer
		wifi_cause = 5;
		return;
	}
	// Check for known access points to connect
	wifi_cause = ConnectWiFi();
	switch (wifi_cause)
	{
	case 0: //Everything with normal WiFi connection goes here
	{
		Blynk.config(auth/*, IPAddress(84,40,82,37)*/);
//		Blynk.run();
		setSyncInterval(60 * 60);
	}
	break;
	case 1: //A known network does not connect
	{
		setupAP();
	}
	break;
	case 2: //No known networks
	{
		setupAP();
	}
	break;
	}
//	button.attachDoubleClick(TogleState);
	blink_timer = SleepTimer.setInterval(15 * 1000, SleepTFunc);

}

void loop()
{
	if (wifi_cause)
	{
		server.handleClient();
		SleepTimer.run();
	}
	else
	{
		yield();
		SleepTimer.run();
		Blynk.run();

		ChangeValues();

	if (relay_status && OnSwitch && tempC < req_temp)
		{
				digitalWrite(Rellay, 1);
				digitalWrite(Led, 0);
				blynk_relay_status = 255;
		}
		else if(tempC > req_temp + 1) // 1 degree hysteresis
		{
			digitalWrite(Rellay, 0);
			digitalWrite(Led, 1);
			blynk_relay_status = 0;
		}
		
	}
}

void SetupTemeratureSensor()
{
	sensors.begin();
	sensors.getDeviceCount();
	sensors.getAddress(insideThermometer, 0);
	sensors.setResolution(insideThermometer, 12);
}

BLYNK_WRITE(V11) // Temperature
{
	req_temp = param.asFloat(); 
}

BLYNK_WRITE(V12) // Time schedule
{
		OnTime = param[0].asLong();
		OffTime = param[1].asLong();
}

BLYNK_WRITE(V13)
{
		OnSwitch = param.asInt();
}


BLYNK_CONNECTED()
{
	rtc.begin();
	Blynk.syncAll();
}

void SleepTFunc()
{
	if (!Blynk.connected()) // Not yet connected to server
		return;
	// Now push the values
	//	Blynk.virtualWrite(V3, rtcData.TimeSpent);
		sensors.requestTemperatures();
		tempC = sensors.getTempCByIndex(0);
		if (tempC == 85.0 || tempC == -127.0)
			tempC = oldT;
		else
			oldT = tempC;

	tempC = floor(tempC * 10 + 0.5) / 10 - 2;
	Blynk.virtualWrite(V0, tempC); // Current temperature
	Blynk.virtualWrite(V1, blynk_relay_status); // ON/OFF Status
	
	return;
}

int ConnectWiFi()
{
	int n = WiFi.scanNetworks();
//	Serial.println("Nets " + n);
	buf_pointer = 1;
	if (numnets == 0) return 2;
	for (i = 0; i < numnets; i++)
	{
		t_ssdi = String(epromdata + buf_pointer);
		buf_pointer += t_ssdi.length() + 1;
		t_pw = String(epromdata + buf_pointer);
		buf_pointer += t_pw.length() + 1;
		for (j = 0; j < n; j++)
		{
			if (t_ssdi == String(WiFi.SSID(j)))
			{
				int c = 0;
				WiFi.begin(t_ssdi.c_str(), t_pw.c_str());
				while (c < 20)
				{
					if (WiFi.status() == WL_CONNECTED)
					{
						delay(1000);
						return 0;
					}
					delay(500);
					c++;
				}
				return 1;
			}

		}

	}
	return 2;
}

void setupAP(void)
{
	WiFi.mode(WIFI_STA);
	WiFi.disconnect();
	delay(100);
	int n = WiFi.scanNetworks();
	st = "<ol>";
	for (int i = 0; i < n; ++i)
	{
		// Print SSID and RSSI for each network found
		st += "<li>";
		st += WiFi.SSID(i);
		st += "</li>";
	}
	st += "</ol>";
	delay(100);
	WiFi.softAP(ssid);
	launchWeb();
}

void launchWeb(void)
{

	server.on("/", []() {
		IPAddress ip = WiFi.softAPIP();
		String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
		content = "<!DOCTYPE HTML>\r\n";
		content += "<head>\r\n";
		content += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\" />\r\n";
		content += "<title>Точка за достъп</title>";
		content += "<head>\r\n";
		content += ipStr;
		content += "<p>";
		content += st;
		content += "</p><form method='get' action='setting'><label>SSID: </label><input name='ssid' length=32><input name='pass' length=64><input type='submit'></form>";
		content += "</html>";
		server.send(200, "text/html; charset=utf-8", content);
	});
	server.on("/setting", []() {
		qsid = server.arg("ssid");
		qpass = server.arg("pass");
		if (qsid.length() > 0 && qpass.length() > 0)
		{
			// Should write qsid & qpass to EEPROM
			if (wifi_cause == 1) remove_ssdi();
			if (!append_ssdi())
			{
				content = "No more room for access points";
			}
			content = "<!DOCTYPE HTML>\r\n<html>";
			content += "<p>saved to eeprom... reset to boot into new wifi</p></html>";
		}
		else {
			content = "Въведете правилни креденции на предишната страница\n\r";
			content += "Или изберете без връзка\n\r";
			content += "</p><form method='get' action='setting'><input name='confirm' length=0><input type='submit'></form>";
		}
		server.send(200, "text/html; charset=utf-8", content);
	});

	server.on("/setting", []() {
		qsid = server.arg("confirm");
		wifi_cause = 0;
	});
	server.begin();
	SleepTimer.setTimeout(5 * 60 * 1000, GOrestart); // just reset if no answer

}

bool append_ssdi(void)
{
	epromdata[0]++;
	for (i = 0; i < qsid.length(); i++)
		epromdata[i + buf_pointer] = qsid[i];
	buf_pointer += qsid.length();
	epromdata[buf_pointer] = 0;
	buf_pointer++;
	for (i = 0; i < qpass.length(); i++)
		epromdata[i + buf_pointer] = qpass[i];
	buf_pointer += qpass.length();
	epromdata[buf_pointer] = 0;
	buf_pointer++;
	if (buf_pointer > 255) return FALSE; // Exceeded the EEPROM size
	EEPROM.put(0, epromdata);
	EEPROM.commit();
	delay(500);
	return TRUE;
}

void remove_ssdi(void)
{
	epromdata[0]--;
	if (epromdata[0] == 0)
	{
		buf_pointer = 1;
		return; // No saved networks left
	}
	int block = t_ssdi.length() + t_pw.length() + 2;

	int old_pointer = buf_pointer - block; //Dest. pointer - points the ssdi to be removed
	for (i = 0; i < 512 - buf_pointer; i++)
		epromdata[old_pointer + i] = epromdata[buf_pointer + i];
	// Adjust the pointer
	buf_pointer = 1;
	for (i = 0; i < epromdata[0]; i++)
	{
		t_ssdi = String(epromdata + buf_pointer);
		buf_pointer += t_ssdi.length() + 1;
		t_pw = String(epromdata + buf_pointer);
		buf_pointer += t_pw.length() + 1;
	}
}

void GOrestart()
{
	ESP.restart();
}

void ChangeValues(void)
{
	this_second = elapsedSecsToday(now());

	if (OnTime < OffTime)
	{
		if (this_second > OnTime && this_second < OffTime)
			relay_status = true;
		else
			relay_status = false;
	}
	else
	{
		if (this_second > OnTime && this_second < OffTime)
			relay_status = false;
		else
			relay_status = true;

	}
}