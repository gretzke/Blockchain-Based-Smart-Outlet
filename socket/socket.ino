// Wifi libraries
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
HTTPClient http;

// Pin config
int RELAY = 12;
int LED = 13;

// import credentials file
#include "src/credentials.h"

// cryptography libraries
#include <types.h>
#include <uECC.h>
#include <uECC_vli.h>
#include "src/keccak256.h"
// EIP 155: https://github.com/ethereum/EIPs/blob/master/EIPS/eip-155.md
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
  // initialize led pin as output, switch led off
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);
  // initialize relay pin as output, switch relay off
  pinMode(RELAY, OUTPUT);
  digitalWrite(RELAY, LOW);

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

  // Connect to Wi-Fi network
  WiFi.begin(ssid, wifipass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Successfully connected to WiFi");

  signTransaction();
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
  signHash(dataHash, signedTransaction);
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
void signHash(uint8_t* message, uint8_t* signedMessage) {
  uint8_t recid = 0;
  uint8_t privateKeyBytes[32] = {0};
  hexStringToBytes(privateKey, privateKeyBytes);
  if (!uECC_sign_with_recid(privateKeyBytes, message, 32, signedMessage, uECC_secp256k1(), &recid)) {
    // TODO: Error handling
  }
  signedMessage[64] = 35 + CHAIN_ID * 2 + recid;
}
