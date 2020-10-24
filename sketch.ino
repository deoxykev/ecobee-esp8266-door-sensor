#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h> 


const char *ssid = "changeme";
const char *password = "changeme";
const char fingerprint[] PROGMEM = "E4 17 67 E0 5E 79 6B 35 46 EE F7 52 99 87 AC 8B B7 37 D5 1F"; // api.ecobee.com sha1 fingerprint on oct 2020
const char *host = "api.ecobee.com";
const int port = 443;

String authToken    = "changeme";
String refreshToken = "changeme";
String clientID     = "changeme";
int reedPin = 13;
int reedState = 0;
int prevReedState = 0;

void setup()
{
  Serial.begin(9600);
  Serial.println();

  WiFi.begin(ssid, password);

  Serial.print("[+] Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  Serial.print("[+] Connected, IP address: ");
  Serial.println(WiFi.localIP());

  Serial.println("[+] Initializing pin 13 (D7)");
  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
  pinMode(reedPin, INPUT_PULLUP); // activate internal pullup register to mitigate "floating state"
  
  Serial.println("[+] Initializing EEPROM");
  EEPROM.begin(64); // allocate 64 bytes to save api keys in
  String savedAuthToken = readAuthToken();
  Serial.println("[+] Using saved AuthToken: " + savedAuthToken);
   String savedRefreshToken = readRefreshToken();
   Serial.println("[+] Using saved RefreshToken " +  savedRefreshToken);

  Serial.println("[+] Initialization all done!");
}

void loop() {
  reedState = digitalRead(reedPin);
  
  if (reedState != prevReedState) {
      if (reedState == 0) {
        Serial.println("[+] STATE CHANGE: Switch / Door Closed");
        blinkLights(2);
        int return_code = setEcobeeMode("cool");
        if (return_code == 14){
            Serial.println("[-] API Auth Token Expired");
            getNewToken();
            setEcobeeMode("cool");
        }
      } else {
        Serial.println("[+] STATE CHANGE: Switch / Door Open");
        blinkLights(4);  
        int return_code = setEcobeeMode("off");
        if (return_code == 14){
            Serial.println("[-] API Auth Token Expired");
            getNewToken();
            setEcobeeMode("off");
        }
      }
      //delay(10000); // debounce 10 seconds
  }
  
  prevReedState = reedState;
  
  //setEcobeeMode("off");
  delay(1000);                      // Wait for two seconds (to demonstrate the active low LED)
  
  }


//mode = off, auto, cool, heat
//iter = 0 (recursion counter)
int setEcobeeMode(String mode) {
  Serial.print("[+] Setting ecoBee to ");
  Serial.println(mode);
    
    BearSSL::WiFiClientSecure *client = new BearSSL::WiFiClientSecure();

    Serial.printf("[+] Using SHA1 fingerprint '%s'\n", fingerprint);
    client->setFingerprint(fingerprint);
  

    Serial.printf("[+] Connecting using SSL: %s", host);
    int r=0; //retry counter
    while((!client->connect(host, port)) && (r < 30)){
      delay(100);
      Serial.print(".");
      r++;
    }
  
    if(r==30) {
      Serial.print("\n[-] SSL Connection failed\n");
    } else {
      Serial.println(" => OK!");

    HTTPClient http;
    String savedAuthToken = readAuthToken();
    http.begin(dynamic_cast<WiFiClient&>(*client), "https://api.ecobee.com/1/thermostat?format=json");
    http.addHeader("User-Agent", "ESP2688 ecoBee Door Bot");
    http.addHeader("Accept", "application/json");
    http.addHeader("Connection", "close");
    http.addHeader("Authorization", "Bearer " + savedAuthToken );
    http.addHeader("Content-Type", "text/json");
    Serial.println("[+] Sending payload...");
    String postPayload = "{\"selection\":{\"selectionType\":\"registered\",\"selectionMatch\":\"\"},\"thermostat\":{\"settings\":{\"hvacMode\":\""  + mode +  "\"}}}"  + "\r\n";
    int httpCode = http.POST(postPayload);
    
    String payload = http.getString();
    Serial.print(payload);
    const size_t capacity = 256;
    DynamicJsonDocument doc(capacity);
    deserializeJson(doc, payload);
    int status_code = doc["status"]["code"];
    return status_code;
    }

    Serial.println("[-] Unknown failure");
    return 1;
}

void getNewToken() {
    blinkLights(8);
    BearSSL::WiFiClientSecure *client = new BearSSL::WiFiClientSecure();

    Serial.printf("[+] Using SHA1 fingerprint '%s'\n", fingerprint);
    client->setFingerprint(fingerprint);
  

    Serial.printf("[+] Connecting using SSL: %s", host);
    int r=0; //retry counter
    while((!client->connect(host, port)) && (r < 30)){
      delay(100);
      Serial.print(".");
      r++;
    }
  
    if(r==30) {
      Serial.print("\n[-] SSL Connection failed\n");
    } else {
      Serial.println(" => OK!");
            Serial.print("[-] Requesting a new Auth token");
            String savedRefreshToken = readRefreshToken();
            HTTPClient http;
                
            http.begin(dynamic_cast<WiFiClient&>(*client), "https://api.ecobee.com/token?grant_type=refresh_token&code=" + savedRefreshToken + "&client_id=" + clientID);
            int httpCode = http.POST("");
            String payload = http.getString();
            Serial.print(payload);
            const size_t capacity = 256;
            DynamicJsonDocument doc(capacity);
            deserializeJson(doc, payload);
            deserializeJson(doc, payload);
            String authToken = doc["access_token"];
            String refreshToken = doc["refresh_token"];
            saveAuthToken(authToken);
            saveRefreshToken(refreshToken);
            
            String savedAuthToken = readAuthToken();
            Serial.println("[+] Using new saved AuthToken: " + savedAuthToken);
            savedRefreshToken = readRefreshToken();
            Serial.println("[+] Using new saved RefreshToken: " +  savedRefreshToken);
            http.end();
    }

}

void blinkLights(int n) {
  for (int i=0; i<=n; i++) {
    digitalWrite(LED_BUILTIN, i % 2);
    delay(100);
  }
  digitalWrite(LED_BUILTIN, HIGH);
}



void saveAuthToken(String authToken) {
  Serial.print("[+] Saving authToken to EEPROM: ");
  Serial.print(authToken);
  for (int i=0; i < 32; i++) {
    EEPROM.write(i, authToken[i]); 
  }
  EEPROM.commit();
  Serial.print(" => OK\n");
}

void saveRefreshToken(String refreshToken) {
  Serial.print("[+] Saving refreshToken to EEPROM: ");
  Serial.print(refreshToken);
  for (int i=32; i < 64; i++) {
    EEPROM.write(i, refreshToken[i - 32]); 
  }
  EEPROM.commit();
  Serial.print(" => OK\n");
}

String readAuthToken() {
  char authKey[33];
  for (int i=0; i < 32; i++) {
    authKey[i] = EEPROM.read(i);
  }
  authKey[32] = '\0';
  return String(authKey);
}

String readRefreshToken() {
  char refreshKey[33];
  for (int i=32; i < 64; i++) {
    refreshKey[i - 32] = EEPROM.read(i);
  }
  refreshKey[32] = '\0';
  return String(refreshKey);
}
