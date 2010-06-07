/* use bio to call openssl on https://host/url */

#include "openssl/ssl.h"
#include "openssl/err.h"

#include "bio.h"

int check_cert(SSL *ssl, char *host)
{
X509 *peer;
char peer_CN[256];

int verifyResult = SSL_get_verify_result(ssl);
if (verifyResult != X509_V_OK)
    {
    fprintf(stderr,"Certificate doesn't verify, result=%d\n", verifyResult);
    return FALSE;
    }


/*Check the cert chain. The chain length
is automatically checked by OpenSSL when
we set the verify depth in the ctx */


/*Check the common name*/

peer=SSL_get_peer_certificate(ssl);

X509_NAME_get_text_by_NID( X509_get_subject_name(peer),
 NID_commonName, peer_CN, 256);

if (strcasecmp(peer_CN,host))
    {
    fprintf(stderr,"Common name %s doesn't match host name %s\n",peer_CN,host);
    return FALSE;
    }
    
return TRUE;
}

struct dyString *bio(char *host, char *url, char *certFile, char *certPath)
/*
This SSL/TLS client example, attempts to retrieve a page from an SSL/TLS web server. 
The I/O routines are identical to those of the unencrypted example in BIO_s_connect(3).
*/
{
struct dyString *dy = dyStringNew(0);
char hostnameProto[256];
char requestLine[4096];

BIO *sbio;
int len;
char tmpbuf[1024];
SSL_CTX *ctx;
SSL *ssl;

SSL_library_init();
ERR_load_crypto_strings();
ERR_load_SSL_strings();
OpenSSL_add_all_algorithms();

/* We would seed the PRNG here if the platform didn't
* do it automatically
*/

ctx = SSL_CTX_new(SSLv23_client_method());
if (certFile || certPath)
    {
    SSL_CTX_load_verify_locations(ctx,certFile,certPath);
#if (OPENSSL_VERSION_NUMBER < 0x0090600fL)
    SSL_CTX_set_verify_depth(ctx,1);
#endif
    }

/* We'd normally set some stuff like the verify paths and
* mode here because as things stand this will connect to
* any server whose certificate is signed by any CA.
*/

sbio = BIO_new_ssl_connect(ctx);

BIO_get_ssl(sbio, &ssl);

if(!ssl) 
    {
    fprintf(stderr, "Can't locate SSL pointer\n");
    return NULL;
    }

/* Don't want any retries */
SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);

/* We might want to do other things with ssl here */

safef(hostnameProto,sizeof(hostnameProto),"%s:https",host);
BIO_set_conn_hostname(sbio, hostnameProto);

if(BIO_do_connect(sbio) <= 0) 
    {
    fprintf(stderr, "Error connecting to server\n");
    ERR_print_errors_fp(stderr);
    return NULL;
    }

if(BIO_do_handshake(sbio) <= 0) 
    {
    fprintf(stderr, "Error establishing SSL connection\n");
    ERR_print_errors_fp(stderr);
    return NULL;
    }

if (certFile || certPath)
    if (!check_cert(ssl, host))
	return NULL;

/* Could examine ssl here to get connection info */

safef(requestLine,sizeof(requestLine),"GET %s HTTP/1.0\n\n",url);
BIO_puts(sbio, requestLine);
for(;;) 
    {
    len = BIO_read(sbio, tmpbuf, 1024);
    if(len <= 0) break;
    dyStringAppendN(dy, tmpbuf, len);
    }
BIO_free_all(sbio);

return dy;
}
