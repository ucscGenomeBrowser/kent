/* paypalSignEncrypt.h - routines to sign and encrypt button data using openssl */

#ifndef PPSIGNENCRYPT_H
#define PPSIGNENCRYPT_H


/* The following code comes directly from PayPal's ButtonEncyption.cpp file, and has been
   modified only to work with C
*/

char* sign_and_encryptFromFiles(
  const char *data, char *keyFile, char *certFile, char *ppCertFile, int verbose);
/* sign and encrypt button data for safe delivery to paypal, use keys/certs in specified filenames.  Free return value with free(). */

#endif
