#include <EEPROM.h>

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <WiFiClient.h>

#include <PubSubClient.h> //https://github.com/knolleary/pubsubclient/releases/tag/2.4


static const uint8_t RELAY = 12; // MTDI
static const uint8_t SWITCH = 0; // GPIO0
static const uint8_t LED = 13; // MTCK

boolean spacestate = LOW;
boolean override = LOW;
boolean lowpower = LOW;
boolean spacechange = LOW;
boolean lowpowerchange = LOW;
boolean overridechange = LOW;

// WiFi settings
char ssid[] = "revspace-pub-2.4ghz";  //  your network SSID (name)
char pass[] = "";       // your network password

// MQTT Server settings and preparations
const char* mqtt_server = "mosquitto.space.revspace.nl";
WiFiClient espClient;
PubSubClient client(mqtt_server, 1883, onMqttMessage, espClient);
long lastReconnectAttempt = 0;
String chipid;
String switchtopic;
char switchchar[100];
char chipidchar[6];
String listentopic;
String debugtopic;
char debugtopicchar[100];
uint8_t MAC_array[6];
char MAC_char[18];

void setup() {
  // put your setup code here, to run once:
  pinMode(RELAY, OUTPUT);
  pinMode(SWITCH, INPUT);
  pinMode(LED, OUTPUT);

  WiFi.mode(WIFI_STA);
  Serial.begin(115200);
  Serial.println();

  chipid = ESP.getChipId();
  chipid.toCharArray(chipidchar, sizeof chipidchar);

  switchtopic = String("revspace/switches/") + chipid;

  listentopic = String("revdebug/switches/") + chipid ;
  debugtopic = listentopic + String("/debug");
  Serial.print("Debugtopic: ");
  Serial.println(debugtopic);

  debugtopic.toCharArray(debugtopicchar, sizeof debugtopicchar);
  switchtopic.toCharArray(switchchar, sizeof switchchar);

  Serial.println();
  Serial.print("Hi, i am RevSwitch ");
  Serial.println(chipid);

  WiFi.macAddress(MAC_array);
  sprintf(MAC_char, "%02x:%02x:%02x:%02x:%02x:%02x",
          MAC_array[0], MAC_array[1], MAC_array[2], MAC_array[3], MAC_array[4], MAC_array[5]);

  // We start by connecting to a WiFi network
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED, LOW);
    delay(500);
    Serial.print(".");
    digitalWrite(LED, HIGH);

  }
  Serial.println("");

  Serial.print("WiFi connected");
  Serial.println(ssid);

  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.print("Mac Address: ");
  Serial.println(MAC_char);
}

void loop() {
  // put your main code here, to run repeatedly:
  if (!client.connected()) {
    Serial.println(".");
    long verstreken = millis();
    if (verstreken - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = verstreken;
      // Attempt to reconnect
      if (reconnect()) {
        lastReconnectAttempt = 0;
      }
    }
  } else {
    // Client connected
    client.loop();
  }

  if (spacechange == HIGH) {
    spacechange = LOW;
    if (spacestate == HIGH) {
      if (lowpower == LOW) {
        relayon();
      } else {
        relayoff();
      }
    } else {
      relayoff();
    }
  }

  if (overridechange == HIGH) {
    overridechange = LOW;
    if (override == HIGH) {
      relayon();
    } else {
      relayoff();
    }
  }

  if (lowpowerchange == HIGH) {
    lowpowerchange = LOW;
    if (lowpower == HIGH) {
      relayoff();
    } else {
      spacechange=HIGH;
    }
  }
}

void relayon() {
  digitalWrite(RELAY, HIGH);
  digitalWrite(LED, LOW);
  delay(2000);
}

void relayoff() {
  digitalWrite(RELAY, LOW);
  digitalWrite(LED, HIGH);
  delay(2000);
}


boolean reconnect() {
  if (client.connect(chipidchar)) {
    // Once connected, publish an announcement...

    mqtt_publish(listentopic, "hello world");

    client.subscribe(debugtopicchar);
    client.loop();
    // ... and resubscribe
    client.subscribe("revspace/state");
    client.loop();
    client.subscribe("revspace/state/lowpower");
    client.loop();
    client.subscribe(switchchar);
    client.loop();

  }
  return client.connected();
}


void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  uint16_t spaceCnt;
  uint8_t numCnt = 0;
  char bericht[50] = "";

  Serial.print("received topic: ");
  Serial.println(topic);
  Serial.print("length: ");
  Serial.println(length);
  Serial.print("payload: ");
  for (uint8_t pos = 0; pos < length; pos++) {
    bericht[pos] = payload[pos];
  }
  Serial.println(bericht);
  Serial.println();

  // Lets select a topic/payload handler
  // Some topics (buttons for example) don't need a specific payload handled, just a reaction to the topic. Saves a lot of time!

  // Space State
  if (strcmp(topic, "revspace/state") == 0) {
    if (strcmp(bericht, "open") == 0) {
      Serial.println("Revspace is open");
      if (spacestate == LOW) {
        spacestate = HIGH;
        spacechange = HIGH;
        override = HIGH;
      }
    } else {
      // If it ain't open, it's closed! (saves a lot of time doing strcmp).
      Serial.println("Revspace is dicht");
      if (spacestate == HIGH) {
        spacestate = LOW;
        spacechange = HIGH;
        override = LOW;
      }
    }
  }

  // Switcher
  if (strcmp(topic, switchchar) == 0) {
    if (strcmp(bericht, "on") == 0) {
      Serial.println("Switch me on");
      if (override == LOW) {
        overridechange = HIGH;
        override = HIGH;
      }
    }
    if (strcmp(bericht, "off") == 0) {
      // If it ain't open, it's closed! (saves a lot of time doing strcmp).
      Serial.println("Switch me off");
      if (override == HIGH) {
        overridechange = HIGH;
        override = LOW;

      }
    }
  }

  if (strcmp(topic, "revspace/state/lowpower") == 0) {
    if (strcmp(bericht, "on") == 0) {
      Serial.println("Low Power Mode");
      if (lowpower == LOW) {
        lowpowerchange = HIGH;
        lowpower = HIGH;
      }
    }
    if (strcmp(bericht, "off") == 0) {
      Serial.println("Normal power mode");
      if (lowpower == HIGH) {
        lowpowerchange = HIGH;
        lowpower = LOW;
      }
    }
  }

  if (strcmp(topic, debugtopicchar) == 0) {
    Serial.println("Debug!");
    mqtt_publish(debugtopic + String("/response"), WiFi.localIP().toString() + String(" ") + String(MAC_char));
  }

  Serial.println();
}

void mqtt_publish (String topic, String message) {
  Serial.println();
  Serial.print("Publishing ");
  Serial.print(message);
  Serial.print(" to ");
  Serial.println(topic);

  char t[100], m[100];
  topic.toCharArray(t, sizeof t);
  message.toCharArray(m, sizeof m);

  client.publish(t, m, /*retain*/ 1);
}


