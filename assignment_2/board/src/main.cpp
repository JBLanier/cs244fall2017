#include "Arduino.h"
#include "secrets.h"
#include "helpers.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Wire.h>
#include "MAX30105.h"
#include "BigNumber.h"

const int SAMPLE_RATE = 50;
const int NUM_SAMPLES_PER_POST = 50; // Not Necessarily tied to Sample Rate
const int MAX_SAMPLE_TRIPLET_SIZE = 50; // max size example: {"t":1508735981516,"ir":4294967295,"r":4294967295}
const int MAX_SAMPLES_ARRAY_SIZE = ( (MAX_SAMPLE_TRIPLET_SIZE + 1) * NUM_SAMPLES_PER_POST ) + 1;
const int MAX_JSON_STR_SIZE = MAX_SAMPLES_ARRAY_SIZE + 30;

char json_str[MAX_JSON_STR_SIZE];
size_t json_offset = 0;

MAX30105 particleSensor;
byte powerLevel = 0x02;
//Options: 0=Off to 255=50mA 
//powerLevel = 0x02, 0.4mA - Presence detection of ~4 inch
//powerLevel = 0x1F, 6.4mA - Presence detection of ~8 inch
//powerLevel = 0x7F, 25.4mA - Presence detection of ~8 inch
//powerLevel = 0xFF, 50.0mA - Presence detection of ~12 inch

String deviceName = "CS244";
const int HTTP_PORT = 80;
WiFiClient client;
bool aWriteHasFailed = false;


long hz_startTime; //Used to calculate Hz, reset often
int hz_samples_taken = 0; //Used to calculate Hz, reset often

int samples_taken = 0; //Reset for each POST to server

long unblockedValue; //Average IR at power up

void resetJsonString() {
  json_offset = 0;
  char *pwr;
  switch (powerLevel) {
    case 0x02: pwr = "0.4"; break;
    case 0x1F: pwr = "6.4"; break;
    case 0x7F: pwr = "25.4"; break;
    case 0xFF: pwr = "50.0"; break;
    default: pwr = "unkown"; break;
  }
  json_offset += sprintf(json_str, "{\"pwr\":\"%s\",\"samples\":[",pwr);
}

// function to display a big number and free it afterwards
void printBignum (BigNumber & n)
{
  char *s = n.toString();
  Serial.println(s);
  free(s);
}  // end of printBignum

// Returns only when connected to Wifi
void connectToWifi() {
    WiFi.begin(SSID, PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");

        for (int i = 0; i < 3; i++) {
          digitalWrite(LED_BUILTIN, LOW);
          delay(400);
          digitalWrite(LED_BUILTIN, HIGH);
          delay(400);
        }
    } 
}

// Returns only when connected to the Server, connects to Wifi if Necessary
void connectToServer() {
  while (!client.connected()) {
    if (WiFi.status() != WL_CONNECTED) {
      connectToWifi();
    }
    client.connect(SERVER_HOSTNAME, 80);
    digitalWrite(LED_BUILTIN, LOW);
    delay(200);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(200);
  }
  client.setNoDelay(1);
  digitalWrite(LED_BUILTIN, LOW);
}

// Speedy, relies on using existing connection to server, can send multiple packets.
// Doesn't even look at response, doesn't care.
void doBetterPost(const char *path, char *json_content, size_t json_size) {
  String endpointURL = String("http://") + SERVER_HOSTNAME + path;
  
  // create the request and headers
  String request = "POST " + endpointURL + " HTTP/1.1\r\n" +
    "Host: " + String(SERVER_HOSTNAME) + "\r\n" + 
    "Accept: application/json" + "\r\n" + 
    "Connection: Keep-Alive\r\n" +
    "Content-Type: application/json\r\n" +
    "Content-Length: " + String(json_size) + "\r\n\r\n";
  client.flush();
  client.print(request);

  size_t c_offset = 0;
  while (c_offset < json_size) {
    client.flush();
    const uint8_t *json_content_at_offset = (const uint8_t *) &json_content[c_offset];
    size_t result = client.write(json_content_at_offset, WIFICLIENT_MAX_PACKET_SIZE < (json_size - c_offset) ? WIFICLIENT_MAX_PACKET_SIZE : (json_size - c_offset) );
    if (result > 0) {
      c_offset += result;
    } else {
      Serial.println("write failed");
      aWriteHasFailed = true;
      return;
    }
  }
  //Serial.println(aWriteHasFailed);
}

BigNumber getCurrentTime() {
  static BigNumber timestamp_start = BigNumber(-1); //timestamp taken from server at startup
  static long system_millis_at_server_calibration;

  if (timestamp_start == -1) {
    if (WiFi.status() != WL_CONNECTED) {
      connectToWifi();
    }
  
    String url = String("http://") + SERVER_HOSTNAME + "/time";
    Serial.println(String("POST to ") + url);
    if(WiFi.status()== WL_CONNECTED){   //Check WiFi connection status
      
        HTTPClient http;    //Declare object of class HTTPClient
        http.begin(url);    //Specify request destination
        long request_start = millis();
        int httpCode = http.GET(); //Send the request
        String payload = http.getString();                  //Get the response payload
        Serial.println("response code: " + String(httpCode));   //Print HTTP return code
        Serial.println("response data: " + payload + "\n\n");    //Print request response payload
        http.end();  //Close connection
        
        if (httpCode != 200) {
          Serial.print("HTTP Code for getting time: ");
          Serial.println(httpCode);
          return BigNumber(-1);
        }
  
        system_millis_at_server_calibration = millis();
         // factor in latency for timestamp to be sent back over internet to the board.
        timestamp_start = BigNumber(payload.c_str()) + BigNumber((system_millis_at_server_calibration - request_start)/2);

      } else {
         Serial.println("WiFi not connected, POST aborted.");   
         return BigNumber(-1);
      }
  }

  return BigNumber(timestamp_start + BigNumber(millis() - system_millis_at_server_calibration));
}

void setup() {

  // We use bignumber to handle full-size timestamps
  Serial.println("Program started");
  BigNumber::begin(0); //set constructor with max number of decimal digits we care about
  Serial.println("gfd");

  // initialize LED digital pin as an output.
  pinMode(LED_BUILTIN, OUTPUT);
  // Baud rate
  Serial.begin(115200);
  Serial.println("Program started");
  
  // We'll use this connection later to doBetterPost() with the header, "connection: Keep-Alive"
  connectToWifi();

  // Print some fun internet stats
  Serial.println("");
  Serial.println("WiFi connected");
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

  // Say hi to the MAX30105 Particle Sensor
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) //Use default I2C port, 400kHz speed
  {
    Serial.println("MAX30105 was not found. Please check wiring/power. ");
    while (1);
  }

  byte sampleAverage = 4; //Options: 1, 2, 4, 8, 16, 32
  byte ledMode = 2; //Options: 1 = Red only, 2 = Red + IR, 3 = Red + IR + Green
  int sampleRate = 1000; //Options: 50, 100, 200, 400, 800, 1000, 1600, 3200
  int pulseWidth = 411; //Options: 69, 118, 215, 411
  int adcRange = 2048; //Options: 2048, 4096, 8192, 16384
  particleSensor.setup(powerLevel, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange); //Configure sensor with these settings
  particleSensor.setPulseAmplitudeRed(0); //Turn off Red LED
  particleSensor.setPulseAmplitudeGreen(0); //Turn off Green LED

  //Take an average of IR readings at power up to calibrate
  unblockedValue = 0;
  for (byte x = 0 ; x < 32 ; x++)
  {
    unblockedValue += particleSensor.getIR(); //Read the IR value
  }
  unblockedValue /= 32;

  resetJsonString();
  
  while (getCurrentTime() == -1) {
    Serial.println("Failed to calibrate time with server.");
    delay(5000);
  }
  connectToServer();

  // Do time sensitive operations last
  hz_startTime = millis();
  particleSensor.check(); // read in the first (up to 3) samples
}

void loop() {

  if(!client.connected()) {
    Serial.println("Waiting to connect to server");
    connectToServer();
  }

  long frame_start = millis();
  samples_taken++;
  hz_samples_taken++;

  char *ir = BigNumber(particleSensor.getFIFOIR()).toString();
  char *r = BigNumber(particleSensor.getFIFORed()).toString();
  char *time_str = getCurrentTime().toString();
  
  char *json_str_at_offset = &json_str[json_offset];
  json_offset += sprintf(json_str_at_offset, "{\"t\":%s,", time_str);
  json_str_at_offset = &json_str[json_offset];  
  json_offset += sprintf(json_str_at_offset, "\"ir\":%s,", ir);
  json_str_at_offset = &json_str[json_offset];  
  json_offset += sprintf(json_str_at_offset, "\"r\":%s},", r);

  // json_offset += sprintf(json_str_at_offset, "{\"t\":%s,", "1508735981516");
  // json_str_at_offset = &json_str[json_offset];  
  // json_offset += sprintf(json_str_at_offset, "\"ir\":%ld,", 1294967295);
  // json_str_at_offset = &json_str[json_offset];
  // json_offset += sprintf(json_str_at_offset, "\"r\"%ld},", 1294967295);
  
  //Real time readings
  Serial.print("R[");
  Serial.print(r);
  Serial.print("] IR[");
  Serial.print(ir);
  Serial.print("] Hz[");
  Serial.print((float)hz_samples_taken / ((millis() - hz_startTime) / 1000.0), 2);
  Serial.print("]");
  Serial.println();

  free(time_str);
  free(ir);
  free(r);
  

  particleSensor.nextSample(); //We're finished with this sample so move to next sample
  if (!particleSensor.available()) { //are we out of new data?
    particleSensor.check(); //Check the sensor, read up to 3 samples
  }

  //reset these stats to keep fps counter fresh
  if (hz_samples_taken >= 180) {
    hz_samples_taken = 0;
    hz_startTime = millis();
  }

  if (samples_taken >= NUM_SAMPLES_PER_POST) {

    json_str[json_offset-1] = ']';
    json_str_at_offset = &json_str[json_offset];
    json_offset += sprintf(json_str_at_offset, "}\0");

    // ir_sample_str[ir_offset - 1] = ']';
    // r_sample_str[r_offset - 1] = ']';
    // times_str[times_offset - 1] = ']';
    // ir_sample_str[ir_offset] = '\0';
    // r_sample_str[r_offset] = '\0';
    // times_str[times_offset] = '\0';
    
    // size_t json_offset = 0;
    // char *json_at_offset = &json_str[json_offset];
    // json_offset += sprintf(json_at_offset, "{\"data\":{\"times\":%s,\"ir\":",times_str);
    // json_at_offset = &json_str[json_offset];
    // json_offset += sprintf(json_at_offset, "%s,\"r\":",ir_sample_str);
    // json_at_offset = &json_str[json_offset];
    // json_offset += sprintf(json_at_offset, "%s}}",r_sample_str);

    // Serial.write(json_str,json_offset);
    // Serial.println("");
    doBetterPost("/samples",json_str, json_offset);
    resetJsonString();
    samples_taken = 0;
  }

  //Ensure a max rate of 50Hz
  long frame_duration = millis() - frame_start;
  long wait_time = 20 - frame_duration;
  if (wait_time > 0) {
    delay(wait_time);
  }
  
  // //Print frame duration if we POSTed
  // if (samples_taken == 0) {
  //   Serial.println(frame_duration);    
  // }
  
}

