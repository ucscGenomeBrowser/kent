/* blatz client main routine.  The server does almost all the 
 * work.  This just loads DNA, sends it to the server, and
 * prints the response.  It also allows most of the server
 * alignment options to be overridden from the command line. */
/* Copyright 2005 Jim Kent.  All rights reserved. */

#include "common.h"   /* Compiler directives. Strings. Lists. */
#include "options.h"  /* Command line option handling. */
#include "errAbort.h" /* Error and warning handling. */
#include "dnautil.h"  /* Byte <-> 2 bit conversion, reverseComplement etc. */
#include "dnaseq.h"   /* Sequence, name, and size in a struct. */
#include "obscure.h"  /* Handy eclectic utility collection. */
#include "fa.h"       /* Read/write fasta sequence files. */
#include "dnaLoad.h"  /* Read write dna sequences in many formats. */
#include "net.h"      /* Network i/o, mostly TCP/IP oriented. */
#include "bzp.h"      /* Blatz parameter structure. */
#include "blatz.h"    /* Alignment routines. */

int port = bzpDefaultPort;       /* Server TCP/IP port. */
char *host = "localhost";        /* Name of computer server is running on. */

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
printf("It's important that the sequence be repeat masked with repeats in\n");
printf("lower case.\n");
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
    if (optionExists("rna") || optionExists("unmask"))
        toUpperN(seq->dna, seq->size);
    else
	{
	if (seqIsLower(seq))
	    warn("Sequence %s is all lower case, and thus ignored. Use -unmask "
	         "flag to unmask lower case sequence.", seq->name);
	}
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
