// Display libraries
#include <U8g2lib.h>
U8G2_SSD1306_128X32_UNIVISION_F_SW_I2C u8g2(U8G2_R0, 5, 4, 16);

// Wifi libraries
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
HTTPClient http;

// import credentials file
#include "credentials.h"

// cryptography libraries and define ecdsa curve
#include "keccak.h"
#include <uECC.h>
#include <uECC_vli.h>
#include <types.h>
Keccak keccak256;

// utility libraries and variables
#include <ArduinoJson.h>

// wifi credentials
// SSID of your Wifi Router
const char* ssid;
// Password of your Wifi Router
const char* wifipass = credentials.wifipass;

// node and contract address, calldata and fingerprint for https requests
char nodeAddr[] = "https://rinkeby.infura.io/";
char contractAddr[] = "";
char callData[] = "";
// fingerprint in case "https://rinkeby.infura.io/" is used as a host
char fingerprint[] = "0B:FE:65:E8:C1:7A:9E:C2:8D:98:5B:BA:1A:EC:7A:4B:83:68:DE:11";

void setup() {
    // setup display
    u8g2.begin();
    u8g2.enableUTF8Print();
    u8g2.setFont(u8g2_font_helvR12_tr);
    u8g2.setFontDirection(0);
    // setup rng for ecdsa
    uECC_set_rng(&RNG);
    // setup wifi
    Serial.begin(115200);
    delay(20);
    ssid = credentials.ssid;
    wifipass = credentials.wifipass;
    printToDisplay("Connecting WiFi", true);
    // Connect to Wi-Fi network
    WiFi.begin(ssid, wifipass);
    while (WiFi.status() != WL_CONNECTED) {
        printLoading();
        delay(250);
    }
    printToDisplay("Connected", true);
    printToDisplay("successfully", false);

    //char* prefix = "\x19Ethereum Signed Message:\n";
    char* body = "{\"jsonrpc\":\"2.0\",\"method\":\"eth_syncing\",\"params\":[],\"id\":1}";
    String result = web3Call(body);
    Serial.println(result);

    char* encodedPack = encodePacked(52793, "90515798caf27f7b431d46d668be104330c4449b");
    int encodedPackLen = strlen(encodedPack);
    Serial.println(encodedPack);

    unsigned char bytearray[encodedPackLen / 2];
    hexStringToBytes(encodedPack, bytearray);

    const char* hash = calculateHash(bytearray, sizeof(bytearray));

    Serial.println(hash);

    signMessage(hash);
}

void loop() {
}

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

/**
 * @notice prints a dot to the second line, adds another dot on every call
 */
void printLoading() {
    static char loadingText[7] = "";
    // empty loadingText after 10 iterations
    if (strlen(loadingText) >= 7) {
        memset(loadingText, 0, sizeof loadingText);
    }
    // append a dot to loadingText
    strcat(loadingText, ".");
    printToDisplay(loadingText, false);
}

/**
 * @notice executes a web3 rpc call
 * @param body char array with content of request body
 *        example: "{\"jsonrpc\":\"2.0\",\"method\":\"eth_syncing\",\"params\":[],\"id\":1}"
 */
// Function to execute web3 rpc calls  char body[]
// returns string of the result
String web3Call(char body[]) {
    Serial.println(body);
    // http POST
    http.begin(nodeAddr, fingerprint);
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.POST(body);
    String payload = http.getString();
    http.end();

    if (httpCode > 0) {
        // parse JSON and return result
        StaticJsonBuffer<512> JSONbuffer;
        JsonObject& result = JSONbuffer.parseObject(payload);

        if (result.containsKey("result")) {
            return result["result"];
        } else if (result.containsKey("error")) {
            Serial.println(result["error"].as<String>());
            return "error";
        }
    } else {
        Serial.println(http.errorToString(httpCode).c_str());
        return "error";
    }
}

/**
 * @notice packs message arguments according to the solidity specification
 *         https://solidity.readthedocs.io/en/develop/abi-spec.html?highlight=encodepacked#non-standard-packed-mode
 * @param
 */
char* encodePacked(int value, char* contractAddr) {
    char encodedPack[512] = "";
    char valueHex[64] = "";
    // convert integer to 256bit hex with padded zeros
    sprintf(valueHex, "%064x", value);
    // copy hex value and contract addr to encoded pack
    strcat(encodedPack, valueHex);
    strcat(encodedPack, contractAddr);
    return encodedPack;
}

/**
 * @notice function to convert a hex char array to a byte array
 * @dev https://stackoverflow.com/questions/3408706/hexadecimal-string-to-byte-array-in-c/3409211
 * @param hexString char* of hex chars (0-9,A-F)
 * @param byteArray unsigned char* byte array output
 */
void hexStringToBytes(const char* hexString, unsigned char* byteArray) {
    size_t len = strlen(hexString);
    static const unsigned char TBL[] = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 58, 59, 60, 61,
        62, 63, 64, 10, 11, 12, 13, 14, 15, 71, 72, 73, 74, 75,
        76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89,
        90, 91, 92, 93, 94, 95, 96, 10, 11, 12, 13, 14, 15};
    static const unsigned char* LOOKUP = TBL - 48;
    const char* end = hexString + len;
    while (hexString < end) {
        *(byteArray++) = LOOKUP[*(hexString++)] << 4 | LOOKUP[*(hexString++)];
    }
}

/**
 * @notice calculates the keccak256 hash of a given bytearray
 * @param bytearray unsigned char* data to be hashed
 * @param bytesize size of byte array in bytes
 * @return const char* 32 byte hash
 */
const char* calculateHash(unsigned char* bytearray, int bytesize) {
    return keccak256.operator()(bytearray, bytesize).c_str();
}

void signMessage(const char* hashString) {
  Serial.println(credentials.privkey);  
  Serial.println(hashString);  
  uint8_t privateKey[32];
  uint8_t hash[32];
  hexStringToBytes(credentials.privkey, privateKey);
  hexStringToBytes(hashString, hash);
  uint8_t signature[64];
  const struct uECC_Curve_t * curves[1];
  curves[0] = uECC_secp256k1();
  int res = uECC_sign(privateKey, hash, 32, signature, curves[0]);
  
  Serial.printf("%i", res);
  for (int i = 0; i < 64; i++) {
    Serial.printf("%x", signature[i]);
  }
}

static int RNG(uint8_t *dest, unsigned size) {
  // Use the least-significant bits from the ADC for an unconnected pin (or connected to a source of 
  // random noise). This can take a long time to generate random data if the result of analogRead(0) 
  // doesn't change very frequently.
  while (size) {
    uint8_t val = 0;
    for (unsigned i = 0; i < 8; ++i) {
      int init = analogRead(0);
      int count = 0;
      while (analogRead(0) == init) {
        ++count;
      }
      
      if (count == 0) {
         val = (val << 1) | (init & 0x01);
      } else {
         val = (val << 1) | (count & 0x01);
      }
    }
    *dest = val;
    ++dest;
    --size;
  }
  return 1;
}
