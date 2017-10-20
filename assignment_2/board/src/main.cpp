#include "Arduino.h"
#include "secrets.h"
#include "helpers.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

String deviceName = "CS244";
const int HTTP_PORT = 80;

void connectToWifi() {

    WiFi.begin(SSID, PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");

        for (int i = 0; i < 3; i++) {
          digitalWrite(LED_BUILTIN, LOW);
          delay(150);
          digitalWrite(LED_BUILTIN, HIGH);
          delay(150);
        }
    }
    Serial.println("");
    Serial.println("WiFi connected");
    // Print the IP address
    Serial.println(WiFi.localIP());

    byte mac[6];
    WiFi.macAddress(mac);
    char MAC_char[18]="";
    for (int i = 0; i < sizeof(mac); ++i) {
        sprintf(MAC_char, "%s%02x:", MAC_char, mac[i]);
    }
    Serial.print("Mac address: ");
    Serial.print(MAC_char);

    Serial.println("\n");
}

// Returns True for success
bool doPostToServer(const char *path, const char *json_content) {

  String url = String("http://") + SERVER_HOSTNAME + path;
  Serial.println(String("POST to ") + url);
  if(WiFi.status()== WL_CONNECTED){   //Check WiFi connection status
    
      HTTPClient http;    //Declare object of class HTTPClient
      http.begin(url);    //Specify request destination
      http.addHeader("Content-Type", "application/json");  //Specify content-type header
      int httpCode = http.POST(String(json_content));   //Send the request
      String payload = http.getString();                  //Get the response payload
      Serial.println("response code: " + String(httpCode));   //Print HTTP return code
      Serial.println("response data: " + payload + "\n\n");    //Print request response payload
      http.end();  //Close connection
      
      return httpCode > 0;
    
    } else {
       Serial.println("WiFi not connected, POST aborted.");   
       return false;
    }
}

void setup() {
  // initialize LED digital pin as an output.
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  Serial.println("Program started");
  connectToWifi();
  digitalWrite(LED_BUILTIN, LOW);
}

void loop() {
  doPostToServer("/","{\"message\": \"Sup Dawg\"}");
  delay(10000);  //Send a request every 30 seconds
}

