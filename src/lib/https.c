/* Connect via https. */

/* Copyright (C) 2012 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "openssl/ssl.h"
#include "openssl/err.h"

#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#include "common.h"
#include "internet.h"
#include "errAbort.h"
#include "hash.h"
#include "net.h"

char *https_cert_check = "log";                 // DEFAULT certificate check is log.
char *https_cert_check_depth = "9";             // DEFAULT depth check level is 9.
char *https_cert_check_verbose = "off";         // DEFAULT verbose is off.
char *https_cert_check_domain_exceptions = "";  // DEFAULT space separated list is empty string.

char *https_proxy = NULL;
char *log_proxy = NULL;

char *SCRIPT_NAME = NULL;

// For use with callback. Set a variable into the connection itself,
// and then use that during the callback.
struct myData
    {
    char *hostName;
    };

int myDataIndex = -1;

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

void initDomainWhiteListHash();   // forward declaration

void myGetenv(char **pMySetting, char *envSetting)
/* avoid setenv which causes problems in multi-threaded programs
 * cloning the env var helps isolate it from other threads activity. */
{
char *value = getenv(envSetting);
if (value)
     *pMySetting = cloneString(value);
}

void openSslInit()
/* do only once */
{
static boolean done = FALSE;
static pthread_mutex_t osiMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_lock( &osiMutex );
if (!done)
    {
    // setenv avoided since not thread-safe
    myGetenv(&https_cert_check,                   "https_cert_check");
    myGetenv(&https_cert_check_depth,             "https_cert_check_depth");
    myGetenv(&https_cert_check_verbose,           "https_cert_check_verbose");
    myGetenv(&https_cert_check_domain_exceptions, "https_cert_check_domain_exceptions");
    myGetenv(&https_proxy, "https_proxy");
    myGetenv(&log_proxy,   "log_proxy");
    myGetenv(&SCRIPT_NAME, "SCRIPT_NAME");

    SSL_library_init();
    ERR_load_crypto_strings();
    ERR_load_SSL_strings();
    OpenSSL_add_all_algorithms();
    openssl_pthread_setup();
    myDataIndex = SSL_get_ex_new_index(0, "myDataIndex", NULL, NULL, NULL);
    initDomainWhiteListHash();
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
		int bwtx = write(params->sv[1], bbuf+bwt, brd-bwt);
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


static int verify_callback(int preverify_ok, X509_STORE_CTX *ctx)
{
char    buf[256];
X509   *cert;
int     err, depth;

struct myData *myData;
SSL    *ssl;

cert = X509_STORE_CTX_get_current_cert(ctx);
err = X509_STORE_CTX_get_error(ctx);
depth = X509_STORE_CTX_get_error_depth(ctx);

/*
* Retrieve the pointer to the SSL of the connection currently treated
* and the application specific data stored into the SSL object.
*/

X509_NAME_oneline(X509_get_subject_name(cert), buf, 256);

/*
* Catch a too long certificate chain. The depth limit set using
* SSL_CTX_set_verify_depth() is by purpose set to "limit+1" so
* that whenever the "depth>verify_depth" condition is met, we
* have violated the limit and want to log this error condition.
* We must do it here, because the CHAIN_TOO_LONG error would not
* be found explicitly; only errors introduced by cutting off the
* additional certificates would be logged.
*/

ssl = X509_STORE_CTX_get_ex_data(ctx, SSL_get_ex_data_X509_STORE_CTX_idx());
myData = SSL_get_ex_data(ssl, myDataIndex);


if (depth > atoi(https_cert_check_depth))
    {
    preverify_ok = 0;
    err = X509_V_ERR_CERT_CHAIN_TOO_LONG;
    X509_STORE_CTX_set_error(ctx, err);
    }
if (sameString(https_cert_check_verbose, "on"))
    {
    fprintf(stderr,"depth=%d:%s\n", depth, buf);
    }
if (!preverify_ok) 
    {
    if (SCRIPT_NAME)  // CGI mode
	{
	fprintf(stderr, "verify error:num=%d:%s:depth=%d:%s hostName=%s CGI=%s\n", err,
	    X509_verify_cert_error_string(err), depth, buf, myData->hostName, SCRIPT_NAME);
	}
    if (!sameString(https_cert_check, "log"))
	{
	char *cn = strstr(buf, "/CN=");
	if (cn) cn+=4;  // strlen /CN=
	if (sameString(cn, myData->hostName))
	    warn("%s on %s", X509_verify_cert_error_string(err), cn);
	else
	    warn("%s on %s (%s)", X509_verify_cert_error_string(err), cn, myData->hostName);
	}
    }
/* err contains the last verification error.  */
if (!preverify_ok && (err == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT))
    {
    X509_NAME_oneline(X509_get_issuer_name(cert), buf, 256);
    fprintf(stderr, "issuer= %s\n", buf);
    }
if (sameString(https_cert_check, "warn") || sameString(https_cert_check, "log"))
    return 1;
else
    return preverify_ok;
}

struct hash *domainWhiteList = NULL;

void initDomainWhiteListHash()
/* Initialize once, has all the old existing domains 
 * for which cert checking is skipped since they are not compatible (yet) with openssl.*/
{

domainWhiteList = hashNew(8);

// whitelisted domain exceptions set in hg.conf
// space separated list.
char *dmwl = cloneString(https_cert_check_domain_exceptions);
int wordCount = chopByWhite(dmwl, NULL, 0);
if (wordCount > 0)
    {
    char **words;
    AllocArray(words, wordCount);
    chopByWhite(dmwl, words, wordCount);
    int w;
    for(w=0; w < wordCount; w++)
	{
	hashStoreName(domainWhiteList, words[w]);
	}
    freeMem(words);
    }
freez(&dmwl);

// useful for testing, turns off hardwired whitelist exceptions
if (!hashLookup(domainWhiteList, "noHardwiredExceptions"))  
    {
    // Hardwired exceptions whitelist
    // openssl automatically whitelists domains which are given as IPv4 or IPv6 addresses
    hashStoreName(domainWhiteList, "*.altius.org");
    hashStoreName(domainWhiteList, "*.apps.wistar.org");
    hashStoreName(domainWhiteList, "*.bio.ed.ac.uk");
    hashStoreName(domainWhiteList, "*.cbu.uib.no");
    hashStoreName(domainWhiteList, "*.clinic.cat");
    hashStoreName(domainWhiteList, "*.crg.eu");
    hashStoreName(domainWhiteList, "*.dwf.go.th");
    hashStoreName(domainWhiteList, "*.ezproxy.u-pec.fr");
    hashStoreName(domainWhiteList, "*.genebook.com.cn");
    hashStoreName(domainWhiteList, "*.jncasr.ac.in");
    hashStoreName(domainWhiteList, "*.sund.ku.dk");
    hashStoreName(domainWhiteList, "*.wistar.upenn.edu");
    hashStoreName(domainWhiteList, "52128.bham.ac.uk");
    hashStoreName(domainWhiteList, "animalgenomeinstitute.org");
    hashStoreName(domainWhiteList, "annotation.dbi.udel.edu");
    hashStoreName(domainWhiteList, "arn.ugr.es");
    hashStoreName(domainWhiteList, "bic2.ibi.upenn.edu");
    hashStoreName(domainWhiteList, "bifx-core3.bio.ed.ac.uk");
    hashStoreName(domainWhiteList, "biodb.kaist.ac.kr");
    hashStoreName(domainWhiteList, "bioinfo.gwdg.de");
    hashStoreName(domainWhiteList, "bioinfo2.ugr.es");
    hashStoreName(domainWhiteList, "bioinfo5.ugr.es");
    hashStoreName(domainWhiteList, "bioshare.genomecenter.ucdavis.edu");
    hashStoreName(domainWhiteList, "biowebport.com");
    hashStoreName(domainWhiteList, "braincode.bwh.harvard.edu");
    hashStoreName(domainWhiteList, "bricweb.sund.ku.dk");
    hashStoreName(domainWhiteList, "bx.bio.jhu.edu");
    hashStoreName(domainWhiteList, "ccg.epfl.ch");
    hashStoreName(domainWhiteList, "cctop.cos.uni-heidelberg.de");
    hashStoreName(domainWhiteList, "cell-innovation.nig.ac.jp");
    hashStoreName(domainWhiteList, "chopchop.cbu.uib.no.");
    hashStoreName(domainWhiteList, "cluster.hpcc.ucr.edu");
    hashStoreName(domainWhiteList, "coppolalab.ucla.edu");
    hashStoreName(domainWhiteList, "costalab.ukaachen.de");
    hashStoreName(domainWhiteList, "cvmfs-hubs.vhost38.genap.ca");
    hashStoreName(domainWhiteList, "data.rc.fas.harvard.edu");
    hashStoreName(domainWhiteList, "datahub-7ak6xof0.udes.genap.ca");
    hashStoreName(domainWhiteList, "datahub-7mu6z13t.udes.genap.ca");
    hashStoreName(domainWhiteList, "datahub-bx3mvzla.udes.genap.ca");
    hashStoreName(domainWhiteList, "datahub-gvhsc2p7.udes.genap.ca");
    hashStoreName(domainWhiteList, "datahub-i8kms5wt.udes.genap.ca");
    hashStoreName(domainWhiteList, "datahub-kazb7g4u.udes.genap.ca");
    hashStoreName(domainWhiteList, "datahub-nyt53rix.udes.genap.ca");
    hashStoreName(domainWhiteList, "datahub-ruigbdoq.udes.genap.ca");
    hashStoreName(domainWhiteList, "dev.stanford.edu");
    hashStoreName(domainWhiteList, "dev.herv.img.cas.cz");
    hashStoreName(domainWhiteList, "dianalab.e-ce.uth.gr");
    hashStoreName(domainWhiteList, "dice-green.liai.org");
    hashStoreName(domainWhiteList, "dropbox.ogic.ca");
    hashStoreName(domainWhiteList, "dropfile.hpc.qmul.ac.uk");
    hashStoreName(domainWhiteList, "edn.som.umaryland.edu");
    hashStoreName(domainWhiteList, "edbc.org");
    hashStoreName(domainWhiteList, "egg2.wustl.edu");
    hashStoreName(domainWhiteList, "encode-public.s3.amazonaws.com");
    hashStoreName(domainWhiteList, "epd.epfl.ch");
    hashStoreName(domainWhiteList, "expiereddnsmanager.com");
    hashStoreName(domainWhiteList, "export.uppmax.uu.se");
    hashStoreName(domainWhiteList, "flu-infection.vhost38.genap.ca");
    hashStoreName(domainWhiteList, "frigg.uio.no");
    hashStoreName(domainWhiteList, "ftp--ncbi--nlm--nih--gov.ibrowse.co");
    hashStoreName(domainWhiteList, "ftp.science.ru.nl");
    hashStoreName(domainWhiteList, "functionalgenomics.upf.edu");
    hashStoreName(domainWhiteList, "galaxy.gred-clermont.fr");
    hashStoreName(domainWhiteList, "galaxy.igred.fr");
    hashStoreName(domainWhiteList, "galaxy.med.uvm.edu");
    hashStoreName(domainWhiteList, "garfield.igh.cnrs.fr");
    hashStoreName(domainWhiteList, "gcp.wenglab.org");
    hashStoreName(domainWhiteList, "genap.ca");
    hashStoreName(domainWhiteList, "genemo.ucsd.edu");
    hashStoreName(domainWhiteList, "genome-tracks.ngs.omrf.in");
    hashStoreName(domainWhiteList, "genomics.virus.kyoto-u.ac.jp");
    hashStoreName(domainWhiteList, "genomicsdata.cs.ucl.ac.uk");
    hashStoreName(domainWhiteList, "gsmplot.deqiangsun.org");
    hashStoreName(domainWhiteList, "gwdu100.gwdg.de");
    hashStoreName(domainWhiteList, "hgdownload--soe--ucsc--edu.ibrowse.co");
    hashStoreName(domainWhiteList, "herv.img.cas.cz");
    hashStoreName(domainWhiteList, "hci-bio-app.hci.utah.edu");
    hashStoreName(domainWhiteList, "hkgateway.med.umich.edu");
    hashStoreName(domainWhiteList, "hilbert.bio.ifi.lmu.de");
    hashStoreName(domainWhiteList, "hiview.case.edu");
    hashStoreName(domainWhiteList, "hpc.bmrn.com");
    hashStoreName(domainWhiteList, "hsb.upf.edu");
    hashStoreName(domainWhiteList, "icbi.at");
    hashStoreName(domainWhiteList, "jadhavserver.usc.edu");
    hashStoreName(domainWhiteList, "kbm7.genomebrowser.cemm.at");
    hashStoreName(domainWhiteList, "kbm7.genomebrowser.cemm.at");
    hashStoreName(domainWhiteList, "key2hair.com");
    hashStoreName(domainWhiteList, "ki-data.mit.edu");
    hashStoreName(domainWhiteList, "lichtlab.cancer.ufl.edu");
    hashStoreName(domainWhiteList, "lyncoffee.cafe24.com");
    hashStoreName(domainWhiteList, "lvgsrv1.epfl.ch");
    hashStoreName(domainWhiteList, "mariottigenomicslab.bio.ub.edu");
    hashStoreName(domainWhiteList, "manticore.niehs.nih.gov");
    hashStoreName(domainWhiteList, "metamorf.hb.univ-amu.fr");
    hashStoreName(domainWhiteList, "medinfo.hebeu.edu.cn");
    hashStoreName(domainWhiteList, "microb215.med.upenn.edu");
    hashStoreName(domainWhiteList, "mitranscriptome.org");
    hashStoreName(domainWhiteList, "mitranscriptome.path.med.umich.edu");
    hashStoreName(domainWhiteList, "nextgen.izkf.rwth-aachen.de");
    hashStoreName(domainWhiteList, "nucleus.ics.hut.fi");
    hashStoreName(domainWhiteList, "oculargenomics.meei.harvard.edu");
    hashStoreName(domainWhiteList, "omics.bioch.ox.ac.uk");
    hashStoreName(domainWhiteList, "onesgateway.med.umich.edu");
    hashStoreName(domainWhiteList, "openslice.fenyolab.org");
    hashStoreName(domainWhiteList, "orio.niehs.nih.gov");
    hashStoreName(domainWhiteList, "peromyscus.rc.fas.harvard.edu");
    hashStoreName(domainWhiteList, "pgv19.virol.ucl.ac.uk");
    hashStoreName(domainWhiteList, "pricenas.biochem.uiowa.edu");
    hashStoreName(domainWhiteList, "redirect.medsch.ucla.edu");
    hashStoreName(domainWhiteList, "rewrite.bcgsc.ca");
    hashStoreName(domainWhiteList, "rnaseqhub.brain.mpg.de");
    hashStoreName(domainWhiteList, "rsousaluis.co.uk");
    hashStoreName(domainWhiteList, "ruoho.uta.fi");
    hashStoreName(domainWhiteList, "sbwdev.stanford.edu");
    hashStoreName(domainWhiteList, "schatzlabucscdata.yalespace.org.s3.amazonaws.com");
    hashStoreName(domainWhiteList, "share.ics.aalto.fi");
    hashStoreName(domainWhiteList, "sharing.biotec.tu-dresden.de");
    hashStoreName(domainWhiteList, "sheba-cancer.org.il");
    hashStoreName(domainWhiteList, "si-ru.kr");
    hashStoreName(domainWhiteList, "siemensservices.com");
    hashStoreName(domainWhiteList, "silo.bioinf.uni-leipzig.de");
    hashStoreName(domainWhiteList, "snpinfo.niehs.nih.gov");
    hashStoreName(domainWhiteList, "spades.cgi.bch.uconn.edu");
    hashStoreName(domainWhiteList, "stark.imp.ac.at");
    hashStoreName(domainWhiteList, "starklab.org");
    hashStoreName(domainWhiteList, "swaruplab.bio.uci.edu");
    hashStoreName(domainWhiteList, "synology.com");
    hashStoreName(domainWhiteList, "track-hub.scicore-dmz.lan");
    hashStoreName(domainWhiteList, "trackhub.pnri.org");
    hashStoreName(domainWhiteList, "transcrispr.igcz.poznan.pl");
    hashStoreName(domainWhiteList, "ucsc-track-hubs.scicore.unibas.ch");
    hashStoreName(domainWhiteList, "unibind.uio.no");
    hashStoreName(domainWhiteList, "unibind2018.uio.no");
    hashStoreName(domainWhiteList, "unibind2021.uio.no");
    hashStoreName(domainWhiteList, "v91rc2.master.demo.encodedcc.org");
    hashStoreName(domainWhiteList, "v91rc3.master.demo.encodedcc.org");
    hashStoreName(domainWhiteList, "v94.rc2.demo.encodedcc.org");
    hashStoreName(domainWhiteList, "verjo103.butantan.gov.br");
    hashStoreName(domainWhiteList, "virtlehre.informatik.uni-leipzig.de");
    hashStoreName(domainWhiteList, "vm-galaxy-prod.toulouse.inra.fr");
    hashStoreName(domainWhiteList, "vm10-dn4.qub.ac.uk");
    hashStoreName(domainWhiteList, "webdisk.rsousaluis.co.uk");
    hashStoreName(domainWhiteList, "www.animalgenomeinstitute.org");
    hashStoreName(domainWhiteList, "www.bio.ifi.lmu.de");
    hashStoreName(domainWhiteList, "www.datadepot.rcac.purdue.edu");
    hashStoreName(domainWhiteList, "www.edbc.org");
    hashStoreName(domainWhiteList, "www.encodeproject.org");
    hashStoreName(domainWhiteList, "www.epigenomes.ca");
    hashStoreName(domainWhiteList, "www.healthstoriesonline.com");
    hashStoreName(domainWhiteList, "www.isical.ac.in");
    hashStoreName(domainWhiteList, "www.morgridge.net");
    hashStoreName(domainWhiteList, "www.morgridge.us");
    hashStoreName(domainWhiteList, "www.ogic.ca");
    hashStoreName(domainWhiteList, "www.picb.ac.cn");
    hashStoreName(domainWhiteList, "www.sagatenergy.kz");
    hashStoreName(domainWhiteList, "www.starklab.org");
    hashStoreName(domainWhiteList, "www.v93rc2.demo.encodedcc.org");
    hashStoreName(domainWhiteList, "xinglabtrackhub.research.chop.edu");
    hashStoreName(domainWhiteList, "ydna-warehouse.org");
    hashStoreName(domainWhiteList, "yoda.ust.hk");
    hashStoreName(domainWhiteList, "zdzlab.einsteinmed.edu");
    hashStoreName(domainWhiteList, "zlab-trackhub.umassmed.edu");
    hashStoreName(domainWhiteList, "zlab.umassmed.edu");
    }

}

struct hashEl *checkIfInHashWithWildCard(char *hostName)
/* check if in hash, and if in hash with lowest-level domain set to "*" wildcard */
{
struct hashEl *result = hashLookup(domainWhiteList, hostName);
if (!result)
    {
    char *dot = strchr(hostName, '.');
    if (dot && (dot - hostName) >= 1)
	{
        int length=strlen(hostName)+1;
	char wildHost[length];
	safef(wildHost, sizeof wildHost, "*%s", dot);
	result = hashLookup(domainWhiteList, wildHost);
	}
    }
return result;
}

int netConnectHttps(char *hostName, int port, boolean noProxy)
/* Return socket for https connection with server or -1 if error. */
{

int fd=0;

// https_cert_check env var can be abort warn or none.

char *connectHost;
int connectPort;

BIO *fbio=NULL;  // file descriptor bio
BIO *sbio=NULL;  // ssl bio
SSL_CTX *ctx;
SSL *ssl;

openSslInit();   // call early since it initializes vars from env vars in a thread-safe way.

char *proxyUrl = https_proxy;

if (noProxy)
    proxyUrl = NULL;

ctx = SSL_CTX_new(SSLv23_client_method());

fd_set readfds;
fd_set writefds;
int err;
struct timeval tv;

struct myData myData;
boolean doSetMyData = FALSE;

if (!sameString(https_cert_check, "none"))
    {
    if (checkIfInHashWithWildCard(hostName))
	{
	// old existing domains which are not (yet) compatible with openssl.
	if (SCRIPT_NAME)  // CGI mode
	    {
	    fprintf(stderr, "domain %s cert check skipped because it is white-listed as an exception.\n", hostName);
	    }
	}
    else
	{

	// verify peer cert of the server.
	// Set TRUSTED_FIRST for openssl 1.0
	// Fixes common issue openssl 1.0 had with with LetsEncrypt certs in the Fall of 2021.
	X509_STORE_set_flags(SSL_CTX_get_cert_store(ctx), X509_V_FLAG_TRUSTED_FIRST);

        // This flag causes intermediate certificates in the trust store to be treated as trust-anchors, in the same way as the self-signed root CA certificates. 
        // This makes it possible to trust certificates issued by an intermediate CA without having to trust its ancestor root CA.
        // GNU-TLS uses it, and openssl probably will do it in the future. 
        // Currently this does not fix any of our known issues with users servers certs.
	// X509_STORE_set_flags(SSL_CTX_get_cert_store(ctx), X509_V_FLAG_PARTIAL_CHAIN);

	// verify_callback gets called once per certificate returned by the server.
	SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, verify_callback);

	/*
	 * Let the verify_callback catch the verify_depth error so that we get
	 * an appropriate error in the logfile.
	 */
	SSL_CTX_set_verify_depth(ctx, atoi(https_cert_check_depth) + 1);

	// VITAL FOR PROPER VERIFICATION OF CERTS
	if (!SSL_CTX_set_default_verify_paths(ctx)) 
	    {
	    warn("SSL set default verify paths failed");
	    }

	// add the hostName to the structure and set it here, making it available during callback.
	myData.hostName = hostName;
	doSetMyData = TRUE;

	} 
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
    if (sameOk(log_proxy,"on"))
	verbose(1, "CONNECT %s:%d HTTP/1.0 via %s:%d\n", hostName, port, connectHost,connectPort);
    struct dyString *dy = dyStringNew(512);
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

if (doSetMyData)
    SSL_set_ex_data(ssl, myDataIndex, &myData);

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
	//BIO_do_handshake() failed
	warn("SSL error: %s", ERR_reason_error_string(ERR_get_error()));
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

netBlockBrokenPipes();
// we had a version that was more sophisticated about blocking only the current thread,
// but it only worked for Linux, and fixing it for MacOS would futher increase complexity with little benefit.
// SIGPIPE is often more of a hassle than a help in may cases, so we can just ignore it.

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

