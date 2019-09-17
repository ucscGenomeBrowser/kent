/* blatz server main routine. */

#include "common.h"
#include "options.h"
#include "errAbort.h"
#include "memalloc.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "chain.h"
#include "obscure.h"
#include "fa.h"
#include "dnaLoad.h"
#include "log.h"
#include "net.h"
#include "portable.h"
#include "bzp.h"
#include "blatz.h"
#include <sys/wait.h>

int port = bzpDefaultPort;
int cpuCount = 2;
char *host = "localhost";

struct cidr *subnet = NULL;

void usage()
/* Explain usage and exit. */
{
struct bzp *bzp = bzpDefault();
printf("blatzServer version %d - Set up in-memory server for\n", bzpVersion());
printf("cross-species DNA alignments\n");
printf("usage:\n");
printf("   blatzServer start file(s)\n");
printf("Starts up server. Files are either fasta files, nib files, 2bit files\n");
printf("or text files containing the names of the above files one per line.\n");
printf("It's important that the sequence be repeat masked with repeats in\n");
printf("lower case.\n");
printf("Options: (defaults are shown for numerical parameters)\n");
bzpServerOptionsHelp(bzp);
bzpClientOptionsHelp(bzp);
printf("  -debug Writes diagnostic output to console and no daemon fork\n");
printf("  -subnet=255.255.255.255 Restrict access to subnet. \n");
printf("    Supports comma-separated list of IPv4 or IPv6 subnets in CIDR notation, e.g. 255.255.255.255/24\n");
printf("  -port=%d Use specified TCP/IP port\n", bzpDefaultPort);
printf("  -host=%s Query specified host\n", host);
printf("  -cpu=%d Use specified number of CPUs (processes)\n", cpuCount);
printf("Other uses:\n");
printf("   blatzServer stop\n");
printf("      this terminates server\n");
printf("   blatzServer status\n");
printf("      this prints status info including version\n");
noWarnAbort();
}

static struct optionSpec options[] = {
   BZP_SERVER_OPTIONS
   BZP_CLIENT_OPTIONS
   {"debug", OPTION_BOOLEAN},
   {"subnet", OPTION_STRING},
   {"host", OPTION_STRING},
   {"port", OPTION_INT},
   {"cpu", OPTION_INT},
   {NULL, 0},
};

int goodQueryCount = 0;
int badQueryCount = 0;
int crashCount = 0;
int cpusUsed = 0;

void truncatedQuery(int where)
/* Log truncated message and bump counter. */
{
errAbort("truncated query %d", where);
}

void queryResponse(int sd, struct bzp *bzp, struct blatzIndex *indexList)
/* Respond to query message - read options and dna from socket,
 * and do alignment. */
{
struct bzp lbzp = *bzp;
struct dnaSeq *seq = NULL;
char buf[256], *line, *word;
char *out = NULL, *mafT = NULL, *mafQ = NULL;

/* First get options - overriding what got set at startup. */
for (;;)
    {
    if ((line = netGetString(sd, buf)) == NULL)
         {
         truncatedQuery(1);
         return;
         }
    word = nextWord(&line);
    line = skipLeadingSpaces(line);
    if (sameString(word, "seq"))
        break;
    else if (sameString(word, "rna"))
       lbzp.rna = TRUE;
    else if (sameString(word, "minScore"))
       lbzp.minScore = atoi(line);
    else if (sameString(word, "minGapless"))
       lbzp.minGapless = atoi(line);
    else if (sameString(word, "multiHits"))
       lbzp.multiHits = atoi(line);
    else if (sameString(word, "minChain"))
       lbzp.minChain = atoi(line);
    else if (sameString(word, "maxExtend"))
       lbzp.maxExtend = atoi(line);
    else if (sameString(word, "maxBandGap"))
       lbzp.maxBandGap = atoi(line);
    else if (sameString(word, "minExpand"))
       lbzp.minExpand = atoi(line);
    else if (sameString(word, "expandWindow"))
       lbzp.expandWindow = atoi(line);
    else if (sameString(word, "out"))
       lbzp.out = out = cloneString(line);
    else if (sameString(word, "mafQ"))
       lbzp.mafQ = mafQ = cloneString(line);
    else if (sameString(word, "mafT"))
       lbzp.mafT = mafT = cloneString(line);
    }

/* Get DNA into seq*/
    {
    char *name = netGetString(sd, buf);
    char *dna;
    if (name == NULL)
        {
        truncatedQuery(2);
        return;
        }
    dna = netGetHugeString(sd);
    if (dna == NULL)
        {
        truncatedQuery(3);
        return;
        }
    AllocVar(seq);
    seq->dna = dna;
    seq->size = strlen(dna);
    seq->name = cloneString(name);
    bzpTime("Received %d bases in %s", seq->size, seq->name);
    if (lbzp.rna)
        maskTailPolyA(seq->dna, seq->size);
    }

/* Create alignments into chainList and write results. */
    {
    FILE *f = netFileFromSocket(sd);
    struct chain *chainList = blatzAlign(&lbzp, indexList, seq);
    blatzWriteChains(&lbzp, &chainList, seq, 0, seq->size, seq->size, indexList, f);
    bzpTime("sent result - %d chains", slCount(chainList));
    carefulClose(&f);
    }

dnaSeqFree(&seq);
freez(&out);
freez(&mafQ);
freez(&mafT);
}

long long basesInIndexList(struct blatzIndex *indexList)
/* Return number of bases in index list. */
{
long long total = 0;
struct blatzIndex *index;
for (index = indexList; index != NULL; index = index->next)
    total += index->target->size;
return total;
}

void statusResponse(int sd, struct bzp *bzp, struct blatzIndex *indexList)
/* Respond to status message. */
{
FILE *f = netFileFromSocket(sd);

fprintf(f, "version: %d\n", bzpVersion());
fprintf(f, "indexed sequences: %d\n", slCount(indexList));
fprintf(f, "bases indexed: %lld\n", basesInIndexList(indexList));
fprintf(f, "weight: %d\n", bzp->weight);
fprintf(f, "rna: %s\n", trueFalseString(bzp->rna));
fprintf(f, "minScore: %d\n", bzp->minScore);
fprintf(f, "minGapless: %d\n", bzp->minGapless);
fprintf(f, "multiHits: %d\n", bzp->multiHits);
fprintf(f, "minChain: %d\n", bzp->minChain);
fprintf(f, "maxExtend: %d\n", bzp->maxExtend);
fprintf(f, "maxBandGap: %d\n", bzp->maxBandGap);
fprintf(f, "minExpand: %d\n", bzp->minExpand);
fprintf(f, "expandWindow: %d\n", bzp->expandWindow);
fprintf(f, "out: %s\n", bzp->out);
fprintf(f, "good queries: %d\n", goodQueryCount);
fprintf(f, "bad queries: %d\n", badQueryCount);
fprintf(f, "crashes: %d\n", crashCount);
fprintf(f, "cpus used: %d\n", cpusUsed);
carefulClose(&f);
}

void serviceLoop(int acceptor, struct bzp *bzp, struct blatzIndex *indexList)
/* Wait for requests on acceptor socket. */
{
for (;;)
    {
    int sd;
    char buf[256];
    char *command;

    /* Wait for previous processes */
        {
        int options = 0;
        int w, status;
        for (;;)
            {
            if (cpusUsed < cpuCount)
                options = WNOHANG;
            w = waitpid(-1, &status, options);
            if (w > 0)
                {
                --cpusUsed;
                if (!WIFEXITED(status))
                    ++crashCount;
                else if (WEXITSTATUS(status) != 0)
                    ++badQueryCount;
                else
                    ++goodQueryCount;
                }
            else
                break;
            }
        }

    sd = netAcceptFrom(acceptor, subnet);
    bzpTime("Since last query");
    command = netGetString(sd, buf);
    if (command != NULL)
        {
        verbose(2, "servicing %s\n", command);
        if (sameString(command, "stop"))
            break;
        else
            {
            int pid;
            ++cpusUsed;
            pid = mustFork();
            if (pid == 0)
                {
                if (command != NULL)
                    {
                    if (sameString(command, "status"))
                        statusResponse(sd, bzp, indexList);
                    else if (sameString(command, "query"))
                        queryResponse(sd, bzp, indexList);
                    }
                exit(0);
                }
            }
        }
    close(sd);
    }
}

void serverStart(char *files[], int fileCount)
/* Load DNA. Build up indexes, set up listing port, and fall into
 * accept loop. */
{
struct blatzIndex *indexList = NULL;
int i;
int acceptor;
struct bzp *bzp = bzpDefault();

/* Daemonize self. */
bzpSetOptions(bzp);

/* Load up all sequences. */
for (i=0; i<fileCount; ++i)
    {
    struct dnaLoad *dl = dnaLoadOpen(files[i]);
    struct blatzIndex *oneList = blatzIndexDl(dl, bzp->weight, bzp->unmask);
    indexList = slCat(indexList, oneList);
    dnaLoadClose(&dl);
    }
bzpTime("Loaded and indexed %d sequences", slCount(indexList));
verbose(1, "Ready for queries\n");

/* Turn self into proper daemon. */
logDaemonize("blatzServer");

acceptor = netAcceptingSocket(port, 100);
serviceLoop(acceptor, bzp, indexList);
}

void serverStop()
/* Send stop message to server. */
{
int sd = netMustConnect(host, port);
netSendString(sd, "stop");
}

void serverStatus()
/* Send status message to server and print result. */
{
int sd = netMustConnect(host, port);
FILE *f;
netSendString(sd, "status");
f = netFileFromSocket(sd);
copyOpenFile(f, stdout);
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *command;
bzpTime(NULL);
dnaUtilOpen();
setMaxAlloc(2LL*1024LL*1024LL*1024LL);
optionInit(&argc, argv, options);
port = optionInt("port", port);
host = optionVal("host", host);
subnet = internetParseSubnetCidr(optionVal("subnet", NULL));
cpuCount = optionInt("cpu", cpuCount);
if (argc < 2)
    usage();
command = argv[1];
if (sameWord(command, "start"))
    {
    if (argc < 3)
        usage();
    serverStart(argv+2, argc-2);
    }
else if (sameWord(command, "stop"))
    {
    serverStop();
    }
else if (sameWord(command, "status"))
    {
    serverStatus();
    }
else
    usage();
return 0;
}
