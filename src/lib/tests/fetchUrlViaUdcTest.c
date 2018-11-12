/* fetchUrlViaUdcTest - test some stuff in net module. */

/* Copyright (C) 2010 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
//#include "errAbort.h"
#include "options.h"
#include "portable.h"
//#include "dystring.h"
//#include "obscure.h"
//#include "net.h"
#include "udc.h"

void usage()
/* Explain usage and exit */
{
errAbort(
"fetchUrlViaUdcTest - try to fetch url via udcCache\n"
"usage:\n"
"   fetchUrlViaUdcTest URL\n"
"options:\n"
"   -udcDir=path (default is ./udcCache)\n"); 
}

static struct optionSpec options[] = {
    {"udcDir", OPTION_STRING},
    {NULL, 0},
};

void udcFetch(char *url)
{
verbose(1, "udcFetch got here url=%s\n", url);

struct udcFile *udcf = udcFileOpen(url, NULL);

/* NOT IN USE YET
udcSeek(udcf, in->offset);
*/

int bufSize = 32768;
char *buf = needLargeMem(bufSize);
bits64 remaining = udcSizeFromCache(url, NULL);

while (remaining > 0)
    {
    if (bufSize > remaining)
	bufSize = remaining;
    bits64 sizeRead = udcRead(udcf, buf, bufSize);
    if (sizeRead < bufSize)
	{
	errAbort("expected %d bytes from %s, only got %llu", bufSize, url, sizeRead);
	}
    else
	{
	verbose(1, "read %llu bytes from %s\n", sizeRead, url); // DEBUG REMOVE
	mustWrite(stdout, buf, sizeRead);
	}
    remaining -= sizeRead;
    }

udcFileClose(&udcf);
}



int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();

char temp[1024];
safef(temp, sizeof temp, "%s/udcCache", getCurrentDir());
if (optionExists("udcDir"))
    safef(temp, sizeof temp, "%s", optionVal("udcDir", NULL));
verbose(1, "setting udcCache dir to: %s\n", temp);
udcSetDefaultDir(temp);   // local path in current dir for testing (turned into an absolute path)

udcFetch(argv[1]);

return 0;
}

