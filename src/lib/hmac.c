/* Calculate an openssl keyed-hash message authentication code (HMAC) */
// You may use other openssl hash engines. e.g EVP_md5(), EVP_sha224,
// EVP_sha512, etc
// Be careful of the length of string with the choosen hash engine.
// SHA1 needed 20 characters, MD5 needed 16 characters.
// Change the length accordingly with your choosen hash engine

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "openssl/hmac.h"
#include "openssl/evp.h"
#include "common.h"

char *hmacSha1(char *key, char *data)
/* Calculate a openssl SHA1 keyed-hash message authentication code (HMAC) */
{
unsigned char* digest;
digest=HMAC(EVP_sha1(), key, strlen(key), (unsigned char*)data, strlen(data), NULL, NULL);
char hmacStr[40];
int i;
for(i = 0; i < 20; i++)
    sprintf(&hmacStr[i*2], "%02x", (unsigned int)digest[i]);
return cloneStringZ(hmacStr, sizeof(hmacStr));
}

char *hmacMd5(char *key, char *data)
/* Calculate a openssl MD5 keyed-hash message authentication code (HMAC) */
{
unsigned char* digest;
digest=HMAC(EVP_md5(), key, strlen(key), (unsigned char*)data, strlen(data), NULL, NULL);
//printf("Raw mdr digest: %s\n", digest);
char hmacStr[32];
int i;
for(i = 0; i < 16; i++)
    sprintf(&hmacStr[i*2], "%02x", (unsigned int)digest[i]);
return cloneStringZ(hmacStr, sizeof(hmacStr));
}

