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
char *typeList[] = {"BLAT's guess", "DNA", "protein", "translated RNA", "translated DNA"};

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

int pslScore(const struct psl *psl)
/* Return score for psl. */
{
int aScore = psl->match + (psl->repMatch>>1) - psl->misMatch - psl->qNumInsert
  - psl->tNumInsert;
}

int pslCmpMatches(const void *va, const void *vb)
/* Compare to sort based on query. */
{
const struct psl *a = *((struct psl **)va);
const struct psl *b = *((struct psl **)vb);
int diff = strcmp(a->qName, b->qName);
if (diff == 0)
    diff = pslScore(b) - pslScore(a);
return diff;
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
printf(" QUERY       SCORE START END  TOTAL IDENTITY CHROMOSOME STRAND  START    END  \n");
printf("--------------------------------------------------------------------------------\n");
for (psl = pslList; psl != NULL; psl = psl->next)
    {
    printf("<A HREF=\"%s?position=%s:%d-%d&db=%s&ss=%s+%s%s\">",
	browserUrl, psl->tName, psl->tStart, psl->tEnd, database, 
	pslName, faName, extraCgi);
    printf("%-12s %5d %5d %5d %5d %5.1f%%  %9s  %2s  %9d %9d</A>\n",
	psl->qName, pslScore(psl), psl->qStart, psl->qEnd, psl->qSize,
	100.0 - pslCalcMilliBad(psl, TRUE) * 0.1,
	skipChr(psl->tName), psl->strand, psl->tStart + 1, psl->tEnd);
    }
pslFreeList(&pslList);
printf("</TT></PRE>");
}

void checkSeqNamesUniq(bioSeq *seqList)
/* Check that all seq's in list have a unique name. */
{
struct hash *hash = newHash(0);
bioSeq *seq;

for (seq = seqList; seq != NULL; seq = seq->next)
    hashAddUnique(hash, seq->name, hash);
freeHash(&hash);
}

int realSeqSize(bioSeq *seq, boolean isDna)
/* Return size of sequence without N's or (for proteins)
 * X's. */
{
char unknown = (isDna ? 'n' : 'X');
int i, size = seq->size, count = 0;
char *s = seq->dna;
for (i=0; i<size; ++i)
    if (s[i] != unknown) ++count;
return count;
}

void blatSeq(char *userSeq)
/* Blat sequence user pasted in. */
{
FILE *f;
struct dnaSeq *seqList = NULL, *seq;
struct tempName pslTn, faTn;
int maxSingleSize, maxTotalSize;
char *genome = cgiString("genome");
char *type = cgiString("type");
char *seqLetters = cloneString(userSeq);
struct serverTable *serve;
int conn;
int oneSize, totalSize = 0;
boolean isTx = FALSE;
boolean isTxTx = FALSE;
boolean txTxBoth = FALSE;

/* Load user sequence and figure out if it is DNA or protein. */
if (sameWord(type, "DNA"))
    {
    seqList = faSeqListFromMemText(seqLetters, TRUE);
    isTx = FALSE;
    }
else if (sameWord(type, "translated RNA") || sameWord(type, "translated DNA"))
    {
    seqList = faSeqListFromMemText(seqLetters, TRUE);
    isTx = TRUE;
    isTxTx = TRUE;
    txTxBoth = sameWord(type, "translated DNA");
    }
else if (sameWord(type, "protein"))
    {
    seqList = faSeqListFromMemText(seqLetters, FALSE);
    isTx = TRUE;
    }
else 
    {
    seqList = faSeqListFromMemText(seqLetters, FALSE);
    isTx = !seqIsDna(seqList);
    if (!isTx)
	{
	for (seq = seqList; seq != NULL; seq = seq->next)
	    {
	    toLowerN(seq->dna, seq->size);
	    }
	}
    }
checkSeqNamesUniq(seqList);

/* Figure out size allowed. */
maxSingleSize = (isTx ? 4000 : 20000);
maxTotalSize = maxSingleSize * 2;

/* Create temporary file to store sequence. */
makeTempName(&faTn, "hgSs", ".fa");
faWriteAll(faTn.forCgi, seqList);

/* Create a temporary .psl file with the alignments against genome. */
serve = findServer(genome, isTx);
makeTempName(&pslTn, "hgSs", ".pslx");
f = mustOpen(pslTn.forCgi, "w");
if (isTx)
    {
    if (isTxTx)
        {
	pslxWriteHead(f, gftDnaX, gftDnaX);
	}
    else
        {
	pslxWriteHead(f, gftProt, gftDnaX);
	}
    }
else
    {
    pslxWriteHead(f, gftDna, gftDna);
    }

/* Loop through each sequence. */
for (seq = seqList; seq != NULL; seq = seq->next)
    {
    if (seq->name[0] == 0)
	seq->name = cloneString("YourSeq");
    oneSize = realSeqSize(seq, !isTx);
    if (oneSize > maxSingleSize)
	{
	warn("Sequence %s is %d letters long (max is %d), skipping",
	    seq->name, seq->size, maxSingleSize);
	continue;
	}
    totalSize += oneSize;
    if (totalSize > maxTotalSize)
        {
	warn("Sequence %s would take us over the %d letter limit, stopping here.",
	     seq->name, maxTotalSize);
	break;
	}
    conn = gfConnect(serve->host, serve->port);
    if (isTx)
	{
	static struct gfSavePslxData data;
	data.f = f;
	data.reportTargetStrand = TRUE;
	if (isTxTx)
	    {
	    gfAlignTransTrans(conn, serve->nibDir, seq, FALSE, 12, gfSavePslx, &data);
	    if (txTxBoth)
		{
		close(conn);
		reverseComplement(seq->dna, seq->size);
		conn = gfConnect(serve->host, serve->port);
		gfAlignTransTrans(conn, serve->nibDir, seq, TRUE, 12, gfSavePslx, &data);
		}
	    }
	else
	    {
	    gfAlignTrans(conn, serve->nibDir, seq, 12, gfSavePslx, &data);
	    }
	}
    else
	{
	gfAlignStrand(conn, serve->nibDir, seq, FALSE, 36, gfSavePsl, f);
	close(conn);
	reverseComplement(seq->dna, seq->size);
	conn = gfConnect(serve->host, serve->port);
	gfAlignStrand(conn, serve->nibDir, seq, TRUE, 36, gfSavePsl, f);
	}
    close(conn);
    }
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
"<TABLE BORDER=0 WIDTH=\"96%\">\n"
"<TR>\n"
"<TD WIDTH=\"50%\">Please paste in a query sequence to see where it is located in the ");
printf("%s", "UCSC assembly\n"
"of the human genome.</TD>\n");
printf("%s", "<TD WIDTH=\"4%\"</TD>\n");
printf("%s", "<TD WIDTH=\"16%\"<CENTER>\n");
printf("Freeze:<BR>");
cgiMakeDropList("genome", genomeList, ArraySize(genomeList), serve->genome);
printf("%s", "</TD><TD WIDTH=\"20%\"<CENTER>\n");
printf("Query type<BR>");
cgiMakeDropList("type", typeList, ArraySize(typeList), NULL);
printf("%s", "</TD>\n");

printf("%s",
"<TD WIDTH=\"10%\">\n"
"<CENTER>\n"
"<P><INPUT TYPE=SUBMIT NAME=Submit VALUE=Submit>\n"
"</CENTER>\n"
"</TD>\n"
"</TR>\n"
"</TABLE>\n"
"<TEXTAREA NAME=userSeq ROWS=14 COLS=80></TEXTAREA>\n");


cgiMakeHiddenVar("db", serve->db);

printf("%s", 
"<P>Only the first 20,000 bases of DNA sequence and the first 4000 bases of\n"
"a protein sequence or translated DNA sequence will be used.  BLAT on DNA is designed to\n"
"quickly find sequences of 95% and greater similarity of length 40 bases or\n"
"more.  It may miss more divergent or shorter sequence alignments.  It will find\n"
"perfect sequence matches of 36 bases, and sometimes find them down to 24 bases.\n"
"BLAT on proteins finds sequences of 80% and greater similarity of length 20 amino\n"
"acids or more.  In practice DNA BLAT works well on primates, and protein\n"
"blat on land vertebrates\n</P>"
"<P>BLAT is not BLAST.  DNA BLAT works by keeping an index of the entire genome\n"
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
