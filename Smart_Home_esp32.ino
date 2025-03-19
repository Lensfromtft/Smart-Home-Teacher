#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>
#include <DHT.h>
#include <Adafruit_NeoPixel.h>

const char* ssid = "Smart Home";
const char* password = "smarthome";

const char* mqtt_server = "public.mqttserver.eu";
const int mqtt_port = 8883;  
const char* topic_temp   = "lentheo/temperature";
const char* topic_led1   = "lentheo/light1";
const char* topic_led2   = "lentheo/light2";
const char* topic_led3   = "lentheo/light3";
const char* topic_door   = "lentheo/door";
const char* topic_wanted = "lentheo/wanted";
const char* topic_heater = "lentheo/heater";
const char* topic_someone = "lentheo/someone";

WiFiClientSecure espClient;
PubSubClient client(espClient);
long lastMsg = 0;

//  LED 
#define LED1_PIN 4
#define LED2_PIN 5
#define LED3_PIN 6
bool ledOn = false;

#define LEDR_PIN 16
#define LEDB_PIN 17

// Fan
int motorpin1 = 8;
int motorpin2 = 18;

// servo motor
#define SERVO_PIN 7
Servo myServo;

// DHT11
#define DHTPIN 15
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Built-in
#define LED_PIN 38
#define NUMPIXELS 1
Adafruit_NeoPixel LED_RGB(NUMPIXELS, LED_PIN, NEO_GRBW + NEO_KHZ800);

// SR04
const int trigPin = 9; 
const int echoPin = 10;  
long duration; 
int distance;

float wantedTemperature = 0.0;
bool heaterOn = false;

void setup() {
  Serial.begin(115200);

  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);
  pinMode(LED3_PIN, OUTPUT);
  pinMode(LEDR_PIN, OUTPUT);
  pinMode(LEDB_PIN, OUTPUT);
  digitalWrite(LED1_PIN, LOW);
  digitalWrite(LED2_PIN, LOW);
  digitalWrite(LED3_PIN, LOW);
  digitalWrite(LEDR_PIN, LOW);
  digitalWrite(LEDB_PIN, LOW); 

  pinMode(motorpin1, OUTPUT);
  pinMode(motorpin2, OUTPUT);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);  

  myServo.attach(SERVO_PIN);
  myServo.write(0); 

  dht.begin();

  LED_RGB.begin();
  LED_RGB.show(); 

  setup_wifi();

  espClient.setInsecure();

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  reconnect();
}

void setup_wifi() {
  Serial.print("Connexion à ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connecté !");
  Serial.print("Adresse IP: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Connexion MQTT...");
    if (client.connect("ESP32_Client")) {
      Serial.println("Connecté !");
      client.subscribe(topic_led1);
      client.subscribe(topic_led2);
      client.subscribe(topic_led3);
      client.subscribe(topic_door);
      client.subscribe(topic_wanted); 
      client.subscribe(topic_heater); 
    } else {
      Serial.print("Échec, code: ");
      Serial.print(client.state());
      Serial.println(" Nouvelle tentative dans 5s");
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.print("Message reçu sur ");
  Serial.print(topic);
  Serial.print(" : ");
  Serial.println(message);

  if (String(topic) == topic_led1) {
    if (message == "ON") {
      digitalWrite(LED1_PIN, HIGH);
      Serial.println("LED 1 allumée");
    } else if (message == "OFF") {
      digitalWrite(LED1_PIN, LOW);
      Serial.println("LED 1 éteinte");
    }
  }
  else if (String(topic) == topic_led2) {
    if (message == "ON") {
      digitalWrite(LED2_PIN, HIGH);
      Serial.println("LED 2 allumée");
    } else if (message == "OFF") {
      digitalWrite(LED2_PIN, LOW);
      Serial.println("LED 2 éteinte");
    }
  }
  else if (String(topic) == topic_led3) {
    if (message == "ON") {
      digitalWrite(LED3_PIN, HIGH);
      ledOn = true;
      Serial.println("LED 3 allumée");
    } else if (message == "OFF") {
      digitalWrite(LED3_PIN, LOW);
      ledOn = false;
      Serial.println("LED 3 éteinte");
    }
  }
  else if (String(topic) == topic_door) {
    if (message == "OPEN") {
      myServo.write(90);
      Serial.println("Servo positionné à 90° (OPEN)");
    } else if (message == "CLOSE") {
      myServo.write(0);
      Serial.println("Servo positionné à 0° (CLOSE)");
    }
  }
  else if (String(topic) == topic_wanted) {
    wantedTemperature = message.toFloat();
    Serial.print("Température wanted mise à jour: ");
    Serial.println(wantedTemperature);
  }
  else if (String(topic) == topic_heater) {
    heaterOn = (message == "ON");
    Serial.print("État du chauffage mis à jour: ");
    Serial.println(heaterOn ? "ON" : "OFF");
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();


  long now = millis();
  if (now - lastMsg > 1000) {
    lastMsg = now;
  
    float temperature = dht.readTemperature();

    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);
    duration = pulseIn(echoPin, HIGH);
    distance = (duration * 0.0343) / 2;
    Serial.print("Distance : ");
    Serial.print(distance);
    Serial.println(" cm");


      if (distance <= 5) {
        client.publish(topic_someone, "YES");
        Serial.println("Someone is in front of the door");
      } 
      else {
        client.publish(topic_someone, "NO");
        Serial.println("Nobody is in front of the door");
      }


    if (isnan(temperature)) {
      Serial.println("Erreur de lecture du capteur DHT!");
    } else {
      Serial.print("Température lue: ");
      Serial.println(temperature);
      if (heaterOn) {
        if (wantedTemperature > temperature) {
          LED_RGB.setPixelColor(0, LED_RGB.Color(255, 0, 0));  
          digitalWrite(LEDR_PIN, HIGH);
          digitalWrite(LEDB_PIN, LOW);
          Serial.println("LED NeoPixel: Rouge (wanted > actual)");
          digitalWrite(motorpin1, HIGH);
          digitalWrite(motorpin2, LOW);
        } else if (wantedTemperature < temperature) {
          LED_RGB.setPixelColor(0, LED_RGB.Color(0, 0, 255));  
          digitalWrite(LEDB_PIN, HIGH);
          digitalWrite(LEDR_PIN, LOW);
          Serial.println("LED NeoPixel: Bleu (wanted < actual)");
          digitalWrite(motorpin1, LOW);
          digitalWrite(motorpin2, HIGH);
        } else {
          LED_RGB.setPixelColor(0, LED_RGB.Color(0, 0, 0));    
          digitalWrite(LEDB_PIN, LOW);
          digitalWrite(LEDR_PIN, LOW);
          Serial.println("LED NeoPixel: Éteinte (wanted == actual)");
          digitalWrite(motorpin1, LOW);
          digitalWrite(motorpin2, LOW);
        }
      }
      else {
        LED_RGB.setPixelColor(0, LED_RGB.Color(0, 0, 0));
        Serial.println("LED NeoPixel: Éteinte (chauffage OFF)");
        digitalWrite(LEDB_PIN, LOW);
        digitalWrite(LEDR_PIN, LOW);
        digitalWrite(motorpin1, LOW);
        digitalWrite(motorpin2, LOW);
      }
      LED_RGB.show();

      char tempString[8];
      dtostrf(temperature, 1, 2, tempString);
      client.publish(topic_temp, tempString);
    }
  }
}