#include <ESP8266WiFi.h> //ESP8266 WiFi Library
#include "HTTPSRedirect.h"
#include "DebugMacros.h"
#include "PMS.h" //PMS Library
#include "DHT.h" //DHT11 Library

#define DHTTYPE DHT11   // DHT 11

/*Put your SSID & Password*/
const char* ssid = "xxx";  // Enter SSID here
const char* password = "xxx";  //Enter Password here

PMS pms(Serial);
PMS::DATA data;

// DHT Sensor
uint8_t DHTPin = D2; //DHT Pin Sensor
               
// Initialize DHT sensor.
DHT dht(DHTPin, DHTTYPE);                

float Temperature;
float Humidity;

//String's for Google Docs
String sheetHum = "";
String sheetTemp = "";
String sheetpm01val = "";
String sheetpm25val = "";
String sheetpm10val = "";

String devid = "AQ-001"; //Device ID to seperate from others

//Google Docs Connection 
const char* host = "script.google.com";
const char *GScriptId = "Axxx"; // Replace with your own google script id
const int httpsPort = 443; //the https port is same

// echo | openssl s_client -connect script.google.com:443 |& openssl x509 -fingerprint -noout
const char* fingerprint = "";

//const uint8_t fingerprint[20] = {};

String url = String("/macros/s/") + GScriptId + "/exec?value=Temperature";  // Write Teperature to Google Spreadsheet at cell A1

// Fetch Google Calendar events for 1 week ahead
String url2 = String("/macros/s/") + GScriptId + "/exec?cal";  // Write to Cell A continuosly

//replace with sheet name not with spreadsheet file name taken from google
String payload_base =  "{\"command\": \"appendRow\", \
                    \"sheet_name\": \"logs\", \
                       \"values\": ";
String payload = "";

HTTPSRedirect* client = nullptr;

// used to store the values of free stack and heap before the HTTPSRedirect object is instantiated
// so that they can be written to Google sheets upon instantiation


void setup()
{
  Serial.begin(9600);   // GPIO1, GPIO3 (TX/RX pin on ESP-12E Development Board)
  //Serial1.begin(9600);  // GPIO2 (D4 pin on ESP-12E Development Board)

//CODE FOR DHT SENSOR
  pinMode(DHTPin, INPUT);

  dht.begin(); 

//CODE FOR WIFI
  Serial.println("Connecting to ");
  Serial.println(ssid);

  //connect to your local wi-fi network
  WiFi.begin(ssid, password);

  //check wi-fi is connected to wi-fi network
  while (WiFi.status() != WL_CONNECTED) {
  delay(1000);
  Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected..!");
  Serial.print("Got IP: ");  Serial.println(WiFi.localIP());

//CODE FOR GOOGLE
  
  // Use HTTPSRedirect class to create a new TLS connection
  client = new HTTPSRedirect(httpsPort);
  client->setInsecure();
  client->setPrintResponseBody(true);
  client->setContentTypeHeader("application/json");
  Serial.print("Connecting to ");
  Serial.println(host);          //try to connect with "script.google.com"
  
  // Try to connect for a maximum of 5 times then exit
  bool flag = false;
  for (int i = 0; i < 5; i++) {
    int retval = client->connect(host, httpsPort);
    if (retval == 1) {
      flag = true;
      break;
    }
    else
      Serial.println("Connection failed. Retrying...");
  }
  if (!flag) {
    Serial.print("Could not connect to server: ");
    Serial.println(host);
    Serial.println("Exiting...");
    return;
  }

// Finish setup() function in 1s since it will fire watchdog timer and will reset the chip.
//So avoid too many requests in setup()
  Serial.println("\nWrite into cell 'A1'");
  Serial.println("------>");
  // fetch spreadsheet data
  client->GET(url, host);
  
  Serial.println("\nGET: Fetch Google Calendar Data:");
  Serial.println("------>");
  
  // fetch spreadsheet data
  client->GET(url2, host);
 Serial.println("\nStart Sending Sensor Data to Google Spreadsheet");
  
  // delete HTTPSRedirect object
  delete client;
  client = nullptr;
}

void loop()
{
  if (pms.read(data))
  {
    Serial.print("DeviceID: ");
    Serial.println(devid);
    
    Serial.print("PM 1.0 (ug/m3): ");
    Serial.println(data.PM_AE_UG_1_0);

    Serial.print("PM 2.5 (ug/m3): ");
    Serial.println(data.PM_AE_UG_2_5);

    Serial.print("PM 10.0 (ug/m3): ");
    Serial.println(data.PM_AE_UG_10_0);

    Temperature = dht.readTemperature(); // Gets the values of the temperature
    Humidity = dht.readHumidity(); // Gets the values of the humidity 
  
    Serial.print("Temperature: ");
    Serial.println(Temperature);
   
    Serial.print("Humidity: ");
    Serial.println(Humidity);

    Serial.println();

//Insert metrics into "sheet" variables
    sheetHum = String(Humidity);
    sheetTemp = String(Temperature);
    sheetpm01val = String(data.PM_AE_UG_1_0);
    sheetpm25val = String(data.PM_AE_UG_2_5);
    sheetpm10val = String(data.PM_AE_UG_10_0);

    static int error_count = 0;
    static int connect_count = 0;
    const unsigned int MAX_CONNECT = 20;
    static bool flag = false;

    payload = payload_base + "\"" + devid + "," + sheetHum + "," + sheetTemp + "," + sheetpm01val + "," + sheetpm25val + "," + sheetpm10val + "\"}";

    if (!flag) {
    client = new HTTPSRedirect(httpsPort);
    client->setInsecure();
    flag = true;
    client->setPrintResponseBody(true);
    client->setContentTypeHeader("application/json");
  }
  if (client != nullptr) {
    if (!client->connected()) {
      client->connect(host, httpsPort);
      client->POST(url2, host, payload, false);
      Serial.print("Sent : ");  Serial.println("Device ID, Humidity, Temperature, PM01, PM2.5 and PM10");
    }
  }
  else {
    DPRINTLN("Error creating client object!");
    error_count = 5;
  }
  if (connect_count > MAX_CONNECT) {
    connect_count = 0;
    flag = false;
    delete client;
    return;
  }
  Serial.println("POST or SEND Sensor data to Google Spreadsheet:");
  if (client->POST(url2, host, payload)) {
    ;
  }
  else {
    ++error_count;
    DPRINT("Error-count while connecting: ");
    DPRINTLN(error_count);
  }
  if (error_count > 3) {
    Serial.println("Halting processor...");
    delete client;
    client = nullptr;
    Serial.printf("Final free heap: %u\n", ESP.getFreeHeap());
    Serial.printf("Final stack: %u\n", ESP.getFreeContStack());
    Serial.flush();
    ESP.deepSleep(0);
  }


    delay(3000);
  }
  
  // Do other stuff...
}
