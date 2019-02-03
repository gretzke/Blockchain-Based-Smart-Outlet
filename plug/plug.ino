// Display libraries
#include <U8g2lib.h>
U8G2_SSD1306_128X32_UNIVISION_F_SW_I2C u8g2(U8G2_R0, 5, 4, 16);

// Wifi libraries
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
HTTPClient http;

// import credentials file
#include "src/credentials.h"

// cryptography libraries
#include <types.h>
#include <uECC.h>
#include <uECC_vli.h>
#include "src/keccak256.h"

// EIP 155
#define CHAIN_ID 4

const char* privateKey;
const char* ethAddr;

// utility libraries and variables
#include <ArduinoJson.h>
#include "src/Util.h"
Util util;

// wifi credentials
// SSID of your Wifi Router
const char* ssid;
// Password of your Wifi Router
const char* wifipass;

// node and contract address, calldata and fingerprint for https requests
char* nodeAddr = "";
char* contractAddr = "";
char* callData = "";
// fingerprint in case "https://rinkeby.infura.io/" is used as a host
char fingerprint[] = "0B:FE:65:E8:C1:7A:9E:C2:8D:98:5B:BA:1A:EC:7A:4B:83:68:DE:11";

void setup() {
  // setup display
  u8g2.begin();
  u8g2.enableUTF8Print();
  u8g2.setFont(u8g2_font_helvR12_tr);
  u8g2.setFontDirection(0);

  // setup rng for ecdsa signatures
  uECC_set_rng(&RNG);

  // setup wifi
  Serial.begin(115200);
  delay(20);

  // load variables from credentials file
  ssid = credentials.ssid;
  wifipass = credentials.wifipass;
  privateKey = credentials.privkey;
  ethAddr = credentials.address;
  nodeAddr = credentials.nodeAddr;
  contractAddr = credentials.contractAddress;

  printToDisplay("Connecting WiFi", true);
  // Connect to Wi-Fi network
  WiFi.begin(ssid, wifipass);
  while (WiFi.status() != WL_CONNECTED) {
    printLoading();
    delay(250);
  }
  printToDisplay("Connected", true);
  printToDisplay("successfully", false);

  signTransaction();
  //signMessage();
}

void loop() {
}

void signTransaction() {
  uint32_t nonce = getTransactionCount();
  uint32_t gasPrice = 1000000000;  //getGasPrice();
  uint32_t gasLimit = 45000;
  const char* to = contractAddr;
  const char* data = "0xd0e30db0";
  const char* value = "0x2386f26fc10000";
  uint8_t signedTransaction[65] = {0};

  Serial.printf("\n%u\n", nonce);

  // encode transaction parameters according to https://github.com/ethereum/wiki/wiki/RLP
  const char* encoded = util.RlpEncode(nonce, gasPrice, gasLimit, to, value, data, CHAIN_ID).c_str();
  Serial.println(encoded);
  // -2 to because encoded has 0x prefix
  int encodedLen = strlen(encoded) - 2;

  // convert encoded values to bytes
  uint8_t bytearray[encodedLen / 2];
  hexStringToBytes(encoded, bytearray);

  // hash encoded data
  uint8_t dataHash[32] = {0};
  calculateHash(bytearray, sizeof(bytearray), dataHash);

  // sign transaction
  signHash(dataHash, signedTransaction, true);
  printByteArray(signedTransaction, sizeof(signedTransaction));

  const char* encodedTransaction = util.RlpEncodeForRawTransaction(nonce, gasPrice, gasLimit, contractAddr, value, data, signedTransaction, signedTransaction[64]).c_str();
  Serial.println(encodedTransaction);
  char methodName[] = "eth_sendRawTransaction";
  char params[256] = "";
  sprintf(&params[0], "\"%s\"", encodedTransaction);
  String transactionHash = web3Call(methodName, params);
  Serial.println(transactionHash);
}

uint32_t getTransactionCount() {
  int returnValue = -1;
  char methodName[] = "eth_getTransactionCount";
  char params[256] = "";
  sprintf(&params[0], "\"%s\",\"latest\"", ethAddr);
  String result = web3Call(methodName, params);
  if (result == "error") {
    // TODO: Error handling
    return 0;
  }
  // convert hex string to int
  return strtol(result.c_str(), NULL, 16);;
}

void printByteArray(uint8_t* byteArray, int size) {
  for (int i = 0; i < size; i++) {
    Serial.printf("%02x", byteArray[i]);
  }
  Serial.println();
}

void signMessage() {
  // encode values
  char* encodedPack = encodePacked(52793, "90515798caf27f7b431d46d668be104330c4449b");
  int encodedPackLen = strlen(encodedPack);
  // convert encoded values to bytes
  uint8_t bytearray[encodedPackLen / 2];
  hexStringToBytes(encodedPack, bytearray);
  // sign bytes
  uint8_t dataHash[32] = {0};
  calculateHash(bytearray, sizeof(bytearray), dataHash);
  // prefix hash with ethereum specific prefix
  uint8_t prefixedHash[32] = {0};
  prefixMessage(dataHash, prefixedHash);
  // sign message
  uint8_t signedMessage[65] = {0};
  signHash(prefixedHash, signedMessage, false);
}

/**
   UTILITY FUNCTIONS
*/

/**
   @notice function to convert a hex char array to a byte array
   @dev https://stackoverflow.com/questions/3408706/hexadecimal-string-to-byte-array-in-c/3409211
   @param hexString char* of hex chars (0-9,A-F)
   @param byteArray uint8_t* byte array output
*/
void hexStringToBytes(const char* hexString, uint8_t* byteArray) {
  size_t len = strlen(hexString);
  if (hexString[0] == '0' && hexString[1] == 'x') {
    len -= 2;
    hexString += 2;
  }
  static const uint8_t TBL[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 58, 59, 60, 61,
    62, 63, 64, 10, 11, 12, 13, 14, 15, 71, 72, 73, 74, 75,
    76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89,
    90, 91, 92, 93, 94, 95, 96, 10, 11, 12, 13, 14, 15
  };
  static const uint8_t* LOOKUP = TBL - 48;
  const char* end = hexString + len;
  while (hexString < end) {
    *(byteArray++) = LOOKUP[*(hexString++)] << 4 | LOOKUP[*(hexString++)];
  }
}

/**

*/

void bytesToHexString(uint8_t* bytes, int byteSize, char* hexString) {
  hexString[0] = '0';
  hexString[1] = 'x';
  for (int i = 0; i < byteSize; i++) {
    sprintf(&hexString[i * 2 + 2], "%02x", &bytes[i]);
  }
  hexString[byteSize] = 0;
}

// from library examples: https://github.com/kmackay/micro-ecc/blob/master/examples/ecc_test/ecc_test.ino
static int RNG(uint8_t* dest, unsigned size) {
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

/**
   DISPLAY FUNCTIONS
*/

/**
  @notice prints text to the display, display consists of two lines
  @param text text to be printed
  @param upper if true, text will be printed to first line, if false to second line
*/
void printToDisplay(char* text, bool upper) {
  static char* upperText;
  static char* lowerText;
  if (upper) {
    upperText = text;
  } else {
    lowerText = text;
  }
  delay(20);
  u8g2.clearBuffer();
  u8g2.setCursor(0, 12);
  u8g2.print(upperText);
  u8g2.setCursor(0, 30);
  u8g2.print(lowerText);
  u8g2.sendBuffer();
}

/**
   @notice prints a dot to the second line, adds another dot on every call
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
   @notice executes a web3 rpc call
   @param body char array with content of request body
          example: "{\"jsonrpc\":\"2.0\",\"method\":\"eth_syncing\",\"params\":[],\"id\":1}"
   @return result on success, "error" on error
*/
String web3Call(char methodName[], char params[]) {
  char body[512];
  sprintf(&body[0], "{\"jsonrpc\":\"2.0\",\"method\":\"%s\",\"params\":[%s],\"id\":1}", methodName, params);
  Serial.println(body);
  // http POST
  // if you are using https://rinkeby.infura.io/ as node uncomment next line
  //http.begin(nodeAddr, fingerprint);
  http.begin(nodeAddr);
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
   @notice packs message arguments according to the solidity specification
           https://solidity.readthedocs.io/en/develop/abi-spec.html?highlight=encodepacked#non-standard-packed-mode
   @param
*/
char* encodePacked(int value, char* contractAddr) {
  char encodedPack[512] = "";
  char valueHex[64] = "";
  // convert integer to 256bit hex with padded zeros
  sprintf(valueHex, "%064x", value);
  // copy hex value and contract addr to encoded pack
  strcat(encodedPack, valueHex);
  //strcat(encodedPack, contractAddr);
  return encodedPack;
}

/**
   @notice calculates the keccak256 hash of a given bytearray
   @param bytearray uint8_t* data to be hashed
   @param bytesize size of byte array in bytes
   @return uint8_t* 32 byte hash
*/
void calculateHash(uint8_t* bytearray, int bytesize, uint8_t* hash) {
  SHA3_CTX context;
  keccak_init(&context);
  keccak_update(&context, (const unsigned char*)bytearray, (size_t)bytesize);
  keccak_final(&context, (unsigned char*)hash);
}

/**

*/
void prefixMessage(uint8_t* hash, uint8_t* prefixedMessage) {
  uint8_t prefixedHash[60] = {0};
  uint8_t messagePrefix[28] = {
    0x19, 'E', 't', 'h', 'e', 'r', 'e', 'u', 'm', ' ', 'S', 'i', 'g', 'n', 'e', 'd', ' ', 'M', 'e', 's', 's', 'a', 'g', 'e', ':', '\n', '3', '2'
  };

  // copy the prefix
  memcpy(&prefixedHash[0], &messagePrefix, 28);
  // copy the hash
  memcpy(&prefixedHash[28], hash, 32);

  // hash again to get message
  calculateHash(prefixedHash, 60, prefixedMessage);
}

/**

*/
void signHash(uint8_t* message, uint8_t* signedMessage, bool isTransaction) {
  uint8_t recid = 0;
  uint8_t privateKeyBytes[32] = {0};
  hexStringToBytes(privateKey, privateKeyBytes);
  if (!uECC_sign_with_recid(privateKeyBytes, message, 32, signedMessage, uECC_secp256k1(), &recid)) {
    // TODO: Error handling
  }
  if (isTransaction) {
    signedMessage[64] = 35 + CHAIN_ID * 2 + recid;
  } else {
    signedMessage[64] = 27 + recid;
  }
}
