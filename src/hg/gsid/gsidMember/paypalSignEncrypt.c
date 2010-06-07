/* paypalSignEncrypt.h - routines to sign and encrypt button data using openssl */

#include <string.h>

#include "paypalSignEncrypt.h"

/* The following code comes directly from PayPal's ButtonEncyption.cpp file, and has been
   modified only to work with C
*/

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

char* sign_and_encrypt(const char *data, RSA *rsa, X509 *x509, X509 *PPx509, int verbose)
/* sign and encrypt button data for safe delivery to paypal */
{
	char *ret = NULL;
	EVP_PKEY *pkey;
	PKCS7 *p7 = NULL;
	BIO *p7bio = NULL;
	BIO *bio = NULL;
	PKCS7_SIGNER_INFO* si;
	int len;
	char *str;

	pkey = EVP_PKEY_new();

	if (EVP_PKEY_set1_RSA(pkey, rsa) == 0)
	{
		fprintf(stderr,"Fatal Error: Unable to create EVP_KEY from RSA key\n");fflush(stderr);
		goto end;
	} else if (verbose) {
		printf("Successfully created EVP_KEY from RSA key\n");
	}

	/* Create a signed and enveloped PKCS7 */
	p7 = PKCS7_new();
	PKCS7_set_type(p7, NID_pkcs7_signedAndEnveloped);

	si = PKCS7_add_signature(p7, x509, pkey, EVP_sha1());

	if (si) {
		if (PKCS7_add_signed_attribute(si, NID_pkcs9_contentType, V_ASN1_OBJECT,
			OBJ_nid2obj(NID_pkcs7_data)) <= 0)
		{
			fprintf(stderr,"Fatal Error: Unable to add signed attribute to certificate\n");
			fprintf(stderr,"OpenSSL Error: %s\n", ERR_error_string(ERR_get_error(), NULL));
			fflush(stderr);
			goto end;
		} else if (verbose) {
			printf("Successfully added signed attribute to certificate\n");
		}

	} else {
		fprintf(stderr,"Fatal Error: Failed to sign PKCS7\n");fflush(stderr);
		goto end;
	}

	/* Encryption */
	if (PKCS7_set_cipher(p7, EVP_des_ede3_cbc()) <= 0)
	{
		fprintf(stderr,"Fatal Error: Failed to set encryption algorithm\n");
		fprintf(stderr,"OpenSSL Error: %s\n", ERR_error_string(ERR_get_error(), NULL));
		fflush(stderr);
		goto end;
	} else if (verbose) {
		printf("Successfully added encryption algorithm\n");
	}

	if (PKCS7_add_recipient(p7, PPx509) <= 0)
	{
		fprintf(stderr,"Fatal Error: Failed to add PKCS7 recipient\n");
		fprintf(stderr,"OpenSSL Error: %s\n", ERR_error_string(ERR_get_error(), NULL));
		fflush(stderr);
		goto end;
	} else if (verbose) {
		printf("Successfully added recipient\n");
	}

	if (PKCS7_add_certificate(p7, x509) <= 0)
	{
		fprintf(stderr,"Fatal Error: Failed to add PKCS7 certificate\n");
		fprintf(stderr,"OpenSSL Error: %s\n", ERR_error_string(ERR_get_error(), NULL));
		fflush(stderr);
		goto end;
	} else if (verbose) {
		printf("Successfully added certificate\n");
	}

	p7bio = PKCS7_dataInit(p7, NULL);
	if (!p7bio) {
		fprintf(stderr,"OpenSSL Error: %s\n", ERR_error_string(ERR_get_error(), NULL));
		fflush(stderr);
		goto end;
	}

	/* Pump data to special PKCS7 BIO. This encrypts and signs it. */
	BIO_write(p7bio, data, strlen(data));
	BIO_flush(p7bio);
	PKCS7_dataFinal(p7, p7bio);

	/* Write PEM encoded PKCS7 */
	bio = BIO_new(BIO_s_mem());

	if (!bio || (PEM_write_bio_PKCS7(bio, p7) == 0))
	{
		fprintf(stderr,"Fatal Error: Failed to create PKCS7 PEM\n");fflush(stderr);
	} else if (verbose) {
		printf("Successfully created PKCS7 PEM\n");
	}

	BIO_flush(bio);
	len = BIO_get_mem_data(bio, &str);
	ret = malloc(sizeof(char)*(len+1));
	memcpy(ret, str, len);
	ret[len] = 0;

end:
	/* Free everything */
	if (bio)
		BIO_free_all(bio);
	if (p7bio)
		BIO_free_all(p7bio);
	if (p7)
		PKCS7_free(p7);
	if (pkey)
		EVP_PKEY_free(pkey);
	return ret;
}


char* sign_and_encryptFromFiles(const char *data, char *keyFile, char *certFile, char *ppCertFile, int verbose)
/* sign and encrypt button data for safe delivery to paypal, use keys/certs in specified filenames.  Free return value with free(). */
{
    ERR_load_crypto_strings();
    OpenSSL_add_all_algorithms();

    /* Load PayPal cert */
    BIO *bio=BIO_new_file(ppCertFile,"rt");
    if (!bio) 
	{
	fprintf(stderr,"Error loading file: %s\n", ppCertFile);fflush(stderr);
	return NULL;
	}

    X509 *ppX509=PEM_read_bio_X509(bio,NULL,NULL,NULL);
    if (!ppX509) {
	fprintf(stderr,"Error bio_reading PayPal certificate from %s\n", ppCertFile);fflush(stderr);
	return NULL;
	}

    BIO_free(bio);


    /* Load Public cert */
    bio=BIO_new_file(certFile,"rt");
    if (!bio) 
	{
	fprintf(stderr,"Error loading file: %s\n", certFile);fflush(stderr);
	return NULL;
	}

    X509 *x509=PEM_read_bio_X509(bio,NULL,NULL,NULL);
    if (!x509) {
	fprintf(stderr,"Error bio_reading Public certificate from %s\n", certFile);fflush(stderr);
	return NULL;
	}

    BIO_free(bio);



    /* Load Private key */
    bio=BIO_new_file(keyFile,"rt");
    if (!bio) 
	{
	fprintf(stderr,"Error loading file: %s\n", keyFile);fflush(stderr);
	return NULL;
	}

    RSA *rsa=PEM_read_bio_RSAPrivateKey(bio,NULL,NULL,NULL);
    if (!rsa) {
	fprintf(stderr,"Error bio_reading RSA key from %s\n", keyFile);fflush(stderr);
	return NULL;
	}

    BIO_free(bio);


    return sign_and_encrypt(data,rsa,x509,ppX509,verbose);

}

