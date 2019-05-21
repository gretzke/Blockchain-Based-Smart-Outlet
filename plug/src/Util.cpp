//
// Created by Okada, Takahiro on 2018/02/11.
// Modified by Gretzke, Daniel on 2019/05/21
//

#include "Util.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "Arduino.h"

#define SIGNATURE_LENGTH 64

string Util::RlpEncode(
    uint32_t nonceVal, uint32_t gasPriceVal, uint32_t gasLimitVal, const char *_toStr, const char *_valueStr, const char *_dataStr, uint32_t chainID) {
    string toStr(_toStr);
    string valueStr(_valueStr);
    string dataStr(_dataStr);

    toStr = ExpandNibbles(toStr);
    valueStr = ExpandNibbles(valueStr);
    dataStr = ExpandNibbles(dataStr);

    vector<uint8_t> nonce = Util::ConvertNumberToVector(nonceVal);
    vector<uint8_t> gasPrice = Util::ConvertNumberToVector(gasPriceVal);
    vector<uint8_t> gasLimit = Util::ConvertNumberToVector(gasLimitVal);
    vector<uint8_t> to = Util::ConvertStringToVector(&toStr);
    vector<uint8_t> value = Util::ConvertStringToVector(&valueStr);
    vector<uint8_t> data = Util::ConvertStringToVector(&dataStr);

    vector<uint8_t> outputNonce = Util::RlpEncodeItemWithVector(nonce);
    vector<uint8_t> outputGasPrice = Util::RlpEncodeItemWithVector(gasPrice);
    vector<uint8_t> outputGasLimit = Util::RlpEncodeItemWithVector(gasLimit);
    vector<uint8_t> outputTo = Util::RlpEncodeItemWithVector(to);
    vector<uint8_t> outputValue = Util::RlpEncodeItemWithVector(value);
    vector<uint8_t> outputData = Util::RlpEncodeItemWithVector(data);

    vector<uint8_t> R;
    R.push_back((uint8_t)0x0);
    vector<uint8_t> S;
    S.push_back((uint8_t)0x0);
    vector<uint8_t> V;
    V.push_back((uint32_t)chainID);
    vector<uint8_t> outputR = Util::RlpEncodeItemWithVector(R);
    vector<uint8_t> outputS = Util::RlpEncodeItemWithVector(S);
    vector<uint8_t> outputV = Util::RlpEncodeItemWithVector(V);

    vector<uint8_t> encoded = Util::RlpEncodeWholeHeaderWithVector(
        outputNonce.size() +
        outputGasPrice.size() +
        outputGasLimit.size() +
        outputTo.size() +
        outputValue.size() +
        outputData.size() +
        outputR.size() +
        outputS.size() +
        outputV.size());

    encoded.insert(encoded.end(), outputNonce.begin(), outputNonce.end());
    encoded.insert(encoded.end(), outputGasPrice.begin(), outputGasPrice.end());
    encoded.insert(encoded.end(), outputGasLimit.begin(), outputGasLimit.end());
    encoded.insert(encoded.end(), outputTo.begin(), outputTo.end());
    encoded.insert(encoded.end(), outputValue.begin(), outputValue.end());
    encoded.insert(encoded.end(), outputData.begin(), outputData.end());
    encoded.insert(encoded.end(), outputV.begin(), outputV.end());
    encoded.insert(encoded.end(), outputR.begin(), outputR.end());
    encoded.insert(encoded.end(), outputS.begin(), outputS.end());

    return VectorToString(encoded);
}

string Util::RlpEncodeForRawTransaction(
    uint32_t nonceVal, uint32_t gasPriceVal, uint32_t gasLimitVal, const char *_toStr, const char *_valueStr, const char *_dataStr, uint8_t *sig, uint8_t recid) {
    string toStr(_toStr);
    string valueStr(_valueStr);
    string dataStr(_dataStr);

    toStr = ExpandNibbles(toStr);
    valueStr = ExpandNibbles(valueStr);
    dataStr = ExpandNibbles(dataStr);

    vector<uint8_t> signature;
    for (int i = 0; i < SIGNATURE_LENGTH; i++) {
        signature.push_back(sig[i]);
    }

    vector<uint8_t> nonce = Util::ConvertNumberToVector(nonceVal);
    vector<uint8_t> gasPrice = Util::ConvertNumberToVector(gasPriceVal);
    vector<uint8_t> gasLimit = Util::ConvertNumberToVector(gasLimitVal);
    vector<uint8_t> to = Util::ConvertStringToVector(&toStr);
    vector<uint8_t> value = Util::ConvertStringToVector(&valueStr);
    vector<uint8_t> data = Util::ConvertStringToVector(&dataStr);

    vector<uint8_t> outputNonce = Util::RlpEncodeItemWithVector(nonce);
    vector<uint8_t> outputGasPrice = Util::RlpEncodeItemWithVector(gasPrice);
    vector<uint8_t> outputGasLimit = Util::RlpEncodeItemWithVector(gasLimit);
    vector<uint8_t> outputTo = Util::RlpEncodeItemWithVector(to);
    vector<uint8_t> outputValue = Util::RlpEncodeItemWithVector(value);
    vector<uint8_t> outputData = Util::RlpEncodeItemWithVector(data);

    vector<uint8_t> R;
    R.insert(R.end(), signature.begin(), signature.begin() + (SIGNATURE_LENGTH / 2));
    vector<uint8_t> S;
    S.insert(S.end(), signature.begin() + (SIGNATURE_LENGTH / 2), signature.end());
    vector<uint8_t> V;
    V.push_back(recid);
    vector<uint8_t> outputR = Util::RlpEncodeItemWithVector(R);
    vector<uint8_t> outputS = Util::RlpEncodeItemWithVector(S);
    vector<uint8_t> outputV = Util::RlpEncodeItemWithVector(V);

    vector<uint8_t> encoded = Util::RlpEncodeWholeHeaderWithVector(
        outputNonce.size() +
        outputGasPrice.size() +
        outputGasLimit.size() +
        outputTo.size() +
        outputValue.size() +
        outputData.size() +
        outputR.size() +
        outputS.size() +
        outputV.size());

    encoded.insert(encoded.end(), outputNonce.begin(), outputNonce.end());
    encoded.insert(encoded.end(), outputGasPrice.begin(), outputGasPrice.end());
    encoded.insert(encoded.end(), outputGasLimit.begin(), outputGasLimit.end());
    encoded.insert(encoded.end(), outputTo.begin(), outputTo.end());
    encoded.insert(encoded.end(), outputValue.begin(), outputValue.end());
    encoded.insert(encoded.end(), outputData.begin(), outputData.end());
    encoded.insert(encoded.end(), outputV.begin(), outputV.end());
    encoded.insert(encoded.end(), outputR.begin(), outputR.end());
    encoded.insert(encoded.end(), outputS.begin(), outputS.end());

    return VectorToString(encoded);
}

// if string has a leading nibble, expand it (e.g. 0x1 -> 0x01)
string Util::ExpandNibbles(string s) {
    if (s.length() % 2) {
        if (s[0] == '0' && s[1] == 'x') {
            s.insert(s.begin() + 2, '0');
        } else {
            s.insert(s.begin(), '0');
        }
    }
    return s;
}

vector<uint8_t> Util::RlpEncodeWholeHeaderWithVector(uint32_t total_len) {
    vector<uint8_t> header_output;
    if (total_len < 55) {
        header_output.push_back((uint8_t)0xc0 + (uint8_t)total_len);
    } else {
        vector<uint8_t> tmp_header;
        uint32_t hexdigit = 1;
        uint32_t tmp = total_len;
        while ((uint32_t)(tmp / 256) > 0) {
            tmp_header.push_back((uint8_t)(tmp % 256));
            tmp = (uint32_t)(tmp / 256);
            hexdigit++;
        }
        tmp_header.push_back((uint8_t)(tmp));
        tmp_header.insert(tmp_header.begin(), 0xf7 + (uint8_t)hexdigit);

        // fix direction for header
        vector<uint8_t> header;
        header.push_back(tmp_header[0]);
        for (int i = 0; i < tmp_header.size() - 1; i++) {
            header.push_back(tmp_header[tmp_header.size() - 1 - i]);
        }

        header_output.insert(header_output.end(), header.begin(), header.end());
    }
    return header_output;
}

vector<uint8_t> Util::RlpEncodeItemWithVector(vector<uint8_t> input) {
    vector<uint8_t> output;
    uint16_t input_len = input.size();

    if (input_len == 1 && input[0] == 0x00) {
        output.push_back(0x80);
    } else if (input_len == 1 && input[0] < 128) {
        output.insert(output.end(), input.begin(), input.end());
    } else if (input_len <= 55) {
        uint8_t _ = (uint8_t)0x80 + (uint8_t)input_len;
        output.push_back(_);
        output.insert(output.end(), input.begin(), input.end());
    } else {
        vector<uint8_t> tmp_header;
        uint32_t tmp = input_len;
        while ((uint32_t)(tmp / 256) > 0) {
            tmp_header.push_back((uint8_t)(tmp % 256));
            tmp = (uint32_t)(tmp / 256);
        }
        tmp_header.push_back((uint8_t)(tmp));
        uint8_t len = tmp_header.size();
        tmp_header.insert(tmp_header.begin(), 0xb7 + len);

        // fix direction for header
        vector<uint8_t> header;
        header.push_back(tmp_header[0]);
        uint8_t hexdigit = tmp_header.size() - 1;
        for (int i = 0; i < hexdigit; i++) {
            header.push_back(tmp_header[hexdigit - i]);
        }

        output.insert(output.end(), header.begin(), header.end());
        output.insert(output.end(), input.begin(), input.end());
    }
    return output;
}

vector<uint8_t> Util::ConvertNumberToVector(uint32_t val) {
    vector<uint8_t> tmp;
    vector<uint8_t> ret;
    if ((uint32_t)(val / 256) >= 0) {
        while ((uint32_t)(val / 256) > 0) {
            tmp.push_back((uint8_t)(val % 256));
            val = (uint32_t)(val / 256);
        }
        tmp.push_back((uint8_t)(val % 256));
        uint8_t len = tmp.size();
        for (int i = 0; i < len; i++) {
            ret.push_back(tmp[len - i - 1]);
        }
    } else {
        ret.push_back((uint8_t)val);
    }
    return ret;
}

vector<uint8_t> Util::ConvertCharStrToVector(const uint8_t *in) {
    uint32_t ret = 0;
    uint8_t tmp[2048];
    strcpy((char *)tmp, (char *)in);
    vector<uint8_t> out;

    // remove "0x"
    char *ptr = strtok((char *)tmp, "x");
    if (strlen(ptr) != 1) {
        ptr = (char *)tmp;
    } else {
        ptr = strtok(NULL, "x");
    }

    size_t lenstr = strlen(ptr);
    for (int i = 0; i < lenstr; i += 2) {
        char c[3];
        c[0] = *(ptr + i);
        c[1] = *(ptr + i + 1);
        c[2] = 0x00;
        uint8_t val = strtol(c, nullptr, 16);
        out.push_back(val);
        ret++;
    }
    return out;
}

vector<uint8_t> Util::ConvertStringToVector(const string *str) {
    return ConvertCharStrToVector((uint8_t *)(str->c_str()));
}

string Util::VectorToString(const vector<uint8_t> buf) {
    string ret = "0x";
    for (int i = 0; i < buf.size(); i++) {
        char v[3];
        memset(v, 0, sizeof(v));
        sprintf(v, "%02x", buf[i]);
        ret = ret + string(v);
    }
    return ret;
}