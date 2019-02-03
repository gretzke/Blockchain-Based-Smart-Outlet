#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <string>
#include <vector>

using namespace std;

class Util {
   public:
    // RLP implementation https://github.com/ethereum/wiki/wiki/RLP
    string RlpEncode(uint32_t nonceVal, uint32_t gasPriceVal, uint32_t gasLimitVal, const char* _toStr, const char* _valueStr, const char* _dataStr, uint32_t chainID);
    string RlpEncodeForRawTransaction(uint32_t nonceVal, uint32_t gasPriceVal, uint32_t gasLimitVal, const char* _toStr, const char* _valueStr, const char* _dataStr, uint8_t* sig, uint8_t recid);

   private:
    string ExpandNibbles(string s);
    vector<uint8_t> RlpEncodeWholeHeaderWithVector(uint32_t total_len);
    vector<uint8_t> RlpEncodeItemWithVector(const vector<uint8_t> input);

    vector<uint8_t> ConvertNumberToVector(uint32_t val);
    vector<uint8_t> ConvertCharStrToVector(const uint8_t* in);
    vector<uint8_t> ConvertStringToVector(const string* str);

    string VectorToString(const vector<uint8_t> buf);
};

#endif  //UTIL_H