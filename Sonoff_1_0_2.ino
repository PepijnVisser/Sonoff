
/*
 Sketch for Sonoff WIFI switch with OTA Update - click Tools -> Ports -> look for your node with hostname you gave it
  MQTT commands sent to topic this switch subscribes to:
  - on 0 switches off relay and led
  - on 1 switches on relay and led
  - on 2 toggles the state of the relay
  Switch sends status of relay to MQTT topic every minute

 It will reconnect to the server if the connection is lost using a blocking

 The bootup state for Relay and onboard led is ON

*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Bounce2.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

const char* ssid = "visnetje"; //SSID WIFI Access Point
const char* password = "vissershut"; //Access Point Password
const char* mqtt_server = "192.168.2.19"; //IP address MQTT server

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

const char* $State = "SonoffSwitch1Out/$State"; // MQTT Topic this switch publishes state to
const char* $Signal = "SonoffSwitch1Out/$Signal"; // MQTT Topic this switch publishes signal strength to
const char* $IP = "SonoffSwitch1Out/$IP"; // MQTT Topic this switch publishes signal strength to
const char* inTopic = "SonoffSwitch1In"; // MQTT Topic this switch subscribes ("listens" to

int relay_pin = 12; //Pin for Relay
int button_pin = 0; //Pin for button
bool relayState = HIGH;

// Instantiate a Bounce object - used for built in button to toggle relay on/off:
Bounce debouncer = Bounce(); 

unsigned long previousMillis = 0;  // will store last time STATUS was send to MQTT
const long interval = 60000; //elke minuut wordt status doorgegeven

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    extButton();
    for(int i = 0; i<500; i++){
      extButton();
      delay(1);
    }
    Serial.print(".");
  }
  digitalWrite(13, LOW);
  delay(500);
  digitalWrite(13, HIGH);
  delay(500);
  digitalWrite(13, LOW);
  delay(500);
  digitalWrite(13, HIGH);
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the RELAY if an "1" was received as first character or OFF if first character was a "0" or toggle relay when received a "2"
  if ((char)payload[0] == '0') {
    digitalWrite(13, HIGH); // Turn LED OFF
    digitalWrite(relay_pin, LOW);   // Turn the RELAY OFF
    client.publish($State, "I am OFF");
    Serial.println("relay_pin -> LOW");
    relayState = LOW;
  } else if ((char)payload[0] == '1') {
    digitalWrite(13, LOW); // Turn LED OFF
    digitalWrite(relay_pin, HIGH);  // Turn the RELAY ON by making the voltage HIGH
    client.publish($State, "I am ON");
    Serial.println("relay_pin -> HIGH");
    relayState = HIGH;
  } else if ((char)payload[0] == '2') {
    relayState = !relayState; // set relaystate the opposite to what it was
    digitalWrite(relay_pin, relayState);  // Set Relay to state set in previous line
    Serial.print("relay_pin -> switched to ");
    Serial.println(relayState); 
      // send OFF or ON and switch LED OFF or ON, depending on toggle above
      if (relayState == 1){
        client.publish($State, "I am ON");
        digitalWrite(13, LOW);
      }
     else if (relayState == 0){
        client.publish($State, "I am OFF");
        digitalWrite(13, HIGH);
      }
  } else if ((char)payload[0] == '3') {
      //Send WIFI Signal Strength
      long rssi = WiFi.RSSI();
      char sig[50];
      sprintf(sig, "%d.%02d", (int)rssi, (int)(rssi*100)%100);
      client.publish($Signal, sig);
      //Send IP address of node
      char buf[16];
      sprintf(buf, "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3] );
      client.publish($IP, buf);
      // send status
      if (relayState == 1){
        client.publish($State, "I am still ON");
        digitalWrite(13, LOW);
      }
     else if (relayState == 0){
        client.publish($State, "I am still OFF");
        digitalWrite(13, HIGH);
      }
    }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266Client")) {
      Serial.println("connected");
      // Once connected, publish an announcement to $State...
      client.publish($State, "I am ON");
      //Send WIFI Signal Strength
      long rssi = WiFi.RSSI();
      char sig[50];
      sprintf(sig, "%d.%02d", (int)rssi, (int)(rssi*100)%100);
      client.publish($Signal, sig);
      
      //Send IP address of node
      char buf[16];
      sprintf(buf, "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3] );
      client.publish($IP, buf);
     
      //Subscribe to incoming commands
      client.subscribe(inTopic);
      
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      for(int i = 0; i<5000; i++){
        extButton();
        delay(1);
      }
    }
  }
}

void extButton() {
  debouncer.update();
   // Call code if Bounce fell / onboard button pushed (Toggle from HIGH to LOW) :
   if ( debouncer.fell() ) {
     Serial.println("Debouncer fell");
     // Toggle relay state :
     relayState = !relayState;
     digitalWrite(relay_pin,relayState);
     if (relayState == 1){
      client.publish($State, "I am ON");
      digitalWrite(13, LOW);
     }
     else if (relayState == 0){
      client.publish($State, "I am OFF");
      digitalWrite(13, HIGH);
     }
   }
}

void updatecall()  {
    unsigned long currentMillis = millis();
      if (currentMillis - previousMillis >= interval) {
        // save the last time status was send
        previousMillis = currentMillis;
          if (relayState == 1){
            client.publish($State, "I am ON");
          }
          else if (relayState == 0){
            client.publish($State, "I am OFF");
          }
       }
}
void setup() {
  pinMode(relay_pin, OUTPUT);     // Initialize the relay pin as an output
  pinMode(button_pin, INPUT);     // Initialize the relay pin as an input
  pinMode(13, OUTPUT);            // Initialize the onboard LED as output

  debouncer.attach(button_pin);   // Use the bounce2 library to debounce the built in button
  debouncer.interval(50);         // Input must be low for 50 ms
  
  digitalWrite(13, LOW);          // Blink to indicate setup
  delay(500);
  digitalWrite(13, HIGH);
  delay(500);
  
  Serial.begin(115200);
  setup_wifi();                   // Connect to wifi 
  client.setServer(mqtt_server, 1883); //connect to MQTT
  client.setCallback(callback);

  //set relay power and LED on at startup
  digitalWrite(relay_pin,HIGH); 
  Serial.println("relay_pin -> HIGH");
  digitalWrite(13, LOW);

  // Port defaults to 8266: ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID] - change to your own hostname with ArduinoOTA.setHostname("YOURNODEHOSTNAME");
  ArduinoOTA.setHostname("SonoffSwitch1OTA");
  // No authentication by default - set your password with ArduinoOTA.setPassword((const char *)"YOURPASSWORD");
  ArduinoOTA.setPassword((const char *)"123456");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

}

void loop() {
  ArduinoOTA.handle();
  
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  extButton();
  updatecall();
}


