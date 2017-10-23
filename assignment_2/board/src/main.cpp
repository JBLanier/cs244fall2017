#include "Arduino.h"
#include "secrets.h"
#include "helpers.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Wire.h>
#include "MAX30105.h"

const int SAMPLE_RATE = 50;
const int MAX_SAMPLE_STRING_SIZE = 11; //includes a comma character at the end
const int MAX_TIME_STRING_SIZE = 14; //includes a comma character at the end
const int NUM_SAMPLES_PER_POST = 50;
const int MAX_SAMPLES_SIZE = MAX_SAMPLE_STRING_SIZE*NUM_SAMPLES_PER_POST + 2; //+2 for '[' and ']'
const int MAX_TIMES_SIZE = MAX_TIME_STRING_SIZE*NUM_SAMPLES_PER_POST + 2; //+2 for '[' and ']'
char ir_sample_str[MAX_SAMPLES_SIZE];
char r_sample_str[MAX_SAMPLES_SIZE];
char times_str[MAX_TIMES_SIZE];
size_t ir_offset = 0;
size_t r_offset = 0;
size_t times_offset = 0;

char json_str[MAX_TIMES_SIZE + MAX_SAMPLES_SIZE*2 + 45];

MAX30105 particleSensor;

String deviceName = "CS244";
const int HTTP_PORT = 80;
WiFiClient client;
bool aWriteHasFailed = false;


long hz_startTime; //Used to calculate Hz, reset often
long startTime; //Reset for each connection made to server

int hz_samples_taken = 0; //Used to calculate Hz, reset often
int samples_taken = 0; //Reset for each POST to server

long unblockedValue; //Average IR at power up

void resetSampleStrings() {
  ir_offset = 1;
  r_offset = 1;
  times_offset = 1;
  ir_sample_str[0] = '[';
  r_sample_str[0] = '[';
  times_str[0] = '[';
}

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

void connectToClient() {
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


void doBetterPost(const char *path, char *json_content, size_t json_size) {
  String endpointURL = String("http://") + SERVER_HOSTNAME + path;
  
  // create the request and headers
  String request = "POST " + endpointURL + " HTTP/1.1\r\n" +
    "Host: " + String(SERVER_HOSTNAME) + "\r\n" + 
    "Accept: application/json" + "\r\n" + 
    "Connection: Keep-Alive\r\n" +
    "Content-Type: application/json\r\n" +
    "Content-Length: " + String(json_size) + "\r\n\r\n";
  // This will send the request and headers to the server
  client.flush();
  client.print(request);

  // now we need to chunk the payload into 1000 byte chunks
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

// Returns True for success
bool doPostToServer(const char *path, uint8_t *json_content, size_t json_size) {

  String url = String("http://") + SERVER_HOSTNAME + path;
  Serial.println(String("POST to ") + url);
  if(WiFi.status()== WL_CONNECTED){   //Check WiFi connection status
    
      HTTPClient http;    //Declare object of class HTTPClient
      http.begin(url);    //Specify request destination
      http.addHeader("Content-Type", "application/json");  //Specify content-type header
      int httpCode = http.POST(json_content, json_size);   //Send the request
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
  connectToClient();

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

  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) //Use default I2C port, 400kHz speed
  {
    Serial.println("MAX30105 was not found. Please check wiring/power. ");
    while (1);
  }

  //Setup to sense up to 18 inches, max LED brightness
  byte powerLevel = 0x1F; //Options: 0=Off to 255=50mA 
  //powerLevel = 0x02, 0.4mA - Presence detection of ~4 inch
  //powerLevel = 0x1F, 6.4mA - Presence detection of ~8 inch
  //powerLevel = 0x7F, 25.4mA - Presence detection of ~8 inch
  //powerLevel = 0xFF, 50.0mA - Presence detection of ~12 inch

  byte sampleAverage = 4; //Options: 1, 2, 4, 8, 16, 32
  byte ledMode = 2; //Options: 1 = Red only, 2 = Red + IR, 3 = Red + IR + Green
  int sampleRate = 1000; //Options: 50, 100, 200, 400, 800, 1000, 1600, 3200
  int pulseWidth = 411; //Options: 69, 118, 215, 411
  int adcRange = 2048; //Options: 2048, 4096, 8192, 16384

  particleSensor.setup(powerLevel, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange); //Configure sensor with these settings

  particleSensor.setPulseAmplitudeRed(0); //Turn off Red LED
  particleSensor.setPulseAmplitudeGreen(0); //Turn off Green LED

  //Take an average of IR readings at power up
  unblockedValue = 0;
  for (byte x = 0 ; x < 32 ; x++)
  {
    unblockedValue += particleSensor.getIR(); //Read the IR value
  }
  unblockedValue /= 32;

  resetSampleStrings();

  // Do time sensitive operations last
  startTime = millis();
  Serial.println(startTime);
  hz_startTime = startTime;
  particleSensor.check(); // read in the first (up to 3) samples
}

void loop() {

  if(!client.connected()) {
    connectToClient();
  }

  long frame_start = millis();
  samples_taken++;
  hz_samples_taken++;

  int ir = particleSensor.getFIFOIR();
  int r = particleSensor.getFIFORed();

  char *ir_sample_str_at_offset = &ir_sample_str[ir_offset];
  char *r_sample_str_at_offset = &r_sample_str[r_offset];
  char *times_str_at_offset = &times_str[times_offset];
  ir_offset += sprintf(ir_sample_str_at_offset, "%ld,", ir);
  r_offset += sprintf(r_sample_str_at_offset, "%ld,", r);
  times_offset += sprintf(times_str_at_offset, "%ld,", frame_start);
  // ir_offset += sprintf(ir_sample_str_at_offset, "4294967295,"); //testing with max values
  // r_offset += sprintf(r_sample_str_at_offset, "4294967295,");
  // times_offset += sprintf(times_str_at_offset, "1508735981516,");
  
  // Serial.print("R[");
  // Serial.print(r);
  // Serial.print("] IR[");
  // Serial.print(ir);
  // Serial.print("] Hz[");
  // Serial.print((float)hz_samples_taken / ((millis() - hz_startTime) / 1000.0), 2);
  // Serial.print("]");
  // Serial.println();

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

    ir_sample_str[ir_offset - 1] = ']';
    r_sample_str[r_offset - 1] = ']';
    times_str[times_offset - 1] = ']';
    ir_sample_str[ir_offset] = '\0';
    r_sample_str[r_offset] = '\0';
    times_str[times_offset] = '\0';
    
    size_t json_offset = 0;
    char *json_at_offset = &json_str[json_offset];
    json_offset += sprintf(json_at_offset, "{\"data\":{\"times\":%s,\"ir\":",times_str);
    json_at_offset = &json_str[json_offset];
    json_offset += sprintf(json_at_offset, "%s,\"r\":",ir_sample_str);
    json_at_offset = &json_str[json_offset];
    json_offset += sprintf(json_at_offset, "%s}}",r_sample_str);

    // Serial.write(json_str,json_offset);
    doBetterPost("/",json_str, json_offset);
    resetSampleStrings();
    samples_taken = 0;
    startTime = millis();
  }

  //Ensure a max rate of 50Hz
  long frame_duration = millis() - frame_start;
  long wait_time = 20 - frame_duration;

  if (samples_taken == 0) {
    Serial.println(frame_duration);    
  }

  if (wait_time > 0) {
    delay(wait_time);
  }
  
  
}

