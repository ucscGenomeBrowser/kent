/* paypalSignEncrypt.h - routines to sign and encrypt button data using openssl */

#include "paypalSignEncrypt.h"

/* The following code comes directly from PayPal's ButtonEncyption.cpp file, and has been
   modified only to work with C
*/

char* sign_and_encrypt(const char *data, RSA *rsa, X509 *x509, X509 *PPx509, bool verbose)
/* sign and encrypt button data for safe delivery to paypal */
{
	char *ret = NULL;
	EVP_PKEY *pkey;
	PKCS7 *p7 = NULL;
	BIO *memBio = NULL;
	BIO *p7bio = NULL;
	BIO *bio = NULL;
	PKCS7_SIGNER_INFO* si;
	int len;
	char *str;

	pkey = EVP_PKEY_new();

	if (EVP_PKEY_set1_RSA(pkey, rsa) == 0)
	{
		printf("Fatal Error: Unable to create EVP_KEY from RSA key\n");
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
			printf("Fatal Error: Unable to add signed attribute to certificate\n");
			printf("OpenSSL Error: %s\n", ERR_error_string(ERR_get_error(), NULL));
			goto end;
		} else if (verbose) {
			printf("Successfully added signed attribute to certificate\n");
		}

	} else {
		printf("Fatal Error: Failed to sign PKCS7\n");
		goto end;
	}

	/* Encryption */
	if (PKCS7_set_cipher(p7, EVP_des_ede3_cbc()) <= 0)
	{
		printf("Fatal Error: Failed to set encryption algorithm\n");
		printf("OpenSSL Error: %s\n", ERR_error_string(ERR_get_error(), NULL));
		goto end;
	} else if (verbose) {
		printf("Successfully added encryption algorithm\n");
	}

	if (PKCS7_add_recipient(p7, PPx509) <= 0)
	{
		printf("Fatal Error: Failed to add PKCS7 recipient\n");
		printf("OpenSSL Error: %s\n", ERR_error_string(ERR_get_error(), NULL));
		goto end;
	} else if (verbose) {
		printf("Successfully added recipient\n");
	}

	if (PKCS7_add_certificate(p7, x509) <= 0)
	{
		printf("Fatal Error: Failed to add PKCS7 certificate\n");
		printf("OpenSSL Error: %s\n", ERR_error_string(ERR_get_error(), NULL));
		goto end;
	} else if (verbose) {
		printf("Successfully added certificate\n");
	}

	memBio = BIO_new(BIO_s_mem());
	p7bio = PKCS7_dataInit(p7, memBio);

	if (!p7bio) {
		printf("OpenSSL Error: %s\n", ERR_error_string(ERR_get_error(), NULL));
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
		printf("Fatal Error: Failed to create PKCS7 PEM\n");
	} else if (verbose) {
		printf("Successfully created PKCS7 PEM\n");
	}

	BIO_flush(bio);
	len = BIO_get_mem_data(bio, &str);
	ret = needMem(sizeof(char)*(len+1));
	memcpy(ret, str, len);
	ret[len] = 0;

end:
	/* Free everything */
	if (p7)
		PKCS7_free(p7);
	if (bio)
		BIO_free_all(bio);
	if (memBio)
		BIO_free_all(memBio);
	if (p7bio)
		BIO_free_all(p7bio);
	if (pkey)
		EVP_PKEY_free(pkey);
	return ret;
}


char* sign_and_encryptFromFiles(const char *data, char *keyFile, char *certFile, char *ppCertFile, bool verbose)
/* sign and encrypt button data for safe delivery to paypal, use keys/certs in specified filenames */
{

    ERR_load_crypto_strings();
    OpenSSL_add_all_algorithms();

    /* Load PayPal cert */
    BIO *bio=BIO_new_file(ppCertFile,"rt");
    if (!bio) 
	{
	printf("Error loading file: %s\n", ppCertFile);
	return NULL;
	}

    X509 *ppX509=PEM_read_bio_X509(bio,NULL,NULL,NULL);
    if (!ppX509) {
	printf("Error bio_reading PayPal certificate from %s\n", ppCertFile);
	return NULL;
	}

    BIO_free(bio);


    /* Load Public cert */
    bio=BIO_new_file(certFile,"rt");
    if (!bio) 
	{
	printf("Error loading file: %s\n", certFile);
	return NULL;
	}

    X509 *x509=PEM_read_bio_X509(bio,NULL,NULL,NULL);
    if (!x509) {
	printf("Error bio_reading Public certificate from %s\n", certFile);
	return NULL;
	}

    BIO_free(bio);



    /* Load Private key */
    bio=BIO_new_file(keyFile,"rt");
    if (!bio) 
	{
	printf("Error loading file: %s\n", keyFile);
	return NULL;
	}

    RSA *rsa=PEM_read_bio_RSAPrivateKey(bio,NULL,NULL,NULL);
    if (!rsa) {
	printf("Error bio_reading RSA key from %s\n", keyFile);
	return NULL;
	}

    BIO_free(bio);


    return sign_and_encrypt(data,rsa,x509,ppX509,verbose);


}

