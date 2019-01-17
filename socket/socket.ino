#include <ESP8266WiFi.h>

// Pin config
int RELAY = 12;
int LED = 13;

int BLINK_DURATION = 250;

// wifi credentials
// SSID of your Wifi Router
const char* ssid = "JD Network";
// Password of your Wifi Router
const char* password = "5626234938797786";


void setup() {
  // initialize led pin as output, switch led off
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);
  // initialize relay pin as output, switch relay off
  pinMode(RELAY, OUTPUT);
  digitalWrite(RELAY, LOW);

  // setup wifi
  
  Serial.begin(115200);
  delay(20);
  
  // Connect to Wi-Fi network
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Successfully connected to WiFi");
}

void loop() {
  digitalWrite(LED, HIGH);
  digitalWrite(RELAY, HIGH);
  delay(BLINK_DURATION);
  digitalWrite(LED, LOW);
  digitalWrite(RELAY, LOW);
  delay(BLINK_DURATION);
}
