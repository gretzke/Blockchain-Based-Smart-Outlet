// Display libraries
#include <U8g2lib.h>
U8G2_SSD1306_128X32_UNIVISION_F_SW_I2C u8g2(U8G2_R0, /* clock=*/ 5, /* data=*/ 4, /* reset=*/ 16);
// Wifi library
#include <ESP8266WiFi.h>

// wifi credentials
// SSID of your Wifi Router
const char* ssid = "";
// Password of your Wifi Router
const char* password = "";

/**
 * @notice prints text to the display, display consists of two lines
 * @param text text to be printed
 * @param upper if true, text will be printed to first line, if false to second line
 */
void printToDisplay(char* text, bool upper) {
  static char* upperText;
  static char* lowerText;
  if (upper) {
    upperText = text;
  } else {
    lowerText = text;
  }
  u8g2.clearBuffer();
  u8g2.setCursor(0, 12);
  u8g2.print(upperText);
  u8g2.setCursor(0, 30);
  u8g2.print(lowerText);
  u8g2.sendBuffer();
}

void printLoading() {
  static char loadingText[7] = "";
  // empty loadingText after 10 iterations
  if (strlen(loadingText) >= 7) {
    memset(loadingText, 0 , sizeof loadingText);
  }
  // append a dot to loadingText
  strcat(loadingText, ".");
  printToDisplay(loadingText, false);
  
}

void setup() {
  // setup display
  u8g2.begin();
  u8g2.enableUTF8Print();
  u8g2.setFont(u8g2_font_helvR12_tr);
  u8g2.setFontDirection(0);
  
  // setup wifi
  Serial.begin(115200);
  delay(20);
  printToDisplay("Connecting WiFi", true);
  // Connect to Wi-Fi network
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    printLoading();
    delay(250);
  }
  printToDisplay("Connected", true);
  printToDisplay("successfully", false);
}

void loop() {
}
