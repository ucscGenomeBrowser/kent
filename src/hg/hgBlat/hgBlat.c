/* hgSeqSearch - CGI-script to manage fast human genome sequence searching. */
#include "common.h"
#include "hCommon.h"
#include "portable.h"
#include "linefile.h"
#include "dnautil.h"
#include "fa.h"
#include "psl.h"
#include "genoFind.h"
#include "cheapcgi.h"
#include "htmshell.h"

/* Some variables that say where sequence is.  These will have to
 * be changed from version to version. */
char *hostName = "kks00.cse.ucsc.edu";
char *hostPort = "17777";
char *nibDir = "/projects/cc/hg/oo.23/nib";
char *database = "hg5";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgSeqSearch - CGI-script to manage fast human genome sequence searching\n"
  "usage:\n"
  "   hgSeqSearch XXX\n");
}

int pslCmpMatches(const void *va, const void *vb)
/* Compare to sort based on query. */
{
const struct psl *a = *((struct psl **)va);
const struct psl *b = *((struct psl **)vb);
int aScore = a->match + (a->repMatch>>1) - a->misMatch - a->qNumInsert;
int bScore = b->match - (b->repMatch>>1) - b->misMatch - b->qNumInsert;
return bScore - aScore;
}

void showAliPlaces(char *pslName, char *faName)
/* Show all the places that align. */
{
struct lineFile *lf = lineFileOpen(pslName, TRUE);
struct psl *pslList = NULL, *psl;
char *browserUrl = hgTracksName();
char *extraCgi = "";

while ((psl = pslNext(lf)) != NULL)
    {
    slAddHead(&pslList, psl);
    }
lineFileClose(&lf);
if (pslList == NULL)
    errAbort("Sorry, no matches found");

slSort(&pslList, pslCmpMatches);
fprintf(stdout, "<TT><PRE>");
fprintf(stdout, " SIZE IDENTITY  CHROMOSOME   START     END       cDNA   START  END  TOTAL\n");
fprintf(stdout, "-------------------------------------------------------------------------\n");
for (psl = pslList; psl != NULL; psl = psl->next)
    {
    printf("<A HREF=\"%s?position=%s:%d-%d&db=%s&ss=%s+%s%s\">",
	browserUrl, psl->tName, psl->tStart, psl->tEnd, database, 
	pslName, faName, extraCgi);
    printf("%5d  %5.1f%%  %9s  %9d %9d  %8s %5d %5d %5d</A>\n",
	psl->match + psl->misMatch + psl->repMatch + psl->nCount,
	100.0 - pslCalcMilliBad(psl, TRUE) * 0.1,
	skipChr(psl->tName), psl->tStart + 1, psl->tEnd,
	psl->qName, psl->qStart+1, psl->qEnd, psl->qSize);
    }
pslFreeList(&pslList);
}

void doMiddle()
{
static struct dnaSeq *seq;
struct tempName pslTn, faTn;
char *userSeq = cgiString("userSeq");
char *port = cgiOptionalString("port");
char *host = cgiOptionalString("host");
char *nib = cgiOptionalString("nib");
char *db = cgiOptionalString("db");
char *userFile = cgiOptionalString("userFile");
FILE *f;
int maxSize = 20000;

if (port != NULL)
    hostPort = port;
if (host != NULL)
    hostName = host;
if (nib != NULL)
    nibDir = nib;
if (db != NULL)
    database = db;
    
uglyf("userFile = %s<BR>\n", userFile);

/* Load up sequence from CGI. */
seq = faFromMemText(cloneString(userSeq));
if (seq->name[0] == 0)
    seq->name = "YourSeq";

if (seq->size > maxSize)
   {
   printf("Warning: only the first %d of %d bases used.<BR>\n",
   	maxSize, seq->size);
   seq->size = maxSize;
   seq->dna[maxSize] = 0;
   }

makeTempName(&faTn, "hgSs", ".fa");
faWrite(faTn.forCgi, seq->name, seq->dna, seq->size);

makeTempName(&pslTn, "hgSs", ".psl");
f = mustOpen(pslTn.forCgi, "w");


/* Create a temporary .psl file with the alignments against genome. */
gfAlignStrand(hostName, hostPort, nibDir, seq, FALSE, ffCdna, 40, gfSavePsl, f);
reverseComplement(seq->dna, seq->size);
gfAlignStrand(hostName, hostPort, nibDir, seq, TRUE,  ffCdna, 40, gfSavePsl, f);
carefulClose(&f);

showAliPlaces(pslTn.forCgi, faTn.forCgi);
}


int main(int argc, char *argv[])
/* Process command line. */
{
dnaUtilOpen();
htmShell("BLAT Search Results", doMiddle, NULL);
return 0;
}
