#ifndef CREDENTIALS_H
#define CREDENTIALS_H

struct Credentials {
    char *privkey;
    char *address;
    char *ssid;
    char *wifipass;
    char *nodeAddr;
    char *contractAddress;
};

extern struct Credentials credentials;

#endif
