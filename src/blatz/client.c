/* blatz client main routine.  The server does almost all the 
 * work.  This just loads DNA, send it to the server, and
 * prints the response.  It also allows most of the server
 * alignment options to be overridden from the command line. */
/* Copyright 2005 Jim Kent.  All rights reserved. */

#include "common.h"
#include "options.h"
#include "errabort.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "obscure.h"
#include "fa.h"
#include "dnaLoad.h"
#include "net.h"
#include "bzp.h"
#include "blatz.h"

int port = bzpDefaultPort;
char *host = "localhost";

static void usage()
/* Explain usage and exit. */
{
struct bzp *bzp = bzpDefault();
printf("blatzClient version %d - Ask server to do\n", bzpVersion());
printf("cross-species DNA alignments and save results.\n");
printf("usage:\n");
printf("   blatzClient queryFile outputFile.\n");
printf("The queryFile can be in fasta, nib, or 2bit format or a \n");
printf("text file containing the names of the above files one per line.\n");
printf("Options: (defaults are shown for numerical parameters)\n");
bzpClientOptionsHelp(bzp);
printf("  -port=%d Use specified TCP/IP port\n", bzpDefaultPort);
printf("  -host=%s Query specified host\n", host);
noWarnAbort();
}

static struct optionSpec options[] = {
   BZP_CLIENT_OPTIONS
   {"host", OPTION_STRING},
   {"port", OPTION_INT},
   {NULL, 0},
};


static void sendOption(int sd, char *option)
/* If option exists send it down socket. */
{
if (optionExists(option))
    {
    char buf[256];
    safef(buf, sizeof(buf), "%s %s", option, optionVal(option, NULL));
    netSendString(sd, buf);
    }
}

static void blatzClient(char *input, char *output)
/* Send query message and dna to server and print result. */
{
struct dnaLoad *dl = dnaLoadOpen(input);
struct dnaSeq *seq;
FILE *f = mustOpen(output, "w");
static struct optionSpec options[] = {
   BZP_CLIENT_OPTIONS
};
int i;
while ((seq = dnaLoadNext(dl)) != NULL)
    {
    /* Connect */
    int sd = netMustConnect(host, port);
    FILE *sf = NULL;

    /* Send query command. */
    netSendString(sd, "query");

    /* Send options. */
    for (i=0; i<ArraySize(options); ++i)
        sendOption(sd, options[i].name);

    /* Send sequence. */
    netSendString(sd, "seq");
    netSendString(sd, seq->name);
    netSendHugeString(sd, seq->dna);
    verbose(1, "%s\n", seq->name);
    dnaSeqFree(&seq);

    /* Get and save response. */
    sf = netFileFromSocket(sd);
    copyOpenFile(sf, f);
    carefulClose(&sf);

    /* Close connection */
    close(sd);
    }
dnaLoadClose(&dl);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *command;
bzpTime(NULL);
dnaUtilOpen();
optionInit(&argc, argv, options);
port = optionInt("port", port);
host = optionVal("host", host);
if (argc != 3)
    usage();
blatzClient(argv[1], argv[2]);
return 0;
}
