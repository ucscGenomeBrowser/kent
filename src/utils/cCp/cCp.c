/* cCp - copy a file to the cluster fairly efficiently. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "obscure.h"
#include "dystring.h"


static void cCpFile(char *source, char *destHost, char *destFile)
/* Execute scp command to copy source file to host. */
{
struct dyString *dy = newDyString(512);
dyStringPrintf(dy, "scp %s %s:/%s",  source, destHost, destFile);
system(dy->string);
printf("%s\n", dy->string);
freeDyString(&dy);
}

void sshSelf(char *hostList, char *host, int start, int count, char *destFile)
/* Execute ssh command to invoke self. */
{
struct dyString *dy = newDyString(512);
dyStringPrintf(dy, "ssh %s ~kent/bin/i386/cCp %s %s %s %d %d &",
	host, destFile, destFile, hostList, start, count);
uglyf("%s\n", dy->string);
system(dy->string);
freeDyString(&dy);
}

void cCp(char *hostList, int startHost, int hostCount, 
	char *sourceFile, char *destFile)
/* Do cluster copy from source file to a portion of the
 * host list. */
{
int firstHalf;
int secondHalf;
int firstSize, secondSize;
char *hostBuf;
static char **hosts;
int totalHostCount;
char *newHost;

readAllWords(hostList, &hosts, &totalHostCount, &hostBuf);
if (hostCount == 0)
    hostCount = totalHostCount;
firstHalf = startHost;
secondHalf = startHost + hostCount/2;
firstSize = secondHalf - firstHalf;
secondSize = startHost + hostCount - secondHalf;
if (firstSize > 0)
    {
    newHost = hosts[firstHalf];
    cCpFile(sourceFile, newHost, destFile);
    if (firstSize > 1)
	sshSelf(hostList, newHost, firstHalf+1, firstSize-1, destFile);
    }
if (secondSize > 0)
    {
    newHost = hosts[secondHalf];
    cCpFile(sourceFile, newHost, destFile);
    if (secondSize > 1)
	sshSelf(hostList, newHost, secondHalf+1, secondSize-1, destFile);
    }
}

void usage()
/* Explain usage and exit. */
{
errAbort(
"cCp - copy a file to cluster."
"usage:\n"
"   cCp sourceFile destFile hostList\n"
"This will copy sourceFile to destFile for all machines in\n"
"hostList\n"
"   cCp sourceFile destFile hostList start count\n"
"This will copy soourceFile to destFile for count machines\n"
"in hostList starting with start (which is zero based)\n"
"\n"
"example:\n"
"   cCp h.zip /var/tmp/h.zip newHosts");
}

int main(int argc, char *argv[])
/* Process command line. */
{
int start = 0, count = 0;
if (argc != 4 && argc != 6)
    usage();
if (argc == 6)
    {
    start = atoi(argv[4]);
    count = atoi(argv[5]);
    }
cCp(argv[3], start, count, argv[1], argv[2]);
return 0;
}

