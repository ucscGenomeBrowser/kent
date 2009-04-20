
#ifdef USE_SSL

#include "openssl/ssl.h"
#include "openssl/err.h"

#include <sys/socket.h>
#include <unistd.h>

#include "errabort.h"

int netMustConnectHttps(char *hostName, int port)
/* Start https connection with server or die. */
{

fflush(stdin);
fflush(stdout);
fflush(stderr);

int sv[2]; /* the pair of socket descriptors */

socketpair(AF_UNIX, SOCK_STREAM, 0, sv);

int pid = fork();

if (pid < 0)
    errnoAbort("can't fork in netMustConnectHttps");
if (pid == 0)
    {
    /* child */

    fclose(stdin);
    fclose(stdout);

    close(sv[0]);  /* close unused half of pipe */

    char hostnameProto[256];

    BIO *sbio;
    int len;
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

    /* future extension: checking certificates 

    char *certFile = NULL;
    char *certPath = NULL;
    if (certFile || certPath)
	{
	SSL_CTX_load_verify_locations(ctx,certFile,certPath);
    #if (OPENSSL_VERSION_NUMBER < 0x0090600fL)
	SSL_CTX_set_verify_depth(ctx,1);
    #endif
	}

    */

    /* We'd normally set some stuff like the verify paths and
    * mode here because as things stand this will connect to
    * any server whose certificate is signed by any CA.
    */

    sbio = BIO_new_ssl_connect(ctx);

    BIO_get_ssl(sbio, &ssl);

    if(!ssl) 
	{
	errAbort("Can't locate SSL pointer\n");
	return -1; 
	}

    /* Don't want any retries */
    SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);

    /* We might want to do other things with ssl here */

    snprintf(hostnameProto,sizeof(hostnameProto),"%s:https",hostName);
    BIO_set_conn_hostname(sbio, hostnameProto);
    BIO_set_conn_int_port(sbio, &port);

    if(BIO_do_connect(sbio) <= 0) 
	{
	ERR_print_errors_fp(stderr);
	errAbort("Error connecting to server\n");
	return -1;
	}

    if(BIO_do_handshake(sbio) <= 0) 
	{
	ERR_print_errors_fp(stderr);
	errAbort("Error establishing SSL connection\n");
	return -1;
	}

    /* future extension: checking certificates 

    if (certFile || certPath)
	if (!check_cert(ssl, host))
	    return -1;

    */

    /* Could examine ssl here to get connection info */

    char buf[32768];
    int rd = 0;

    while((rd = read(sv[1], buf, 32768)) > 0) 
	{
	if(BIO_write(sbio, buf, rd) <= 0) 
	    {
	    ERR_print_errors_fp(stderr);
	    errAbort("Error writing SSL connection\n");
	    return -1;
	    }

        // TODO may someday need to readywait on both connections
        break;   // for now, just get input once and move on
        
	}
    if (rd == -1)
	errnoAbort("error reading https socket");

    for(;;) 
	{
	len = BIO_read(sbio, buf, 32768);
	if(len < 0) 
	    {
	    ERR_print_errors_fp(stderr);
	    errAbort("Error reading SSL connection\n");
	    return -1;
	    }
	if(len == 0) break;
	int wt = write(sv[1], buf, len);
	if (wt == -1)
	    errnoAbort("error writing https socket");
	}

    BIO_free_all(sbio);
    close(sv[1]);  /* being safe */

    exit(0);

    /* child will never get to here */
    }

/* parent */

close(sv[1]);  /* close unused half of socket */

return sv[0];

}

#else

#include <stdarg.h>
#include "errabort.h"

int netMustConnectHttps(char *hostName, int port)
/* Start https connection with server or die. */
{

errnoAbort("No openssl available in netMustConnectHttps for %s : %d", hostName, port);

return -1;   /* will never get to here, make compiler happy */

}

#endif
