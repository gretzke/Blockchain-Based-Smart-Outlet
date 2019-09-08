// Wifi libraries
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
HTTPClient http;
// Websocket library
#include <WebSocketsServer.h>
WebSocketsServer webSocket = WebSocketsServer(1337);

// Pin config
int RELAY = 12;
int LED = 13;
bool LEDon = false;

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
// arduinojson needs to use long long otherwise it won't be able to parse some JSON data
#define ARDUINOJSON_USE_LONG_LONG 1
#include <ArduinoJson.h>
#include "src/Util.h"
Util util;
#include <inttypes.h>

// wifi credentials
// SSID of your Wifi Router
const char* ssid;
// Password of your Wifi Router
const char* wifipass;

// node and contract address, calldata and fingerprint for https requests
char* nodeAddr = "";
char* contractAddr = "";
char* callData = "";
// when making get/post requests to https urls the fingerprint of that url is required
// fingerprint in case "https://rinkeby.infura.io/" is used as a node provider
char fingerprint[] = "0B:FE:65:E8:C1:7A:9E:C2:8D:98:5B:BA:1A:EC:7A:4B:83:68:DE:11";

// half of the numeric value of the curve n (secp256k1) used for the signature process
uint8_t halfCurveN[32] = {0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x5D, 0x57, 0x6E, 0x73, 0x57, 0xA4, 0x50, 0x1D, 0xDF, 0xE9, 0x2F, 0x46, 0x68, 0x1B, 0x20, 0xA0};

// status variable for the different states
enum Status {disconnected, connected_P, connected_S, initialized_P, initialized_S, active_P, active_S, closed} paymentStatus;

// define IP address and port of device
const char* IPaddr = "192.168.0.128";
const char* port = "1337";

// set seconds per payment here
int secondsPerPayment = 15;

// variable to track the current client
int clientID = -1;

// varaibles to save the customer address, price per second, max value and nonce for current customer from smart contract
char customerAddr[43];
uint64_t pricePerSecond = 0;
uint64_t maxValue = 0;
uint64_t nonceSC = 0;

// during the payment process these variables are used to keep track of
// the latest signature and the number of transactions and the latest value
char lastSignature[131];
char lastSignatureTemp[131];
uint64_t transactionValue = 0;
uint64_t transactionCounter = 0;

// during payment process these variables are used to keep track of the time passed and the status of the relay
unsigned long timestamp = 0;
bool relayStatus = false;

// smart contract function IDs
char* priceID = "0x3804fd1f";
char* ownerID = "0x8da5cb5b";
char* nonceID = "0x367c832a";
char* initID = "0x2eb41a8b";
char* activeID = "0x3df969ea";
char* maxValueID = "0x94a5c2e4";
char* verifyID = "0x72c53d9b";
char* closeID = "0x0dd703d9";

void setup() {
  // initialize led pin as output, switch led off
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);
  // initialize relay pin as output, switch relay off
  pinMode(RELAY, OUTPUT);
  digitalWrite(RELAY, LOW);

  // setup rng function for ecdsa signatures
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

  // setup static IP
  IPAddress ip(192, 168, 0, 128);
  IPAddress gateway(192, 168, 0, 1); // set gateway to match your network
  IPAddress subnet(255, 255, 255, 0); // set subnet mask to match your network
  WiFi.config(ip, gateway, subnet);
  WiFi.hostname("bc-smart-socket");
  // next 2 lines might be required to prevent disconnects
  WiFi.mode(WIFI_AP);
  wifi_set_sleep_type(NONE_SLEEP_T);
  // Connect to Wi-Fi network
  WiFi.begin(ssid, wifipass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
    if (LEDon) {
      digitalWrite(LED, LOW);
      LEDon = false;
    } else {
      digitalWrite(LED, HIGH);
      LEDon = true;
    }
  }
  Serial.println("Successfully connected to WiFi");
  digitalWrite(LED, HIGH);

  // set current state to disconnected
  paymentStatus = disconnected;

  // start websocket server
  webSocket.begin();
  // set event handler function
  webSocket.onEvent(webSocketEvent);
}

void loop() {
  // the ESP8266 needs to run utility functions in the background
  // if these functions are blocked from running the ESP8266 panics and crashes
  // calling the yield() function allows the ESP8266 to run these utility functions
  yield();
  // calling the websocket event handler to fetch new messages, etc
  webSocket.loop();

  // if relay is active, check if time has run out and shut off relay if necessary
  if (relayStatus) {
    if (timestamp < millis()) {
      Serial.println("\nTurning relay off\n");
      digitalWrite(RELAY, LOW);
      relayStatus = false;
    }
  }

  // the rest of the code inside the loop function only runs when a plug is connected
  if (clientID != -1) {
    // execute code according to current payment status
    // connection state code
    if (paymentStatus == connected_S) {
      // send payment information to plug (owner and smart contract address, seconds of electricity transfer per payment)
      Serial.println("Sending payment information to plug");
      char messageBuffer[512] = "";
      sprintf(messageBuffer, "{\"id\": %d, \"contract\": \"%s\", \"owner\": \"%s\", \"secondsPerPayment\": %d}", connected_S, contractAddr, ethAddr, secondsPerPayment);
      webSocket.sendTXT(clientID, messageBuffer);

      // update connection state, await customer information from socket
      Serial.println("Awaiting payment channel initialization from plug\n");
      paymentStatus = initialized_P;

      // initialization code
    } else if (paymentStatus == initialized_S) {
      Serial.println("Verifying contract");
      // check if smart contract was initialized
      String activeHex = readFromSmartContract(activeID);
      // convert hex string to uint
      int success = strtoul(activeHex.c_str(), 0, 16);
      if (success) {
        Serial.println("Initialization was verified\nAwaiting payment message from plug\n");
        // update state
        paymentStatus = active_P;
        webSocket.sendTXT(clientID, "{\"id\": 4}");
      } else {
        Serial.println("Initialization was invalid");
        disconnectFromPlug(true);
      }

      // payment code
    } else if (paymentStatus == active_S) {
      // verify the latest received payment
      Serial.println("Verifying payment");
      // encode signature into call data to verify payment with the smart contract
      char* encodedData = (char*)malloc(512 * sizeof(char));
      encodeData(encodedData, verifyID, transactionValue, lastSignatureTemp);
      // call smart contract
      String result = readFromSmartContract(encodedData);
      free(encodedData);
      // if transaction is valid returns 0x1, if invalid returns 0x0
      int success = strtoul(result.c_str(), 0, 16);
      if (success == 1) {
        Serial.println("Payment valid");
        // save last valid signature
        strcpy(lastSignature, lastSignatureTemp);
        // if relay is switched off, init timer and activate relay, else add seconds to timer
        if (!relayStatus) {
          timestamp = millis() + secondsPerPayment * 1000;
          Serial.println("Switching Relay on");
          digitalWrite(RELAY, HIGH);
          relayStatus = true;
        } else {
          timestamp += secondsPerPayment * 1000;
        }
        // close payment channel after 5 transactions for demonstration purposes
        if (transactionCounter > 5) {
          // update state to run close code
          paymentStatus = closed;
        } else {
          Serial.println("Awaiting next payment\n");
          paymentStatus = active_P;
        }
      } else {
        paymentStatus = closed;
      }
    }

    // settlement code
    else if (paymentStatus == closed) {
      // if no signature present, disconnect
      Serial.println("Closing payment channel");
      if (strlen(lastSignature) == 0) {
        disconnectFromPlug(true);
      } else {
        Serial.println("Submitting signature");
        // call close channel function
        String transactionHash = closeChannel();
        Serial.println(transactionHash);
        webSocket.sendTXT(clientID, "{\"id\": 7}");
        // update state to disconnected
        paymentStatus = disconnected;
        clientID = -1;
        digitalWrite(LED, HIGH);
      }
      transactionCounter = 0;
      // reset last signature
      lastSignature[0] = '\0';
    }
  }
}

/**
  @notice disconnect from plug
  @param notifyPlug if true, notify plug of disconnect before disconnecting
*/
void disconnectFromPlug(bool notifyPlug) {
  paymentStatus = disconnected;
  transactionCounter = 0;
  if (notifyPlug) {
    webSocket.sendTXT(clientID, "{\"id\": 0}");
  }
  Serial.println("Disconnecting from plug");
  clientID = -1;
  digitalWrite(LED, HIGH);
}

/**
    Websocket Callback function
    https://github.com/Links2004/arduinoWebSockets/
*/
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {

  switch (type) {
    // gets called when a client disconnects
    case WStype_DISCONNECTED:
      if (clientID == num) {
        Serial.printf("Plug [%u] disconnected!\n", num);
        // close channel if signature is present
        if (strlen(lastSignature) == 0) {
          disconnectFromPlug(false);
        } else {
          paymentStatus = closed;
        }
      }
      break;

    // gets called if a new client connects
    case WStype_CONNECTED:
      // only allow one client connection at the time
      if (clientID == -1) {
        clientID = num;
        IPAddress ip = webSocket.remoteIP(clientID);
        Serial.printf("New connection from plug [%u]. Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
        paymentStatus = connected_P;
        digitalWrite(LED, LOW);
      }
      break;

    // gets called when websocket server receives a text message from client
    case WStype_TEXT:
      if (clientID == num) {
        Serial.printf("new message from plug [%u]: %s\n", num, payload);

        // parse JSON to doc
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, payload);

        // If parsing does not succeed, return
        if (error) {
          Serial.print(F("deserializeJson() failed with code "));
          Serial.println(error.c_str());
          return;
        }

        // messages from plug will be identified and matched by "id"
        if (doc.containsKey("id")) {

          // received payment information from plug, ready to initiate payment process
          if (doc["id"] == (int)connected_P) {
            Serial.println("Received customer information from plug: ");
            // save customer address
            strcpy(customerAddr, doc["address"].as<char*>());
            Serial.printf("Customer Address: %s\n\n", customerAddr);
            // update payment status
            paymentStatus = connected_S;

            // plug initiated payment channel
          } else if (doc["id"] == (int)initialized_P) {
            Serial.println("Received payment channel initialization notification from plug: ");
            pricePerSecond = doc["price"].as<uint64_t>();
            nonceSC = doc["nonce"].as<uint64_t>();
            maxValue = doc["maxValue"].as<uint64_t>();
            Serial.printf("Price: %llu\n", pricePerSecond);
            Serial.printf("Smart Contract Nonce: %llu\n", nonceSC);
            Serial.printf("Max Value: %llu\n\n", maxValue);
            // update payment status to verify smart contract data
            paymentStatus = initialized_S;

            // plug sent a payment
          } else if (doc["id"] == (int)active_P) {
            Serial.println("Received payment from plug");
            transactionCounter += 1;
            transactionValue = transactionCounter * pricePerSecond * secondsPerPayment;
            strcpy(lastSignatureTemp, doc["signature"].as<char*>());
            Serial.printf("Signature: %s\n", lastSignatureTemp);
            Serial.printf("Value: %llu\n", transactionValue);
            paymentStatus = active_S;

            // plug sent channel closing request
          } else if (doc["id"] == (int)closed) {
            paymentStatus = closed;

            // plug disconnects
          } else if (doc["id"] == (int)disconnected) {
            disconnectFromPlug(false);

          } else {
            Serial.printf("\n---\nNew message that couldn't be assigned by id:\n%s\n---\n", payload);
          }
        } else {
          Serial.printf("\n---\nNew message that couldn't be identified:\n%s\n---\n", payload);
        }
      }
      break;
  }

}


/**
   ETHEREUM FUNCTIONS
*/

/**
   @notice submits the latest signature to the smart contract and closes the payment channel
   @return transaction hash
*/
String closeChannel() {
  uint32_t nonce = getTransactionCount();
  // transaction price usually 1GWei on Rinkeby
  uint32_t gasPrice = 1000000000;  //getGasPrice();
  // set gas limit to 200000, sufficient for all function calls
  uint32_t gasLimit = 200000;
  const char* toAddr = contractAddr;
  // no value is sent with the transaction
  char valueHex[] = "0x0";

  // generate call data
  char* data = (char*)malloc(512 * sizeof(char));
  encodeData(data, closeID, transactionValue, lastSignature);

  // encode transaction parameters according to https://github.com/ethereum/wiki/wiki/RLP
  const char* encoded = util.RlpEncode(nonce, gasPrice, gasLimit, toAddr, valueHex, data, CHAIN_ID).c_str();
  // -2 to because encoded has 0x prefix
  int encodedLen = strlen(encoded) - 2;
  // convert encoded values to bytes
  uint8_t bytearray[encodedLen / 2];
  hexStringToBytes(encoded, bytearray);

  // hash encoded data
  uint8_t dataHash[32] = {0};
  calculateHash(bytearray, sizeof(bytearray), dataHash);

  // sign transaction
  uint8_t signedTransaction[65] = {0};
  Serial.println("Signing Transaction");
  // s part of signature needs to be less than half of the curve n to be accepted by the Ethereum protocol
  // if this condition is not fulfilled signHash() returns 0
  // repeat signature process until condition is met
  while (!signHash(dataHash, signedTransaction, true)) {
    // yield function here is important to avoid crashes
    yield();
    Serial.println("Transaction invalid, retrying");
  }

  // encode signature into raw transaction
  const char* encodedTransaction = util.RlpEncodeForRawTransaction(nonce, gasPrice, gasLimit, toAddr, valueHex, data, signedTransaction, signedTransaction[64]).c_str();

  free(data);

  char methodName[] = "eth_sendRawTransaction";
  char params[1024] = "";
  sprintf(params, "\"%s\"", encodedTransaction);

  // submit transaction
  String transactionHash = web3Call(methodName, params);
  return transactionHash;
}

/**
   @notice get transaction count of an account, used to calculate nonce
   @returns transaction count
*/
uint32_t getTransactionCount() {
  char methodName[] = "eth_getTransactionCount";
  char params[64] = "";
  sprintf(params, "\"%s\",\"latest\"", ethAddr);
  String result = web3Call(methodName, params);
  if (result == "error") {
    return 0;
  }
  // convert hex string to int
  return strtol(result.c_str(), NULL, 16);;
}

/**
   @notice read a public variable from a smart contract
   @param functionID public variables get a getter function automatically during the compilation process, functionID is the identifier of that function
*/
String readFromSmartContract(char* functionID) {
  char callBuffer[512] = "";
  sprintf(callBuffer, "{\"to\": \"%s\", \"data\": \"%s\"}, \"latest\"", contractAddr, functionID);
  String result = web3Call("eth_call", callBuffer);
  return result;
}


/**
   @notice executes a web3 rpc call
   @param body char array with content of request body
          example: "{\"jsonrpc\":\"2.0\",\"method\":\"eth_syncing\",\"params\":[],\"id\":1}"
   @return result on success, "error" on error
*/
String web3Call(char methodName[], char params[]) {
  char* body = (char*)malloc(1024 * sizeof(char));
  sprintf(body, "{\"jsonrpc\":\"2.0\",\"method\":\"%s\",\"params\":[%s],\"id\":1}", methodName, params);
  // http POST
  // if you are using https://rinkeby.infura.io/ as node (or any https node) use next line instead
  //http.begin(nodeAddr, fingerprint);
  http.begin(nodeAddr);
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.POST(body);

  free(body);

  String payload = http.getString();

  http.end();

  if (httpCode > 0) {
    // parse JSON and return result
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, payload);
    // Test if parsing succeeds.
    if (error) {
      Serial.print("Error parsing Json: ");
      Serial.println(error.c_str());
      return "error";
    }
    // json rpc has a "result" keyword on success, "error" on failure
    // https://github.com/ethereum/wiki/wiki/JSON-RPC
    if (doc.containsKey("result")) {
      // for eth_getTransactionReceipt method get the status directly instead of entire result
      if (methodName == "eth_getTransactionReceipt") {
        return doc["result"]["status"];
      }
      return doc["result"];
    } else if (doc.containsKey("error")) {
      // print error message
      const char* node_error = doc["error"]["message"];
      Serial.println(node_error);
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
  @notice signs a hash
  @dev Ethereum has 2 special requirements for transaction signatures
       a special byte v (or recovery id) for easy address recovery (also required for normal signatures)
       s value of signature has to be smaller than half of the curve n
  @param message byte array pointer of message
  @param signedMessage target byte array for signature
  @param isTransaction true if a transaction is being signed, false if a message is signed
  @return 1 on success, 0 on failure or invalid signature
*/
uint8_t signHash(uint8_t* message, uint8_t* signedMessage, bool isTransaction) {
  // v byte of signature
  uint8_t recid = 0;

  // convert private key to bytes
  uint8_t privateKeyBytes[32] = {0};
  hexStringToBytes(privateKey, privateKeyBytes);

  // s has to be smaller than 7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF5D576E7357A4501DDFE92F46681B20A1
  // sign
  if (!uECC_sign_with_recid(privateKeyBytes, message, 32, signedMessage, uECC_secp256k1(), &recid)) {
    return 0;
  }

  // if a transaction was signed, 2nd requirement needs to be met
  if (isTransaction) {
    // compare result to curve.n / 2 + 1
    int i = 0;
    int result = 0;
    do {
      result = halfCurveN[i] - signedMessage[i + 32];
      if (result < 0) {
        return 0;
      } else if (result > 0 || result == 0 && i == 31) {
        break;
      }
      i++;
    } while (result == 0);
    signedMessage[64] = 35 + CHAIN_ID * 2 + recid;
  } else {
    signedMessage[64] = 27 + recid;
  }
  return 1;
}

/**
   UTILITY FUNCTIONS
*/

/**
   @notice packs message arguments according to the solidity specification
           https://solidity.readthedocs.io/en/develop/abi-spec.html?highlight=encodepacked#non-standard-packed-mode
   @param encodedPack target char array pointer for call data
   @param functionID either verifyID or closeID
   @param value of latest off-chain transaction
   @param signature of plug
*/
void encodeData(char* encodedPack, char functionID[], uint64_t value, char* signature) {
  // signature always has the same length, thats why position, length and padding for the encoding can be saved as constants
  char position[] = "0000000000000000000000000000000000000000000000000000000000000040";
  char length[] = "0000000000000000000000000000000000000000000000000000000000000041";
  char padding[] = "00000000000000000000000000000000000000000000000000000000000000";
  // concatenate values and convert integers to 256bit hex with padded zeros
  sprintf(encodedPack, "%s%064llx%s%s%s%s", functionID, value, position, length, signature, padding);
}

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
   @notice random number generator that generates a pseudorandom number for the signature process
           required for the micro-ecc library / uECC.h
*/
static int RNG(uint8_t* dest, unsigned size) {
  while (size) {
    *dest = random(256);
    ++dest;
    --size;
  }
  return 1;
}
