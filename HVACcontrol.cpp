#include <Arduino.h>
#include <esp8266wifi.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <esp8266webserver.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "default.h"


// WIFI parameters
const char* SSID = MY_WIFI_SSID;
const char* PASSWORD = MY_WIFI_PASSWORD;
String NodeRedAlive = "CONNECTED";
String MyIP = "";

// MQTT Config
const char* BROKER_MQTT = MY_BROKER_MQTT_IP; // MQTT Broker IP
int BROKER_PORT = MY_BROKER_PORT;
const char* MQTT_USER = MY_MQTT_USER;
const char* MQTT_PASS = MY_MQTT_PASS;

WiFiClient espClient;
PubSubClient MQTT(espClient); // MQTT client setup
String MQTTConnected = "";


//Setup
String DeviceID = "ESP HVAC"; //set unique device name for each device
String SystemID = "HVAC Control"; //Family of devices for MQTT

String SystemOne = "Fan on";
String SystemTwo = "Call for Heat";
String SystemThree = "Call for Cool";
String SystemFour = "Call for Humm";  // not yet used

//NPT config
WiFiUDP ntpUDP;
NTPClient TimeClient(ntpUDP, "ca.pool.ntp.org", -21600 , 60000); // use canadian pool and set offset for local -6 hours


//pin config for linknode R4
int relayOne = 12;
int relayTwo = 13;
int relayThree = 14;
int relayFour = 16;


//MQTT topics
String TopicRelayOne = "Home/HVAC/1/Fan/1"; //Relay1 FAN MQTT sub
String TopicRelayTwo = "Home/HVAC/1/Heat/1";  //Relay2 HEAT MQTT sub
String TopicRelayThree = "Home/HVAC/1/Cooling/1";  //Relay3 AC MQTT sub
String TopicRelayFour = "Home/HVAC/1/Humm/1";  //Relay4 HUMM MQTT sub
String TopicStatusNR = "Home/Status/WatchDog/1";  //Node red system health sub
String TopicAllCmd = "Home/HVAC/1/System/1"; // send commands to entire system

String TopicStatusOne = "Home/HVAC/1/Status/Fan/1"; //Relay1 status Publish
String TopicStatusTwo = "Home/HVAC/1/Status/Heat/1"; //Relay2 status Publish
String TopicStatusThree = "Home/HVAC/1/Status/Cooling/1";  //Relay3 status Publish
String TopicStatusFour = "Home/HVAC/1/Status/Humm/1";  //Relay4 status Publish
String TopicStatusAll = "Home/HVAC/1/Status"; // topic status all Publish
String TopicStatusDevice = "Home/HVAC/1/Status/WatchDog"; //node keep alive Publish

//Web server
ESP8266WebServer webServer(80);
String myPage = "";


//Timer Setup
double WDTimer = 0;


class tempcontrol {

	public:
		int tempsetpoint; //
		int lowCutIn;  //low temp cut in for heating system
		int HighCutIn;  //high temperature cut in for cooling system
		int HeatOn;  //turn the heating systm ON
		int CoolingOn; //turn the cooling system ON
		int FanOn; //turn the FAN always ON
		String DoAction; //status of system Heating, Cooling, Off

	tempcontrol();
	bool adjust();
	bool publish();
	bool publish(String output);
	bool updatesetpoints(int tempSP, int Low, int high);
	String getMQTT();


	private:

	String output;

};

tempcontrol::tempcontrol(){
	//setup init values generic setpoints
	this -> tempsetpoint = 20;
	this -> lowCutIn = 18;
	this -> HighCutIn = 25;
	this -> HeatOn = 0;
	this -> CoolingOn = 0;
	this -> FanOn = 1;
}

bool tempcontrol::adjust(){

	//check current temperature


	//check if heating or cooling required and enabled


	//perform action

	return 0;
}

String tempcontrol::getMQTT(){

	String mystring = "";

	mystring = "{\"Device\" : \""  + DeviceID + "\" , \"SetPoint\" : " + this -> tempsetpoint  + " , \"Status\" : \"" + this -> DoAction + "\"}" ;


	this-> output = mystring;

	return mystring;

}

bool tempcontrol::publish(){

	if(!MQTT.publish( TopicStatusAll.c_str()  , this -> output.c_str() )){
		Serial.println("MQTT publish failed");
		return 0;

	};

	return 1;
}

bool tempcontrol::publish(String output){

	if(!MQTT.publish( TopicStatusAll.c_str()  , output.c_str() )){
			Serial.println("MQTT publish failed");
			return 0;

		};

		return 1;

}

bool tempcontrol::updatesetpoints(int tempSP, int Low, int high){
	// update setpoints

	this -> tempsetpoint = tempSP;
	this -> lowCutIn = Low;
	this -> HighCutIn = high;


	return 1;
}

tempcontrol MyTempControl;




//  functions and stuff
void initPins() {
	pinMode(relayOne, OUTPUT);
	pinMode(relayTwo, OUTPUT);
	pinMode(relayThree, OUTPUT);
	pinMode(relayFour, OUTPUT);
	digitalWrite(relayOne, 0);
	digitalWrite(relayTwo, 0);
	digitalWrite(relayThree, 0);
	digitalWrite(relayFour, 0);
}


void initSerial() {
	Serial.begin(115200);
}


void initWiFi() {
	delay(10);
	Serial.println();
	Serial.print("Connecting: ");
	Serial.println(SSID);

	WiFi.begin(SSID, PASSWORD); // Wifi Connect
	while (WiFi.status() != WL_CONNECTED) {
		delay(100);
		Serial.print(".");
	}

	Serial.println("");
	Serial.print(SSID);
	Serial.println(" | IP ");
	Serial.println(WiFi.localIP());

	MyIP = String(WiFi.localIP()[0]) + "." + String(WiFi.localIP()[1]) + "." + String(WiFi.localIP()[2]) + "." + String(WiFi.localIP()[3]);
}

// Receive messages/
void mqtt_callback(char* topic, byte* payload, unsigned int length) {

	String message;
	StaticJsonBuffer<600> jsonBuffer;
	double ActionTimeStamp;
	double ActionDelay;

	//split out payload in message, parse into JSON buffer;
	for (int i = 0; i < length; i++) {
		char c = (char) payload[i];
		message += c;
	}

	// serial debug JSON msg and topic recieved
	Serial.print("Topic ");
	Serial.print(topic);
	Serial.print(" | ");
	Serial.println(message);

	// JSON parser
	JsonObject& parsed = jsonBuffer.parseObject(message);
	if (!parsed.success()) {
		Serial.println("parseObject() failed");
		return;
	}


	//check if there is a timestamp included
	if(!parsed["TimeStamp"])	{
		ActionTimeStamp = TimeClient.getEpochTime();

	}
	else{
		ActionTimeStamp = parsed["TimeStamp"];
	}
	Serial.println(" New time = " + String(ActionTimeStamp));



	if(!strcmp(topic, TopicAllCmd.c_str())){  // Device commands check



	}

	//Fan message
	if (!strcmp(topic, TopicRelayOne.c_str())) {

		if(parsed["Action"]){
			MyTempControl.FanOn = 1;
			Serial.println("Fan is on");
		}
		else{
			MyTempControl.FanOn = 0;
			Serial.println("Fan is off");

		}
		}

	if (!strcmp(topic, TopicRelayTwo.c_str())) {

		if(parsed["Action"]){
			MyTempControl.HeatOn = 1;
			Serial.println("Heat is on");
		}
		else{
			MyTempControl.HeatOn = 0;
			Serial.println("Heat is off");

		}

	}

	if (!strcmp(topic, TopicRelayThree.c_str())) {

		if(parsed["Action"]){
			MyTempControl.CoolingOn = 1;
			Serial.println("Cooling is on");
		}
		else{
			MyTempControl.CoolingOn = 0;
			Serial.println("Cooling is off");

		}



	}

	if (!strcmp(topic, TopicRelayFour.c_str())) {

	}



  message = "";
  Serial.println();
  Serial.flush();

}

// MQTT Broker connection
void initMQTT() {
  MQTT.setServer(BROKER_MQTT, BROKER_PORT);
  MQTT.setCallback(mqtt_callback);
}

//MQTT keep alive
void reconnectMQTT() {
	while (!MQTT.connected()) {
		Serial.print("Attempting MQTT Broker Connection: ");
		Serial.println(BROKER_MQTT);
		if (MQTT.connect(DeviceID.c_str(), MQTT_USER, MQTT_PASS)) {
			Serial.println("Connected");

			MQTT.subscribe(TopicRelayOne.c_str(), 0);
			MQTT.subscribe(TopicRelayTwo.c_str(), 0);
			MQTT.subscribe(TopicRelayThree.c_str(), 0);
			MQTT.subscribe(TopicRelayFour.c_str(), 0);
			MQTT.subscribe(TopicStatusNR.c_str(), 0);
			MQTT.subscribe(TopicAllCmd.c_str(), 0);

			MQTTConnected = "CONNECTED";
		} else {
			Serial.println("Connection Failed");
			Serial.println("Attempting Reconect in 2 seconds");

			MQTTConnected = "DISABLED";
			delay(2000);
		}
	}
}

//Wifi Keep alive
void recconectWiFi() {
	while (WiFi.status() != WL_CONNECTED) {
		delay(100);
		Serial.print(".");
	}
}


int WatchDogTimer(){
	// creates a stay alive message with time stamp every 20 seconds back to listeners

	String BufferTxt = "{\"Device\" : \"" + DeviceID
			+ "\" , \"Status\" : \"Alive\" ,  \"TimeStamp\" : "
			+ String(TimeClient.getEpochTime()) + " , \"IP\" : \"" + MyIP + "\"}";

	//Debug
	// Serial.println(BufferTxt);

	if (!MQTT.publish(TopicStatusDevice.c_str(), BufferTxt.c_str(),	sizeof(BufferTxt))) {

		Serial.println("MQTT pub error  !");
	}


}



void setup() {
  initPins();
  initSerial();
  initWiFi();

  initMQTT();

  // start webserver!
  //startWebserver();

  TimeClient.begin();
  TimeClient.update();
  WDTimer = millis();
  Serial.println("Starup time - Epoch time " + String(TimeClient.getEpochTime()) + " - Formated time - Day" + TimeClient.getDay() + " - " + TimeClient.getFormattedTime());


}


void loop() {

//webServer.handleClient();


  if (!MQTT.connected()) {
	reconnectMQTT(); // Retry Worker MQTT Server connection
  }
  recconectWiFi(); // Retry WiFi Network connection
  MQTT.loop();




//send out watchdog timer update every 20 seconds
if(millis() > (WDTimer + 20000)){

	WDTimer = millis();
	TimeClient.update();
//	WatchDogTimer();
}



}

