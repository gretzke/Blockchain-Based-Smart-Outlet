// Display libraries
#include <U8g2lib.h>
U8G2_SSD1306_128X32_UNIVISION_F_SW_I2C u8g2(U8G2_R0, /* clock=*/ 5, /* data=*/ 4, /* reset=*/ 16);

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
  u8g2.setCursor(0, 14);
  u8g2.print(upperText);
  u8g2.setCursor(0, 32);
  u8g2.print(lowerText);
  u8g2.sendBuffer();
}

void setup() {
  // setup display
  u8g2.begin();
  u8g2.enableUTF8Print();
  u8g2.setFont(u8g2_font_helvR14_tr);
  u8g2.setFontDirection(0);
}

void loop() {
  printToDisplay("Hello World!", true);
}
