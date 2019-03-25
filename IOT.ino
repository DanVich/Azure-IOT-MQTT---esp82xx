/*
*******************************************************
Azure IoT Hub with sensors on ESP8266
*******************************************************
This sample shows how to use Azure IoT Hub. For sending sensors data and receiving data from IoT hub.

Thanks for inspiration and samples 
https://github.com/noopkat/azure-iothub-pubsub-esp8266
https://github.com/Azure-Samples/iot-hub-feather-huzzah-client-app

 
--------------------------------------------------------------------------------------------------------
*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "time.h"

// If you using sensors uncomment next line
#define useSensors true
#define LEDPin 16


#ifdef useSensors
#include "CO2Sensor.h"
#include "DHTesp.h"
#include <Wire.h>
#include <BH1750.h>
#endif



#ifdef ESP32
#pragma message(THIS EXAMPLE IS FOR ESP8266 ONLY!)
#error Select ESP8266 board.
#endif

//--------------------------------------------------------
// Initial settings
// WiFi Settings
const char* wifi_ssid = "<wifi_ssid>";
const char* wifi_password = "<wifi_password>";

// IoT Hub url 
// example: <myiothub>.azure-devices.net
const char* iothub_url = "<myiothub>.azure-devices.net";

// Device ID 
// example: <device>
const char* iothub_deviceid = "<device>";

// IoT Hub user name for device 
// example: <myiothub>.azure-devices.net/<device>
const char* iothub_user = "<myiothub>.azure-devices.net/<device>";

// IoT Hub Device SAS Token - can be generate by Azure IoT Hub plugin from Visual Studio Code 
// example "SharedAccessSignature sr=<myiothub>.azure-devices.net%2Fdevices%2F<device>&sig=123&se=456"
const char* iothub_sas_token = "SharedAccessSignature sr=<myiothub>.azure-devices.net%2Fdevices%2F<device>&sig=123&se=456";

// Default topic feed for subscribing is "devices/<device>/messages/devicebound/#""
const char* iothub_subscribe_endpoint = "devices/<device>/messages/devicebound/#";

// Default topic feed for publishing is "devices/<device>/messages/events/"
const char* iothub_publish_endpoint = "devices/<device>/messages/events/";



WiFiClientSecure espClient;
PubSubClient client(espClient);

#ifdef useSensors
//CO2 Sensor initialize
CO2Sensor co2Sensor(A0, 0.99, 100);
//DHT - temperature and humidity sensor 
DHTesp dht;
//BH1750 digital Light sensor
BH1750 lightMeter(0x23);
#endif

long lastMsg = 0;
int count=0;
bool identify = false;
 

// Connect to Wifi
void setup_wifi() {
  delay(10);

  Serial.println();
  Serial.print("Connecting to wifi");

  WiFi.begin(wifi_ssid, wifi_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // Get connection info
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

// Connect to Azure IoT Hub using by MQTT protocol
void connect_mqtt() {
  // Wait for connection
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(iothub_deviceid, iothub_user, iothub_sas_token)) {
      Serial.println("connected");
      // subscribe to endpoint
      client.subscribe(iothub_subscribe_endpoint);
    } else {
      // diagnosis connection problems  
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println("try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


// Get actual time from ntp server
void initTime()
{
    //Time is in epoch UNIX Time
    time_t epochTime;
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    delay(1000);
    
    // loop for getting time
    while (true)
    {
        epochTime = time(NULL);

        if (epochTime == 0)
        {
            Serial.println("Fetching NTP epoch time failed! Waiting 2 seconds to retry.");
            delay(2000);
        }
        else
        {
            Serial.printf("Fetched NTP epoch time is: %lu.\r\n", epochTime);
            break;
        }
    }
}


// Blink effect for identify device LED in GPIO pin, blinking count respond id number (first device blink 1 times, second device blink 2 times)
void blinkIdentify(int deviceIDNumber,int delayTime, int GPIOPin){
  
  for(int i=0; i< deviceIDNumber;i++){
    
    digitalWrite(GPIOPin, HIGH);
    delay(delayTime); 
    digitalWrite(GPIOPin, LOW);
    delay(delayTime);
    
  }
}

// Callback for receive message from IoT Hub
void callback(char* topic, byte* payload, unsigned int length) {
  
  // Prepare char array, length + 1 for last endchar \0 
  char byteArray[length+1];
  
  // print message to serial for debugging
  Serial.print("Message arrived: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    byteArray[i] = (char)payload[i];
  }
  byteArray[length] = '\0';

  // Convert message to string 
  String message = String(byteArray);
  
  // Cmd message ("identify") for enable identify function
  if(message=="identify"){
    identify = true;
  }
  
    // Cmd message ("stop") for disable identify function
  if(message=="stop"){
    identify = false;
  }

}

void setup() {
  // Begin serial for debugging purposes
  Serial.begin(115200);

  // Setup pinmode for GPIOPin for LED as output    
  pinMode(LEDPin, OUTPUT); 
  
  
  //  Call function for connect to wifi
  setup_wifi();

  // Call function for initTime   
  initTime();

  // Setup connection and callback for MQTT server
  client.setServer(iothub_url,8883);
  client.setCallback(callback);


#ifdef useSensors
  // Calibrate CO2 Sensor
  co2Sensor.calibrate();

  // Setup DHT22 sensor GPIO 0 pin D3 (have pull-up 10k)
  dht.setup(0, DHTesp::DHT22); 

  // Setup light sensor
  lightMeter.begin(BH1750_CONTINUOUS_HIGH_RES_MODE);
 #else
Serial.println("Warning ! Sensors is disabled, using simulatedData.");
 #endif

  // Connect to Azure IoT Hub 
  connect_mqtt();
}


// Main loop 
void loop() {

  

  // If enable identify function
  if(identify){
    // Blink 2 times with delay 500ms with LED on LEDPin   
    blinkIdentify(2,500, LEDPin); 
  }
  
 

  client.loop();
  long now = millis();
     
  
// Publish data and debug mqtt connection every 30 seconds
if (now - lastMsg > 30000) {
    lastMsg = now;
    count++;

    Serial.print("is MQTT client is still connected: ");
    Serial.println(client.connected());

    // Check if Azure IoT Hub Connected, if no reconnect
    if(client.connected()==0){
      connect_mqtt();
    }



// Sensor reading
#ifdef useSensors
  // Light sensor read
  uint16_t lux = lightMeter.readLightLevel();
  Serial.print("Light: ");
  Serial.print(lux);
  Serial.println(" lx");

  
  // CO2 Sensor read
  int CO2Value = co2Sensor.read();
  // Print info to serial
  Serial.print("Concentration CO2: ");
  Serial.print(CO2Value);
  Serial.println(" ppm.");
  // If value > 1000
  // Print warning for high Concentration CO2
  if (CO2Value > 1000) {
    Serial.println("High concentration CO2, more than 1000ppm. Ventilate !");
  }

  // Temperature and humidity sensor read
  delay(dht.getMinimumSamplingPeriod());
  // Get data from sensor
  float humidity = dht.getHumidity();
  float temperature = dht.getTemperature();

  // Print values to serial
  Serial.println("Status\tHumidity (%)\tTemperature (C)\t(F)\tHeatIndex (C)\t(F)");
  Serial.print(dht.getStatusString());
  Serial.print("\t");
  Serial.print(humidity, 1);
  Serial.print("\t\t");
  Serial.print(temperature, 1);
  Serial.print("\t\t");
  Serial.print(dht.toFahrenheit(temperature), 1);
  Serial.print("\t\t");
  Serial.print(dht.computeHeatIndex(temperature, humidity, false), 1);
  Serial.print("\t\t");
  Serial.println(dht.computeHeatIndex(dht.toFahrenheit(temperature), humidity, true), 1);


 // Get actual time 
 time_t nowTime = time(NULL);
 Serial.println(ctime(&nowTime));


// Creating JSON for send data
DynamicJsonDocument root(200);
// Setup JSON keys and values
    root["device"] = "deviceTwo";
    root["time"] = ctime(&nowTime);
    root["CO2"] = CO2Value;
    root["humidity"] = humidity;
    root["temperature"] = temperature;
    root["light"] = lux;
    root["count"] = count;

    // Convert json to buffer for publishing
    char Buffer[256];

    // Serialize JSON
    serializeJson(root, Buffer);
    
    // Publish data to IoTHub
    client.publish(iothub_publish_endpoint, Buffer);

#else

//if not using sensor
// Get actual time 
 time_t nowTime = time(NULL);
 Serial.println(ctime(&nowTime));


// Creating JSON for send data
DynamicJsonDocument root(200);
// Setup JSON keys and values
    root["device"] = "deviceTwo";
    root["time"] = ctime(&nowTime);
    root["data"] = 23.0;
    root["simulatedData"] = true;
    root["count"] = count;

    // Convert json to buffer for publishing
    char Buffer[256];

    // Serialize JSON
    serializeJson(root, Buffer);

    // Publish data to IoTHub
    client.publish(iothub_publish_endpoint, Buffer);

#endif

  }
}