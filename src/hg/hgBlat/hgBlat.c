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

char *defaultDatabase = "hg5";	/* Default database. */

struct serverTable
/* Information on a server. */
   {
   char *db;		/* Database name. */
   char *genome;	/* Genome name. */
   boolean isTrans;	/* Is tranlated to protein? */
   char *host;		/* Name of machine hosting server. */
   char *port;		/* Port that hosts server. */
   char *nibDir;	/* Directory of sequence files. */
   };

char *genomeList[] = {"Oct. 7, 2000", "Dec. 12, 2000"};
char *typeList[] = {"auto", "DNA", "protein"};

struct serverTable serverTable[] =  {
{"hg5", "Oct. 7, 2000", TRUE, "kks00.cse.ucsc.edu", "17776", "/projects/cc/hg/oo.23/nib"},
{"hg5", "Oct. 7, 2000", FALSE, "kks00.cse.ucsc.edu", "17777", "/projects/cc/hg/oo.23/nib"},
{"hg6", "Dec. 12, 2000", TRUE,  "cc.cse.ucsc.edu", "17778", "/projects/hg2/gs.6/oo.27/nib"},
{"hg6", "Dec. 12, 2000", FALSE, "cc.cse.ucsc.edu", "17779", "/projects/hg2/gs.6/oo.27/nib"},
};

struct serverTable *findServer(char *db, boolean isTrans)
/* Return server for given database. */
{
int i;
struct serverTable *serve;

if (db == NULL)
    db = defaultDatabase;
for (i=0; i<ArraySize(serverTable); ++i)
    {
    serve = &serverTable[i];
    if (sameWord(serve->db, db) && serve->isTrans == isTrans)
        return serve;
    if (sameWord(serve->genome, db) && serve->isTrans == isTrans)
        return serve;
    }
errAbort("Can't find a server for %s database %s\n",
	(isTrans ? "translated" : "DNA"), db);
return NULL;
}

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgSeqSearch - CGI-script to manage fast human genome sequence searching\n");
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

void showAliPlaces(char *pslName, char *faName, char *database)
/* Show all the places that align. */
{
struct lineFile *lf = pslFileOpen(pslName);
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
    printf("%5d  %5.1f%%  %9s     %s %9d %9d  %8s %5d %5d %5d</A>\n",
	psl->match + psl->misMatch + psl->repMatch + psl->nCount,
	100.0 - pslCalcMilliBad(psl, TRUE) * 0.1,
	skipChr(psl->tName), psl->strand, psl->tStart + 1, psl->tEnd,
	psl->qName, psl->qStart+1, psl->qEnd, psl->qSize);
    }
pslFreeList(&pslList);
printf("</TT></PRE>");
}

void blatSeq(char *userSeq)
/* Blat sequence user pasted in. */
{
FILE *f;
static struct dnaSeq *seq;
struct tempName pslTn, faTn;
int maxSize;
char *genome = cgiString("genome");
char *type = cgiString("type");
char *seqLetters = cloneString(userSeq);
struct serverTable *serve;
int conn;
boolean isTx;

/* Load user sequence and figure out if it is DNA or protein. */
if (sameWord(type, "DNA"))
    {
    seq = faSeqFromMemText(seqLetters, TRUE);
    isTx = FALSE;
    }
else if (sameWord(type, "protein"))
    {
    seq = faSeqFromMemText(seqLetters, FALSE);
    isTx = TRUE;
    }
else 
    {
    seq = faSeqFromMemText(seqLetters, FALSE);
    isTx = !seqIsDna(seq);
    if (!isTx)
        toLowerN(seq->dna, seq->size);
    }

/* Truncate sequence if necessary and name it. */
maxSize = (isTx ? 4000 : 20000);
if (seq->size > maxSize)
    {
    warn("Sequence is %d letters long, truncating to %d",
        seq->size, maxSize);
    seq->size = maxSize;
    seq->dna[maxSize] = 0;
    }
if (seq->name[0] == 0)
    seq->name = cloneString("YourSeq");


/* Create temporary file to store sequence. */
makeTempName(&faTn, "hgSs", ".fa");
faWrite(faTn.forCgi, seq->name, seq->dna, seq->size);

/* Create a temporary .psl file with the alignments against genome. */
serve = findServer(genome, isTx);
makeTempName(&pslTn, "hgSs", ".pslx");
f = mustOpen(pslTn.forCgi, "w");
conn = gfConnect(serve->host, serve->port);
if (isTx)
    {
    static struct gfSavePslxData data;
    data.f = f;
    data.reportTargetStrand = TRUE;
    pslxWriteHead(f, gftProt, gftDnaX);
    gfAlignTrans(conn, serve->nibDir, seq, 12, gfSavePslx, &data);
    }
else
    {
    pslxWriteHead(f, gftDna, gftDna);
    gfAlignStrand(conn, serve->nibDir, seq, FALSE, ffCdna, 36, gfSavePsl, f);
    close(conn);
    reverseComplement(seq->dna, seq->size);
    conn = gfConnect(serve->host, serve->port);
    gfAlignStrand(conn, serve->nibDir, seq, TRUE,  ffCdna, 36, gfSavePsl, f);
    }
close(conn);
carefulClose(&f);

showAliPlaces(pslTn.forCgi, faTn.forCgi, serve->db);
}

void askForSeq()
/* Put up a little form that asks for sequence.
 * Call self.... */
{
char *db = cgiOptionalString("db");
struct serverTable *serve = findServer(db, FALSE);

printf("%s", 
"<FORM ACTION=\"../cgi-bin/hgBlat\" METHOD=POST>\n"
"<H1 ALIGN=CENTER>BLAT Search Human Genome</H1>\n"
"<P>\n"
"<TABLE BORDER=0 WIDTH=\"94%\">\n"
"<TR>\n"
"<TD WIDTH=\"74%\">Please paste in a sequence to see where it is located in the ");
printf("%s", "UCSC assembly\n"
"of the human genome.</TD>\n");

printf("%s", "<TD WIDTH=\"15%\"<CENTER>\n");
cgiMakeDropList("genome", genomeList, ArraySize(genomeList), serve->genome);
printf("<BR><B>type:</B>\n");
cgiMakeDropList("type", typeList, ArraySize(typeList), NULL);
printf("%s", "</TD>\n");

printf("%s",
"<TD WIDTH=\"11%\">\n"
"<CENTER>\n"
"<P><INPUT TYPE=SUBMIT NAME=Submit VALUE=Submit>\n"
"</CENTER>\n"
"</TD>\n"
"</TR>\n"
"</TABLE>\n"
"<TEXTAREA NAME=userSeq ROWS=14 COLS=72></TEXTAREA>\n");


cgiMakeHiddenVar("db", serve->db);

printf("%s", 
"<P>Only the first 20,000 bases of DNA sequence and the first 4000 bases of\n"
"a protein sequence will be used.  BLAT on DNA is designed to\n"
"quickly find sequences of 95% and greater similarity of length 40 bases or\n"
"more.  It may miss more divergent or shorter sequence alignments.  It will find\n"
"perfect sequence matches of 36 bases, and sometimes find them down to 24 bases.\n"
"BLAT on proteins finds sequences of 80% and greater similarity of length 20 amino\n"
"acids or more.  In practice DNA BLAT works well on primates, and protein\n"
"blat on land vertebrates\n</P>"
"<P>BLAT is not BLAST.  Nucleotide BLAT works by keeping an index of the entire genome\n"
"in memory.  The index consists of all non-overlapping 12-mers except for\n"
"those heavily involved in repeats.  The index takes up a bit less than\n"
"a gigabyte of RAM.  The genome itself is not kept in memory, allowing\n"
"BLAT to deliver high performance on a reasonably priced Linux box.\n"
"The index is used to find areas of probable homology, which are then\n"
"loaded into memory for a detailed alignment. Protein BLAT works in a similar\n"
"manner, except with 4-mers rather than 12-mers.  The protein index takes a little\n"
"more than 2 gigabytes</P>\n"
"<P>BLAT was written by <A HREF=\"mailto:jim_kent@pacbell.net\">Jim Kent</A>.\n"
"Like most of Jim's software interactive use on this web server is free to all.\n"
"Sources and executables to run batch jobs on your own server are available free\n"
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
cgiSpoof(&argc, argv);
dnaUtilOpen();
htmlSetBackground("../images/floret.jpg");
htmShell("BLAT Search", doMiddle, NULL);
return 0;
}
