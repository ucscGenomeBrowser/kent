/* Connect via https. */

/* Copyright (C) 2012 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "openssl/ssl.h"
#include "openssl/err.h"

#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#include "common.h"
#include "internet.h"
#include "errAbort.h"
#include "net.h"


static pthread_mutex_t *mutexes = NULL;
 
static unsigned long openssl_id_callback(void)
{
return ((unsigned long)pthread_self());
}
 
static void openssl_locking_callback(int mode, int n, const char * file, int line)
{
if (mode & CRYPTO_LOCK)
    pthread_mutex_lock(&mutexes[n]);
else
    pthread_mutex_unlock(&mutexes[n]);
}
 
void openssl_pthread_setup(void)
{
int i;
int numLocks = CRYPTO_num_locks();
AllocArray(mutexes, numLocks);
for (i = 0;  i < numLocks;  i++)
    pthread_mutex_init(&mutexes[i], NULL);
CRYPTO_set_id_callback(openssl_id_callback);
CRYPTO_set_locking_callback(openssl_locking_callback);
}
 

struct netConnectHttpsParams
/* params to pass to thread */
{
pthread_t thread;
int sv[2]; /* the pair of socket descriptors */
BIO *sbio;  // ssl bio
};

static void xerrno(char *msg)
{
fprintf(stderr, "%s : %s\n", strerror(errno), msg); fflush(stderr);
}

static void xerr(char *msg)
{
fprintf(stderr, "%s\n", msg); fflush(stderr);
}

void openSslInit()
/* do only once */
{
static boolean done = FALSE;
static pthread_mutex_t osiMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_lock( &osiMutex );
if (!done)
    {
    SSL_library_init();
    ERR_load_crypto_strings();
    ERR_load_SSL_strings();
    OpenSSL_add_all_algorithms();
    openssl_pthread_setup();
    done = TRUE;
    }
pthread_mutex_unlock( &osiMutex );
}


void *netConnectHttpsThread(void *threadParam)
/* use a thread to run socket back to user */
{
/* child */

struct netConnectHttpsParams *params = threadParam;

pthread_detach(params->thread);  // this thread will never join back with it's progenitor


/* we need to wait on both the user's socket and the BIO SSL socket 
 * to see if we need to ferry data from one to the other */

fd_set readfds;
fd_set writefds;

struct timeval tv;
int err;

char sbuf[32768];  // socket buffer sv[1] to user
char bbuf[32768];  // bio buffer
int srd = 0;
int swt = 0;
int brd = 0;
int bwt = 0;
int fd = 0;
while (1) 
    {

    // Do NOT move this outside the while loop. 
    /* Get underlying file descriptor, needed for select call */
    fd = BIO_get_fd(params->sbio, NULL);
    if (fd == -1) 
	{
	xerr("BIO doesn't seem to be initialized in https, unable to get descriptor.");
	goto cleanup;
	}


    FD_ZERO(&readfds);
    FD_ZERO(&writefds);

    if (brd == 0)
	FD_SET(fd, &readfds);
    if (swt < srd)
	FD_SET(fd, &writefds);
    if (srd == 0)
	FD_SET(params->sv[1], &readfds);

    tv.tv_sec = (long) (DEFAULTCONNECTTIMEOUTMSEC/1000);  // timeout default 10 seconds
    tv.tv_usec = (long) (((DEFAULTCONNECTTIMEOUTMSEC/1000)-tv.tv_sec)*1000000);

    err = select(max(fd,params->sv[1]) + 1, &readfds, &writefds, NULL, &tv);

    /* Evaluate select() return code */
    if (err < 0) 
	{
	xerr("error during select()");
	goto cleanup;
	}
    else if (err == 0) 
	{
	/* Timed out - just quit */
	xerr("https timeout expired");
	goto cleanup;
	}

    else 
	{
	if (FD_ISSET(params->sv[1], &readfds))
	    {
	    swt = 0;
	    srd = read(params->sv[1], sbuf, 32768);
	    if (srd == -1)
		{
		if (errno != 104) // udcCache often closes causing "Connection reset by peer"
		    xerrno("error reading user pipe for https socket");
		goto cleanup;
		}
	    if (srd == 0) 
		break;  // user closed socket, we are done
	    }

	if (FD_ISSET(fd, &writefds))
	    {
	    int swtx = BIO_write(params->sbio, sbuf+swt, srd-swt);
	    if (swtx <= 0)
		{
		if (!BIO_should_write(params->sbio))
		    {
		    ERR_print_errors_fp(stderr);
		    xerr("Error writing SSL connection");
		    goto cleanup;
		    }
		}
	    else
		{
		swt += swtx;
		if (swt >= srd)
		    {
		    swt = 0;
		    srd = 0;
		    }
		}
	    }

	if (FD_ISSET(fd, &readfds))
	    {
	    bwt = 0;
	    brd = BIO_read(params->sbio, bbuf, 32768);

	    if (brd <= 0)
		{
		if (BIO_should_read(params->sbio))
		    {
		    brd = 0;
		    continue;
		    }
		else
		    {
		    if (brd == 0) break;
		    ERR_print_errors_fp(stderr);
		    xerr("Error reading SSL connection");
		    goto cleanup;
		    }
		}
	    // write the https data received immediately back on socket to user, and it's ok if it blocks.
	    while(bwt < brd)
		{
		// NOTE: Intended as a thread-specific library-safe way not to ignore SIG_PIPE for the entire application.
		// temporarily block sigpipe on this thread
		sigset_t sigpipe_mask;
		sigemptyset(&sigpipe_mask);
		sigaddset(&sigpipe_mask, SIGPIPE);
		sigset_t saved_mask;
		if (pthread_sigmask(SIG_BLOCK, &sigpipe_mask, &saved_mask) == -1) {
		    perror("pthread_sigmask");
		    exit(1);
		}
		int bwtx = write(params->sv[1], bbuf+bwt, brd-bwt);
		int saveErrno = errno;
		if ((bwtx == -1) && (saveErrno == EPIPE))
		    { // if there was a EPIPE, accept and consume the SIGPIPE now.
		    int sigErr, sig;
		    if ((sigErr = sigwait(&sigpipe_mask, &sig)) != 0) 
			{
			errno = sigErr;
			perror("sigwait");
			exit(1);
			}
		    }
		// restore signal mask on this thread
		if (pthread_sigmask(SIG_SETMASK, &saved_mask, 0) == -1) 
		    {
		    perror("pthread_sigmask");
		    exit(1);
		    }
		errno = saveErrno;
		if (bwtx == -1)
		    {
		    if ((errno != 104)  // udcCache often closes causing "Connection reset by peer"
		     && (errno !=  32)) // udcCache often closes causing "Broken pipe"
			xerrno("error writing https data back to user pipe");
		    goto cleanup;
		    }
		bwt += bwtx;
		}
	    brd = 0;
	    bwt = 0;
	    }
	}
    }

cleanup:


BIO_free_all(params->sbio);  // will free entire chain of bios
close(fd);     // Needed because we use BIO_NOCLOSE above. Someday might want to re-use a connection.
close(params->sv[1]);  /* we are done with it */

return NULL;
}

int netConnectHttps(char *hostName, int port, boolean noProxy)
/* Return socket for https connection with server or -1 if error. */
{

int fd=0;
char *proxyUrl = getenv("https_proxy");

if (noProxy)
    proxyUrl = NULL;
char *connectHost;
int connectPort;

BIO *fbio=NULL;  // file descriptor bio
BIO *sbio=NULL;  // ssl bio
SSL_CTX *ctx;
SSL *ssl;

openSslInit();

ctx = SSL_CTX_new(SSLv23_client_method());

fd_set readfds;
fd_set writefds;
int err;
struct timeval tv;

// Set TRUSTED_FIRST for openssl 1.0
// Fixes common issue openssl 1.0 had with with LetsEncrypt certs in the Fall of 2021.
X509_STORE_set_flags(SSL_CTX_get_cert_store(ctx), X509_V_FLAG_TRUSTED_FIRST);

// verify peer cert of the server.
SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
if (!SSL_CTX_set_default_verify_paths(ctx)) 
    {
    warn("SSL set default verify paths failed");
    }

// Don't want any retries since we are non-blocking bio now
// This is available on newer versions of openssl
//SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);

// Support for Http Proxy
struct netParsedUrl pxy;
if (proxyUrl)
    {
    netParseUrl(proxyUrl, &pxy);
    if (!sameString(pxy.protocol, "http"))
	{
	warn("Unknown proxy protocol %s in %s. Should be http.", pxy.protocol, proxyUrl);
	goto cleanup2;
	}
    connectHost = pxy.host;
    connectPort = atoi(pxy.port);
    }
else
    {
    connectHost = hostName;
    connectPort = port;
    }
fd = netConnect(connectHost,connectPort);
if (fd == -1)
    {
    warn("netConnect() failed");
    goto cleanup2;
    }

if (proxyUrl)
    {
    char *logProxy = getenv("log_proxy");
    if (sameOk(logProxy,"on"))
	verbose(1, "CONNECT %s:%d HTTP/1.0 via %s:%d\n", hostName, port, connectHost,connectPort);
    struct dyString *dy = newDyString(512);
    dyStringPrintf(dy, "CONNECT %s:%d HTTP/1.0\r\n", hostName, port);
    setAuthorization(pxy, "Proxy-Authorization", dy);
    dyStringAppend(dy, "\r\n");
    mustWriteFd(fd, dy->string, dy->stringSize);
    dyStringFree(&dy);
    // verify response
    char *newUrl = NULL;
    boolean success = netSkipHttpHeaderLinesWithRedirect(fd, proxyUrl, &newUrl);
    if (!success) 
	{
	warn("proxy server response failed");
	goto cleanup2;
	}
    if (newUrl) /* no redirects */
	{
	warn("proxy server response should not be a redirect");
	goto cleanup2;
	}
    }

fbio=BIO_new_socket(fd,BIO_NOCLOSE);  
// BIO_NOCLOSE because we handle closing fd ourselves.
if (fbio == NULL)
    {
    warn("BIO_new_socket() failed");
    goto cleanup2;
    }
sbio = BIO_new_ssl(ctx, 1);
if (sbio == NULL) 
    {
    warn("BIO_new_ssl() failed");
    goto cleanup2;
    }
sbio = BIO_push(sbio, fbio);
BIO_get_ssl(sbio, &ssl);
if(!ssl) 
    {
    warn("Can't locate SSL pointer");
    goto cleanup2;
    }


/* 
Server Name Indication (SNI)
Required to complete tls ssl negotiation for systems which house multiple domains. (SNI)
This is common when serving HTTPS requests with a wildcard certificate (*.domain.tld).
This line will allow the ssl connection to send the hostname at tls negotiation time.
It tells the remote server which hostname the client is connecting to.
The hostname must not be an IP address.
*/ 
if (!isIpv4Address(hostName) && !isIpv6Address(hostName))
    SSL_set_tlsext_host_name(ssl,hostName);

BIO_set_nbio(sbio, 1);     /* non-blocking mode */

while (1) 
    {
    if (BIO_do_handshake(sbio) == 1) 
	{
	break;  /* Connected */
	}
    if (! BIO_should_retry(sbio)) 
	{
	//warn("BIO_do_handshake() failed");
	long unsigned bioErr = ERR_get_error();
	warn("SSL error: %s", ERR_reason_error_string(bioErr));
        if (bioErr == 336134278)  // we want these errors to appear in the CGI log, even if the default handler in gui does not report them.
	    fprintf(stderr, "SSL error: %s on %s\n", ERR_reason_error_string(bioErr), hostName);
	goto cleanup2;
	}

    fd = BIO_get_fd(sbio, NULL);
    if (fd == -1) 
	{
	warn("unable to get BIO descriptor");
	goto cleanup2;
	}
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    if (BIO_should_read(sbio)) 
	{
	FD_SET(fd, &readfds);
	}
    else if (BIO_should_write(sbio)) 
	{
	FD_SET(fd, &writefds);
	}
    else 
	{  /* BIO_should_io_special() */
	FD_SET(fd, &readfds);
	FD_SET(fd, &writefds);
	}
    tv.tv_sec = (long) (DEFAULTCONNECTTIMEOUTMSEC/1000);  // timeout default 10 seconds
    tv.tv_usec = (long) (((DEFAULTCONNECTTIMEOUTMSEC/1000)-tv.tv_sec)*1000000);

    err = select(fd + 1, &readfds, &writefds, NULL, &tv);
    if (err < 0) 
	{
	warn("select() error");
	goto cleanup2;
	}

    if (err == 0) 
	{
	warn("connection timeout to %s", hostName);
	goto cleanup2;
	}
    }

struct netConnectHttpsParams *params;
AllocVar(params);
params->sbio = sbio;

socketpair(AF_UNIX, SOCK_STREAM, 0, params->sv);

// netBlockBrokenPipes(); works, but is heavy handed 
//  and ignores SIGPIPE on all connections for all threads in the entire application. 
// We are trying something more subtle and library and thread-friendly instead.

int rc;
rc = pthread_create(&params->thread, NULL, netConnectHttpsThread, (void *)params);
if (rc)
    {
    errAbort("Unexpected error %d from pthread_create(): %s",rc,strerror(rc));
    }

return params->sv[0];

/* parent */

cleanup2:

if (sbio)
    BIO_free_all(sbio);  // will free entire chain of bios
if (fd != -1)
    close(fd);  // Needed because we use BIO_NOCLOSE above. Someday might want to re-use a connection.

return -1;

}

