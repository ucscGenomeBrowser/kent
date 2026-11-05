/* httpsFerryLeakTest - open a udc file over https, read a little, then errAbort
 * inside an errCatch without closing the file, and sleep so the https ferry thread
 * and its socket can be inspected. */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "errCatch.h"
#include "options.h"
#include "udc.h"

void usage()
/* Explain usage and exit */
{
errAbort(
"httpsFerryLeakTest - open an https url with udc, read part of it, errAbort inside\n"
"an errCatch, then sleep so the leftover https thread and socket can be inspected.\n"
"usage:\n"
"   httpsFerryLeakTest URL\n"
"URL should be a large file (several MB) on an https server.\n"
"options:\n"
"   -readSize=N  bytes to read before the errAbort (default 4096)\n"
"   -sleep=N     seconds to sleep after the errCatch (default 180)\n"
"   -close       close the udc file before the errAbort\n");
}

static struct optionSpec options[] = {
    {"readSize", OPTION_INT},
    {"sleep", OPTION_INT},
    {"close", OPTION_BOOLEAN},
    {NULL, 0},
};

void openReadAbort(char *url, int readSize, boolean doClose)
/* Open url with udc, read readSize bytes, optionally close, then errAbort. */
{
struct udcFile *udcf = udcFileOpen(url, NULL);
char *buf = needLargeMem(readSize);
bits64 sizeRead = udcRead(udcf, buf, readSize);
fprintf(stderr, "read %llu bytes from %s\n", sizeRead, url);
freeMem(buf);
if (doClose)
    {
    udcFileClose(&udcf);
    fprintf(stderr, "closed udc file\n");
    }
errAbort("forced errAbort after reading %llu bytes", sizeRead);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
char *url = argv[1];
int readSize = optionInt("readSize", 4096);
int sleepSeconds = optionInt("sleep", 180);
boolean doClose = optionExists("close");

udcDisableCache();

struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    openReadAbort(url, readSize, doClose);
errCatchEnd(errCatch);
if (errCatch->gotError)
    fprintf(stderr, "caught: %s", errCatch->message->string);
errCatchFree(&errCatch);

fprintf(stderr, "pid %d sleeping %d seconds\n", (int)getpid(), sleepSeconds);
sleep(sleepSeconds);
fprintf(stderr, "done\n");
return 0;
}
