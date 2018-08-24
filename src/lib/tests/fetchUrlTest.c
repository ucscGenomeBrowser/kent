/* fetchUrlTest - test some stuff in net module. */

/* Copyright (C) 2010 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "options.h"
#include "dystring.h"
#include "obscure.h"
#include "net.h"


void usage()
/* Explain usage and exit */
{
errAbort(
"fetchUrlTest - try to fetch url\n"
"usage:\n"
"   fetchUrlTest URL\n");
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void fetchUrlTest(char *url)
/* Fetch given URL, send to stdout. */
{
struct dyString *dy = netSlurpUrl(url);
mustWrite(stdout, dy->string, dy->stringSize);
}

void fetchUrlBody(char *url)
/* Fetch given URL, send to stdout. */
{
int sd = netUrlOpen(url);
if (sd < 0)
    {
    errAbort("Couldn't open %s", url);
    }
char *newUrl = NULL;
int newSd = 0;
#define BUFSIZE 65536
char buf[BUFSIZE];
ssize_t readCount = 0;
if (startsWith("http://",url) || startsWith("https://",url))
    {
    if (!netSkipHttpHeaderLinesHandlingRedirect(sd, url, &newSd, &newUrl))
	{
	errAbort("Error processing http response for %s", url);
	}
    if (newUrl != NULL)
	{
	/*  Update sd with newSd, replace it with newUrl, etc. */
	sd = newSd;
	url = newUrl;
	verbose(2, "New url after redirect: %s\n", url);
	}
    }
while (TRUE)
    {
    readCount = read(sd, buf, BUFSIZE);
    if (readCount == 0)
	break;
    if (readCount < 0)
	errnoAbort("error reading from socket for url %s", url);
    mustWrite(stdout, buf, readCount);
    }
close(sd);
if (newUrl) 
    freeMem(newUrl); 
}



int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
//fetchUrlTest(argv[1]);
fetchUrlBody(argv[1]);
return 0;
}

