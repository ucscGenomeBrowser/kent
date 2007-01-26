/* paypalSignEncrypt.h - routines to sign and encrypt button data using openssl */

#ifndef PPSIGNENCRYPT_H
#define PPSIGNENCRYPT_H


#include "common.h"

#include "openssl/buffer.h"
#include "openssl/bio.h"
#include "openssl/sha.h"
#include "openssl/rand.h"
#include "openssl/err.h"
#include "openssl/rsa.h"
#include "openssl/evp.h"
#include "openssl/x509.h"
#include "openssl/x509v3.h"
#include "openssl/pkcs7.h"
#include "openssl/pem.h"

/* The following code comes directly from PayPal's ButtonEncyption.cpp file, and has been
   modified only to work with C
*/

char* sign_and_encrypt(const char *data, RSA *rsa, X509 *x509, X509 *PPx509, bool verbose);
/* sign and encrypt button data for safe delivery to paypal */

char* sign_and_encryptFromFiles(const char *data, char *keyFile, char *certFile, char *ppCertFile, bool verbose);
/* sign and encrypt button data for safe delivery to paypal, use keys/certs in specified filenames */

#endif
