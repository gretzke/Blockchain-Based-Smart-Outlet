// Display libraries
#include <U8g2lib.h>
U8G2_SSD1306_128X32_UNIVISION_F_SW_I2C u8g2(U8G2_R0, 5, 4, 16);

// Wifi libraries
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
HTTPClient http;
// Websocket library
#include <WebSocketsClient.h>
WebSocketsClient webSocket;

// import credentials file
#include "src/credentials.h"

// cryptography libraries
#include <types.h>
#include <uECC.h>
#include <uECC_vli.h>
#include "src/keccak256.h"

// EIP 155 https://github.com/ethereum/EIPs/blob/master/EIPS/eip-155.md
#define CHAIN_ID 4

// private key and ethereum address of plug
const char* privateKey;
const char* ethAddr;

// utility libraries and variables
// arduinojson needs to use long long otherwise it won't be able to parse some JSON data
#define ARDUINOJSON_USE_LONG_LONG 1
#include <ArduinoJson.h>
#include <stdio.h>
#include "src/Util.h"
Util util;

// wifi credentials
// SSID of your Wifi Router
const char* ssid;
// Password of your Wifi Router
const char* wifipass;

// node and contract address, calldata and fingerprint for https requests
char* nodeAddr = "";
char* callData = "";
// when making get/post requests to https urls the fingerprint of that url is required
// fingerprint in case "https://rinkeby.infura.io/" is used as a host
char fingerprint[] = "0B:FE:65:E8:C1:7A:9E:C2:8D:98:5B:BA:1A:EC:7A:4B:83:68:DE:11";

// half of the numeric value of the curve n (secp256k1) used for the signature process
uint8_t halfCurveN[32] = {0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x5D, 0x57, 0x6E, 0x73, 0x57, 0xA4, 0x50, 0x1D, 0xDF, 0xE9, 0x2F, 0x46, 0x68, 0x1B, 0x20, 0xA0};

// status variable for the different states
enum Status {disconnected, connected_P, connected_S, initialized_P, initialized_S, active_P, active_S, closed} paymentStatus;

// address of owner and smart contract, price per second, payment interval
// information will be received by socket
char owner[43];
char contractAddr[43];
uint64_t pricePerSecond = 0;
int secondsPerPayment = 0;

// set desired maximum charging hours here
int maxChargingHours = 11;

// the initial value sent to the smart contract
// max payment 18.44 Ether due to uint64_t limitations
uint64_t maxValue = 0;
// customer nonce, will be fetched from the smart contract
uint64_t nonceSC = 0;

// during the payment process these variables are used to keep track of the number of transactions and the latest value
uint64_t transactionValue = 0;
uint64_t transactionCounter = 0;

// current ethereum price in EUR to calculate the current value in EUR to display it to the user
uint64_t etherPriceEUR = 300;

// during payment process this variable is used to keep track of the time passed
unsigned long timestamp = 0;

// smart contract function IDs
char* priceID = "0x3804fd1f";
char* ownerID = "0x8da5cb5b";
char* nonceID = "0x367c832a";
char* initID = "0x2eb41a8b";
char* verifyID = "0x72c53d9b";

void setup() {
  // setup display
  u8g2.begin();
  u8g2.enableUTF8Print();
  u8g2.setFont(u8g2_font_helvR12_tr);
  u8g2.setFontDirection(0);

  // setup rng function for ecdsa signatures
  uECC_set_rng(&RNG);

  // start serial port
  Serial.begin(115200);
  delay(20);

  // load variables from credentials file
  ssid = credentials.ssid;
  wifipass = credentials.wifipass;
  privateKey = credentials.privkey;
  ethAddr = credentials.address;
  nodeAddr = credentials.nodeAddr;

  // setup wifi
  printToDisplay("Connecting WiFi", true);
  // next 2 lines might be required to prevent disconnects
  WiFi.mode(WIFI_AP);
  wifi_set_sleep_type(NONE_SLEEP_T);
  // Connect to Wi-Fi network
  WiFi.begin(ssid, wifipass);
  while (WiFi.status() != WL_CONNECTED) {
    printLoading();
    delay(250);
  }
  printToDisplay("Connected", true);
  printToDisplay("successfully", false);

  // set current state to disconnected
  paymentStatus = disconnected;

  // connect to websocket
  // server address, port and URL
  webSocket.begin("172.20.10.13", 1337, "/");
  // set event handler function
  webSocket.onEvent(webSocketEvent);
  // if connection has failed try again every 5 seconds
  webSocket.setReconnectInterval(5000);
  // heartbeat required to prevent timeouts during payment process
  // send ping to server every 7.5 seconds
  // expect pong from server within 3 seconds
  // connection is considered disconnected if pong was not received 2 times
  webSocket.enableHeartbeat(7500, 3000, 2);
}

void loop() {
  // the ESP8266 needs to run utility functions in the background
  // if these functions are blocked from running the ESP8266 panics and crashes
  // calling the yield() function allows the ESP8266 to run these utility functions
  yield();
  // calling the websocket event handler to fetch new messages, etc
  webSocket.loop();

  // the rest of the code inside the loop function only runs when the plug is connected to the socket
  if (paymentStatus != disconnected) {
    // execute code according to current payment status
    // connection state code
    if (paymentStatus == connected_P) {
      Serial.println("Sending customer information to socket");
      printToDisplay("Sending Info", true);
      printToDisplay("to socket", false);
      // send customer address to socket
      char messageBuffer[256] = "";
      sprintf(messageBuffer, "{\"id\": %d, \"address\": \"%s\"}", connected_P, ethAddr);
      webSocket.sendTXT(messageBuffer);
      Serial.println("Awaiting payment information from socket\n");

      // update connection state, await sockets connection message
      paymentStatus = connected_S;


      // initialization code
    } else if (paymentStatus == initialized_P) {
      printToDisplay("Fetching data", true);
      printToDisplay("f. smart contract", false);
      Serial.println("Fetching data from Smart Contract");
      // get price per second from smart contract
      String priceHex = readFromSmartContract(priceID);
      // convert hex string to uint
      pricePerSecond = strtoull(priceHex.c_str(), 0, 16);
      Serial.printf("Price: %llu\n", pricePerSecond);

      // get current payment channel customer nonce
      char nonceBuffer[128] = "";
      // Address is 20 bytes, pad 12 bytes with zeros, remove 0x from address
      sprintf(nonceBuffer, "%s000000000000000000000000%s", nonceID, &ethAddr[2]);
      String nonceHex = readFromSmartContract(nonceBuffer);
      // convert hex string to uint
      nonceSC = strtoull(nonceHex.c_str(), 0, 16);
      Serial.printf("Nonce: %llu\n", nonceSC);

      // calculate max value to intialize payment channel in Wei
      maxValue = maxChargingHours * 60 * 60 * pricePerSecond;
      Serial.printf("Max value: %llu\n\n", maxValue);

      printToDisplay("Initializing", true);
      printToDisplay("payment channel", false);
      // send transaction to initialize payment channel
      Serial.println("Sending payment channel initialization transaction");
      String transactionHash = sendTransaction(maxValue, initID);

      // if transaction was sent successfully
      if (transactionHash != "error") {
        Serial.printf("Transaction Hash: %s\n", transactionHash.c_str());

        // convert transaction hash into parameter for transaction receipt call
        char transactionHashBuffer[128] = "";
        sprintf(transactionHashBuffer, "\"%s\"", transactionHash.c_str());
        String result = "null";
        Serial.println("Transaction pending");
        printToDisplay("Pending", true);
        // until transaction is mined, eth_getTransactionReceipt will return null
        // when transaction is mined, it returns 0x1 on success, 0x0 on failure
        while (result == "null") {
          Serial.printf(".");
          printLoading();
          result = web3Call("eth_getTransactionReceipt", transactionHashBuffer);
          delay(250);
        }
        Serial.println();
        Serial.println("Transaction mined");
        // convert hex result to int number
        int success = strtoul(result.c_str(), 0, 16);
        if (success) {
          printToDisplay("Transaction", true);
          printToDisplay("successful", false);
          // on success send payment information to socket
          Serial.println("Transaction was successful");
          char messageBuffer[512] = "";
          sprintf(messageBuffer, "{\"id\": %d, \"price\": %llu, \"nonce\": %llu, \"maxValue\": %llu}", initialized_P, pricePerSecond, nonceSC, maxValue);
          webSocket.sendTXT(messageBuffer);
          // update state and await confirmation from socket
          paymentStatus = initialized_S;
          Serial.println("Awaiting initialization confirmation from socket\n");

          // if transaction was not successful, disconnect
        } else {
          printToDisplay("Transaction", true);
          printToDisplay("failed", false);
          delay(2500);
          Serial.println("Transaction was not successful");
          disconnectFromSocket(true);
        }

        // if transaction was not sent successfully disconnect from socket
      } else {
        Serial.println("Transaction was not sent successfully");
        disconnectFromSocket(true);
      }

      // payment code
    } else if (paymentStatus == active_P) {
      // send first transaction regardless whether current is flowing or not
      if (transactionCounter != 0) {
        // after first transaction was sent wait until current flows, start timer
        if (transactionCounter == 1) {
          double Amps = measureCurrent();
          if (Amps <= 0.3) {
            // return statement restarts the loop function
            return;
          }
          Serial.printf("\nFirst transaction successful, current flowing: %lf A\n", Amps);
          timestamp = millis() + secondsPerPayment * 1000;

        } else {
          // wait until 5 seconds of payment left to send next transaction
          if (timestamp - millis() > 5000) {
            return;
          }
          // measure if current is still flowing before sending the next transaction
          double Amps = measureCurrent();
          Serial.printf("Current: %lf A\n", Amps);
          if (Amps > 0.3) {
            timestamp += secondsPerPayment * 1000;
          } else {
            // close channel if no current present
            paymentStatus = closed;
            webSocket.sendTXT("{\"id\": 7}");
            return;
          }
        }
      }

      Serial.println("Signing Message");
      // increment transaction counter and calculate new transaction value and value in EUR
      transactionCounter += 1;
      transactionValue = transactionCounter * pricePerSecond * secondsPerPayment;
      double euroValue = (transactionValue * etherPriceEUR) / 1000000000000000000.0;
      Serial.printf("Transaction counter: %llu\n", transactionCounter);
      Serial.printf("Current Transaction Value: %llu\n", transactionValue);
      char euroBuffer[16] = "";
      sprintf(euroBuffer, "%.4lf EUR", euroValue);
      Serial.println(euroBuffer);
      printToDisplay("Payment active:", true);
      printToDisplay(euroBuffer, false);
      // sign message
      uint8_t signedMessage[65] = {0};
      signPayment(signedMessage);
      char signedMessageString[131] = "";
      bytesToHexString(signedMessage, sizeof(signedMessage), signedMessageString, false);
      char messageBuffer[512] = "";
      sprintf(messageBuffer, "{\"id\": %d, \"signature\": \"%s\"}", active_P, signedMessageString);
      // send signed message to socket
      webSocket.sendTXT(messageBuffer);
      Serial.println("Transferring Transaction to Socket\n");
    }
  }
}

/**
  @notice disconnect from socket
  @param notifySocket if true, notify socket of disconnect before disconnecting
*/
void disconnectFromSocket(bool notifySocket) {
  paymentStatus = disconnected;
  if (notifySocket) {
    webSocket.sendTXT("{\"id\": 0}");
  }
  printToDisplay("Disconnected", true);
  printToDisplay("from socket", false);
  Serial.println("Disconnected from socket");
  WiFi.disconnect();
}

/**
    Websocket Callback function
    https://github.com/Links2004/arduinoWebSockets/
*/
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {

  switch (type) {
    // gets called when websocket connection disconnects
    case WStype_DISCONNECTED:
      disconnectFromSocket(false);
      break;

    // gets called when connected to websocket server
    case WStype_CONNECTED:
      Serial.println("Connected to socket");
      // set state to execute connection code
      paymentStatus = connected_P;
      printToDisplay("Connected", true);
      printToDisplay("to socket", false);
      break;

    // gets called when client receives a text message from websocket server
    case WStype_TEXT:
      Serial.printf("new message from socket: %s\n", payload);

      // parse JSON to doc
      StaticJsonDocument<512> doc;
      DeserializationError error = deserializeJson(doc, payload);

      // If parsing does not succeed, return
      if (error) {
        Serial.print(F("deserializeJson() failed with code "));
        Serial.println(error.c_str());
        return;
      }

      // messages from socket will be identified and matched by "id"
      if (doc.containsKey("id")) {

        // received payment information from socket, ready to initiate payment process
        if (doc["id"] == (int)connected_S) {
          Serial.println("Received payment information from socket: ");
          // save contract and owner addresses, seconds per payment
          strcpy(contractAddr, doc["contract"].as<char*>());
          strcpy(owner, doc["owner"].as<char*>());
          secondsPerPayment = doc["secondsPerPayment"].as<int>();
          Serial.printf("Contract Address: %s\n", contractAddr);
          Serial.printf("Owner Address: %s\n", owner);
          Serial.printf("Seconds per payment: %d\n\n", secondsPerPayment);
          // update state to execute initialization code
          paymentStatus = initialized_P;

          // socket accepted initialization, ready to send offline transactions
        } else if (doc["id"] == (int)initialized_S) {
          Serial.println("Ready to send payments\n");
          transactionValue = 0;
          transactionCounter = 0;
          // update state to send payments
          paymentStatus = active_P;

          // payment channel closed
        } else if (doc["id"] == (int)closed) {
          Serial.println("Payment channel closed");
          double euroValue = (transactionValue * etherPriceEUR) / 1000000000000000000.0;
          char euroBuffer[16] = "";
          sprintf(euroBuffer, "%.4lf EUR", euroValue);
          Serial.printf("Final value: %s\n", euroBuffer);
          printToDisplay("P.C. closed", true);
          printToDisplay(euroBuffer, false);
          // update state to disconnected
          paymentStatus = disconnected;

          // socket disconnected
        } else if (doc["id"] == (int)disconnected) {
          Serial.println("Socket requested disconnect");
          disconnectFromSocket(false);

        } else {
          Serial.printf("\n---\nNew message that couldn't be assigned by id:\n%s\n---\n", payload);
        }
      } else {
        Serial.printf("\n---\nNew message that couldn't be identified:\n%s\n---\n", payload);
      }

      break;
  }

}


/**
   ETHEREUM FUNCTIONS
*/

/**
   @notice send transaction to the smart contract
   @param value in wei
   @param data as hex string
   @return transaction hash of transaction sent on success, "error" on failure
*/
String sendTransaction(uint64_t value, char* data) {
  uint32_t nonce = getTransactionCount();
  // transaction price usually 1GWei on Rinkeby
  uint32_t gasPrice = 1000000000;  //getGasPrice();
  // set gas limit to 200000, sufficient for all function calls
  uint32_t gasLimit = 200000;
  const char* toAddr = contractAddr;
  // convert value to a hex string
  char valueHex[19] = "";
  sprintf(valueHex, "0x%llx", value);

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
  char methodName[] = "eth_sendRawTransaction";
  char params[256] = "";
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
  int returnValue = 0;
  char methodName[] = "eth_getTransactionCount";
  char params[256] = "";
  sprintf(params, "\"%s\",\"latest\"", ethAddr);
  String result = web3Call(methodName, params);
  if (result == "error") {
    return 0;
  }
  // convert hex string to int
  return strtol(result.c_str(), 0, 16);
}

/**
   @notice signs a new off-chain transaction
   @param signedMessage pointer to empty byte array, is filled with signed payment
*/
void signPayment(uint8_t* signedMessage) {
  // encode payment values
  char encodedPack[512];
  encodePacked(encodedPack);

  // convert encoded values to bytes
  int encodedLen = strlen(encodedPack);
  uint8_t bytearray[encodedLen / 2];
  hexStringToBytes(encodedPack, bytearray);

  // hash data
  uint8_t dataHash[32] = {0};
  calculateHash(bytearray, sizeof(bytearray), dataHash);

  // prefix hash with ethereum specific prefix
  uint8_t prefixedHash[32] = {0};
  prefixMessage(dataHash, prefixedHash);

  // sign message
  signHash(prefixedHash, signedMessage, false);
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
  char* body = (char*)malloc(2048 * sizeof(char));
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
    DynamicJsonDocument doc(2048);
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
   @notice packs message arguments according to the solidity specification
           https://solidity.readthedocs.io/en/develop/abi-spec.html?highlight=encodepacked#non-standard-packed-mode
   @param encodedPack pointer to the target variable
*/
void encodePacked(char* encodedPack) {
  // encodes value of the transaction, address of the smart contract and customer nonce inside smart contract
  // convert integer to 256bit hex with padded zeros
  // copy hex values and contract addr to encoded pack
  sprintf(encodedPack, "%064llx%s%064llx", transactionValue, &contractAddr[2], nonceSC);
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
  @notice prefixes a hash with an Ethereum specific prefix and hashes the result again
  @dev https://github.com/ethereum/EIPs/issues/191
  @param hash byte array pointer with hash
  @param prefixedMessage target byte array pointer for prefixed hash
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
  @notice measures current for 1 second
  @return amps measured
*/
double measureCurrent() {

  int readValue;             //value read from the sensor
  int maxValue = 0;          // store max value here
  int minValue = 1024;       // store min value here

  uint32_t startTime = millis();
  // measure for 1 second
  while ((millis() - startTime) < 1000) {
    readValue = analogRead(A0);
    yield();
    if (readValue > maxValue) {
      // record the maximum sensor value
      maxValue = readValue;
    }
    if (readValue < minValue) {
      // record the minimum sensor value
      minValue = readValue;
    }
  }

  int mVperAmp = 100; // use 185 for 5A Module, 100 for 20A Module and 66 for 30A Module
  // Calculate voltage
  double Voltage = ((maxValue - minValue) * 5.0) / 1024.0;
  // Calculate RMS value of voltage
  double VoltageRMS = (Voltage / 2.0) * 0.707;
  // return Amperes RMS
  return (VoltageRMS * 1000) / mVperAmp;
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
  @notice convert byte array to hex char array
  @param bytes pointer to byte array
  @param byteSize length of byte array
  @param hexString char array pointer
  @param prefixed bool whether hex string should be prefixed or not
*/
void bytesToHexString(uint8_t* bytes, int byteSize, char* hexString, bool prefixed) {
  int prefixOffset = 0;
  if (prefixed) {
    prefixOffset = 2;
    hexString[0] = '0';
    hexString[1] = 'x';
  }
  for (int i = 0; i < byteSize; i++) {
    sprintf(&hexString[i * 2 + prefixOffset], "%02x", bytes[i]);
  }
  hexString[byteSize * 2 + prefixOffset] = 0;
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
  u8g2.clearBuffer();
  u8g2.setCursor(0, 12);
  u8g2.print(upperText);
  u8g2.setCursor(0, 30);
  u8g2.print(lowerText);
  u8g2.sendBuffer();
}

/**
   @notice prints a dot to the second line, adds another dot on every call, resets after 7 dots
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
