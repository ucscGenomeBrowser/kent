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

/* Some variables that say where sequence is.  These are default
 * values that can be overriden by CGI command. */
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
printf("<TT><PRE>");
printf(" SIZE IDENTITY CHROMOSOME STRAND  START     END       cDNA   START  END  TOTAL\n");
printf("------------------------------------------------------------------------------\n");
for (psl = pslList; psl != NULL; psl = psl->next)
    {
    printf("<A HREF=\"%s?position=%s:%d-%d&db=%s&ss=%s+%s%s\">",
	browserUrl, psl->tName, psl->tStart, psl->tEnd, database, 
	pslName, faName, extraCgi);
    printf("%5d  %5.1f%%  %9s     %s %9d %9d  %8s %5d %5d %5d</A>",
	psl->match + psl->misMatch + psl->repMatch + psl->nCount,
	100.0 - pslCalcMilliBad(psl, TRUE) * 0.1,
	skipChr(psl->tName), psl->strand, psl->tStart + 1, psl->tEnd,
	psl->qName, psl->qStart+1, psl->qEnd, psl->qSize);
    }
pslFreeList(&pslList);
}

void blatSeq(char *userSeq)
/* Blat sequence user pasted in. */
{
FILE *f;
static struct dnaSeq *seq;
struct tempName pslTn, faTn;
int maxSize = 20000;
char *port = cgiOptionalString("port");
char *host = cgiOptionalString("host");
char *nib = cgiOptionalString("nib");
char *db = cgiOptionalString("db");

if (port != NULL)
    hostPort = port;
if (host != NULL)
    hostName = host;
if (nib != NULL)
    nibDir = nib;
if (db != NULL)
    database = db;
    
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

void askForSeq()
/* Put up a little form that asks for sequence.
 * Call self.... */
{
char *db = cgiOptionalString("db");
char *port, *host, *nib;

printf("%s", 
"<FORM ACTION=\"../cgi-bin/hgBlat\" METHOD=POST>\n"
"<H1 ALIGN=CENTER>BLAT Search Human Genome</H1>\n"
"<P>\n"
"<TABLE BORDER=0 WIDTH=\"94%\">\n"
"<TR>\n"
"<TD WIDTH=\"85%\">Please paste in a DNA sequence to see where it is located in the Oct. 7, 2000 UCSC assembly\n"
"of the human genome.</TD>\n"
"<TD WIDTH=\"15%\">\n"
"<CENTER>\n"
"<P><INPUT TYPE=SUBMIT NAME=Submit VALUE=Submit>\n"
"</CENTER>\n"
"</TD>\n"
"</TR>\n"
"</TABLE>\n"
"<TEXTAREA NAME=userSeq ROWS=14 COLS=72></TEXTAREA>\n");


if (db != NULL)
    {
    if (sameString(db, "hg5"))
        {
	port = "17777";
	nib = "/projects/cc/hg/oo.23/nib";
	host = "kks00.cse.ucsc.edu";
	}
    else if (sameString(db, "hg6"))
        {
	port = "17778";
	nib = "/projects/hg2/gs.6/oo.27/nib";
	host = "kks00.cse.ucsc.edu";
	}
   else 
	{
        errAbort("Unknown database %s", db);
	}
    cgiMakeHiddenVar("port", port);
    cgiMakeHiddenVar("host", host);
    cgiMakeHiddenVar("nib", nib);
    cgiMakeHiddenVar("db", db);
    }
else
    {
    cgiContinueHiddenVar("port");
    cgiContinueHiddenVar("host");
    cgiContinueHiddenVar("nib");
    cgiContinueHiddenVar("db");
    }

printf("%s", 
"<P>Only the first 20,000 bases of a sequence will be used.  BLAT is designed to\n"
"quickly find sequences of 95% and greater similarity of length 50 bases or\n"
"more.  It may miss more divergent or shorter sequence alignments.</P>\n"
"<P>BLAT is not BLAST.  BLAT works by keeping an index of the entire genome\n"
"in memory.  The index consists of all non-overlapping 12-mers except for\n"
"those heavily involved in repeats.  The index takes up a bit less than\n"
"a gigabyte of RAM.  The genome itself is not kept in memory, allowing\n"
"BLAT to deliver high performance on a reasonably priced Linux box.\n"
"The index is used to find areas of probable homology, which are then\n"
"loaded into memory for a detailed alignment.</P>\n"
"<P>BLAT was written by <A HREF=\"mailto:jim_kent@pacbell.net\">Jim Kent</A>.\n"
"Like most of Jim's software use on this web server is free to all.\n"
"Sources and executables to run on your server are available free\n"
"for academic, personal, and non-profit purposes.  Non-exclusive commercial\n"
"licenses are also available.  Contact Jim for details.</P>\n"
"\n"
"</FORM>\n");
}

void doMiddle()
{
char *userSeq = cgiOptionalString("userSeq");

if (userSeq == NULL)
    askForSeq();
else
    blatSeq(userSeq);
}


int main(int argc, char *argv[])
/* Process command line. */
{
dnaUtilOpen();
htmlSetBackground("../images/floret.jpg");
htmShell("BLAT Search", doMiddle, NULL);
return 0;
}
