/* hgSeqSearch - CGI-script to manage fast human genome sequence searching. */
#include "common.h"
#include "errabort.h"
#include "hCommon.h"
#include "portable.h"
#include "linefile.h"
#include "dnautil.h"
#include "fa.h"
#include "psl.h"
#include "genoFind.h"
#include "cheapcgi.h"
#include "htmshell.h"

char *defaultDatabase = "hg6";	/* Default database. */

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

char *genomeList[] = {/* "Oct. 7, 2000", */ "Dec. 12, 2000", "April 1, 2001"};
char *typeList[] = {"BLAT's guess", "DNA", "protein", "translated RNA", "translated DNA"};
char *sortList[] = {"query,score", "query,start", "chrom,score", "chrom,start", "score"};

struct serverTable serverTable[] =  {
// {"hg5", "Oct. 7, 2000", TRUE, "kks00", "17776", "/projects/cc/hg/oo.23/nib"},
// {"hg5", "Oct. 7, 2000", FALSE, "kks00", "17777", "/projects/cc/hg/oo.23/nib"},
{"hg6", "Dec. 12, 2000", TRUE,  "blat2", "17778", "/projects/hg2/gs.6/oo.27/nib"},
{"hg6", "Dec. 12, 2000", FALSE, "blat2", "17779", "/projects/hg2/gs.6/oo.27/nib"},
{"hg7", "April 1, 2001", TRUE, "blat1", "17778", "/projects/hg3/gs.7/oo.29/nib"},
{"hg7", "April 1, 2001", FALSE, "blat1", "17779", "/projects/hg3/gs.7/oo.29/nib"},
};

struct serverTable *findServer(char *db, boolean isTrans)
/* Return server for given database. */
{
int i;
struct serverTable *serve;

if (db == NULL)
    db = defaultDatabase;
if (sameWord(db, "hg5"))
    errAbort("Sorry, the October 2000 BLAT server was taken down to make room "
             "for April 2001.  Hopefully we'll get an additional machine and "
	     "restore this service in a week or two.");
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
return psl->match + (psl->repMatch>>1) - psl->misMatch - psl->qNumInsert
  - psl->tNumInsert;
}

int pslCmpScore(const void *va, const void *vb)
/* Compare to sort based on query then score. */
{
const struct psl *a = *((struct psl **)va);
const struct psl *b = *((struct psl **)vb);
return pslScore(b) - pslScore(a);
}


int pslCmpQueryScore(const void *va, const void *vb)
/* Compare to sort based on query then score. */
{
const struct psl *a = *((struct psl **)va);
const struct psl *b = *((struct psl **)vb);
int diff = strcmp(a->qName, b->qName);
if (diff == 0)
    diff = pslScore(b) - pslScore(a);
return diff;
}

int pslCmpQueryStart(const void *va, const void *vb)
/* Compare to sort based on query start. */
{
const struct psl *a = *((struct psl **)va);
const struct psl *b = *((struct psl **)vb);
int diff = strcmp(a->qName, b->qName);
if (diff == 0)
    diff = a->qStart - b->qStart;
return diff;
}

int cmpChrom(char *a, char *b)
/* Compare two chromosomes. */
{
char cA, cB;
int diff;

if (startsWith("chr", a)) a += 3;
if (startsWith("chr", b)) b += 3;
cA = *a;
cB = *b;
if (isdigit(cA))
    {
    if (isdigit(cB))
       return atoi(a) - atoi(b);
    else
       return -1;
    }
else if (isdigit(cB))
    return 1;
else
    return strcmp(a, b);
}

int pslCmpTargetScore(const void *va, const void *vb)
/* Compare to sort based on target then score. */
{
const struct psl *a = *((struct psl **)va);
const struct psl *b = *((struct psl **)vb);
int diff = cmpChrom(a->tName, b->tName);
if (diff == 0)
    diff = pslScore(b) - pslScore(a);
return diff;
}

int pslCmpTargetStart(const void *va, const void *vb)
/* Compare to sort based on target start. */
{
const struct psl *a = *((struct psl **)va);
const struct psl *b = *((struct psl **)vb);
int diff = cmpChrom(a->tName, b->tName);
if (diff == 0)
    diff = a->tStart - b->tStart;
return diff;
}

static void earlyWarning(char *format, va_list args)
/* Write an error message so user can see it before page is really started. */
{
static boolean initted = FALSE;
if (!initted)
    {
    htmlStart("HTC Error");
    initted = TRUE;
    }
htmlVaParagraph(format,args);
}

static void enterHtml(char *title)
/* Print out header of web page with title.  Set
 * error handler to normal html error handler. */
{
htmlStart(title);
pushWarnHandler(htmlVaParagraph);
}


void showAliPlaces(char *pslName, char *faName, char *database)
/* Show all the places that align. */
{
struct lineFile *lf = pslFileOpen(pslName);
struct psl *pslList = NULL, *psl;
char *browserUrl = hgTracksName();
char *hgcUrl = hgcName();
char *uiState;
char *sort = cgiUsualString("sort", sortList[0]);

cgiVarExclude("userSeq");
cgiVarExclude("seqFile");
cgiVarExclude("genome");
cgiVarExclude("type");
cgiVarExclude("sort");
cgiVarExclude("position");
cgiVarExclude("db");
cgiVarExclude("ss");
uiState = cgiUrlString()->string;

while ((psl = pslNext(lf)) != NULL)
    {
    slAddHead(&pslList, psl);
    }
lineFileClose(&lf);
if (pslList == NULL)
    errAbort("Sorry, no matches found");

if (sameString(sort, "query,start"))
    {
    slSort(&pslList, pslCmpQueryStart);
    }
else if (sameString(sort, "query,score"))
    {
    slSort(&pslList, pslCmpQueryScore);
    }
else if (sameString(sort, "score"))
    {
    slSort(&pslList, pslCmpScore);
    }
else if (sameString(sort, "chrom,start"))
    {
    slSort(&pslList, pslCmpTargetStart);
    }
else if (sameString(sort, "chrom,score"))
    {
    slSort(&pslList, pslCmpTargetScore);
    }
else
    {
    slSort(&pslList, pslCmpQueryScore);
    }
printf("<H2>BLAT Search Results</H2>");
printf("<TT><PRE>");
printf("   ACTIONS      QUERY           SCORE START  END QSIZE IDENTITY CHRO STRAND  START    END  \n");
printf("--------------------------------------------------------------------------------------------\n");
for (psl = pslList; psl != NULL; psl = psl->next)
    {
    printf("<A HREF=\"%s?position=%s:%d-%d&db=%s&ss=%s+%s&%s\">",
	browserUrl, psl->tName, psl->tStart, psl->tEnd, database, 
	pslName, faName, uiState);
    printf("browser</A> ");
    printf("<A HREF=\"%s?o=%d&g=htcUserAli&i=%s+%s+%s&c=%s&l=%d&r=%d&db=%s\">", 
    	hgcUrl, psl->tStart, pslName, faName, psl->qName,  psl->tName,
	psl->tStart, psl->tEnd, database);
    printf("details</A> ");
    printf("%-14s %5d %5d %5d %5d %5.1f%%  %4s  %2s  %9d %9d\n",
	psl->qName, pslScore(psl), psl->qStart, psl->qEnd, psl->qSize,
	100.0 - pslCalcMilliBad(psl, TRUE) * 0.1,
	skipChr(psl->tName), psl->strand, psl->tStart + 1, psl->tEnd);
    }
pslFreeList(&pslList);
printf("</TT></PRE>");
}

void trimUniq(bioSeq *seqList)
/* Check that all seq's in list have a unique name.  Try and
 * abbreviate longer sequence names. */
{
struct hash *hash = newHash(0);
bioSeq *seq;

for (seq = seqList; seq != NULL; seq = seq->next)
    {
    if (strlen(seq->name) > 14)	/* Try and get rid of long NCBI .fa cruft. */
        {
	char *nameClone = NULL;
	char *abbrv = NULL;
	char *words[32];
	int wordCount;

	nameClone = cloneString(seq->name);
	wordCount = chopString(nameClone, "|", words, ArraySize(words));
	if (wordCount > 2)	/* Looks like it's an NCBI long name alright. */
	    {
	    abbrv = words[wordCount-1];
	    if (abbrv[0] == 0) abbrv = words[wordCount-2];
	    if (hashLookup(hash, abbrv) == NULL)
	        {
		freeMem(seq->name);
		seq->name = cloneString(abbrv);
		}
	    freez(&nameClone);
	    }
	}
    hashAddUnique(hash, seq->name, hash);
    }
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
if (seqList != NULL && seqList->name[0] == 0)
    {
    freeMem(seqList->name);
    seqList->name = cloneString("YourSeq");
    }
trimUniq(seqList);

/* Figure out size allowed. */
maxSingleSize = (isTx ? 5000 : 20000);
maxTotalSize = maxSingleSize * 2.5;

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
	    gfAlignTransTrans(conn, serve->nibDir, seq, FALSE, 5, gfSavePslx, &data, !txTxBoth);
	    if (txTxBoth)
		{
		close(conn);
		reverseComplement(seq->dna, seq->size);
		conn = gfConnect(serve->host, serve->port);
		gfAlignTransTrans(conn, serve->nibDir, seq, TRUE, 5, gfSavePslx, &data, FALSE);
		}
	    }
	else
	    {
	    gfAlignTrans(conn, serve->nibDir, seq, 5, gfSavePslx, &data);
	    }
	}
    else
	{
	gfAlignStrand(conn, serve->nibDir, seq, FALSE, 20, gfSavePsl, f);
	close(conn);
	reverseComplement(seq->dna, seq->size);
	conn = gfConnect(serve->host, serve->port);
	gfAlignStrand(conn, serve->nibDir, seq, TRUE, 20, gfSavePsl, f);
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
"<FORM ACTION=\"../cgi-bin/hgBlat\" METHOD=\"POST\" ENCTYPE=\"multipart/form-data\">\n"
"<H1 ALIGN=CENTER>BLAT Search Human Genome</H1>\n"
"<P>\n"
"<TABLE BORDER=0 WIDTH=\"96%\">\n"
"<TR>\n");

printf("%s", "<TD WIDTH=\"25%\"<CENTER>\n");
printf("Freeze:<BR>");
cgiMakeDropList("genome", genomeList, ArraySize(genomeList), serve->genome);
printf("%s", "</TD><TD WIDTH=\"25%\"<CENTER>\n");
printf("Query type:<BR>");
cgiMakeDropList("type", typeList, ArraySize(typeList), NULL);
printf("%s", "</TD><TD WIDTH=\"25%\"<CENTER>\n");
printf("Sort output:<BR>");
cgiMakeDropList("sort", sortList, ArraySize(sortList), NULL);
printf("%s", "</TD>\n");
printf("%s", "<TD WIDTH=\"25%\">\n"
    "<CENTER>\n"
    "<P><INPUT TYPE=SUBMIT NAME=Submit VALUE=Submit>\n"
    "</CENTER>\n"
    "</TD>\n"
    "</TR>\n"
    "</TABLE>\n");

puts("Please paste in a query sequence to see where it is located in the ");
puts("UCSC assembly of the human genome.  Multiple sequences can be searched\n");
puts("at once if separated by a line starting with > and the sequence name.\n");
puts("<P>");

puts("<TEXTAREA NAME=userSeq ROWS=14 COLS=80></TEXTAREA>\n");
puts("<P>");

puts("Rather than pasting a sequence, you can choose to upload a text file containing "
	 "the sequence.<BR>");
puts("Upload sequence: <INPUT TYPE=FILE NAME=\"seqFile\">");
puts(" <INPUT TYPE=SUBMIT Name=Submit VALUE=\"Submit File\"><P>\n");

cgiContinueAllVars();

printf("%s", 
"<P>Only DNA sequences less than 20,000 bases and protein or translated \n"
"sequence of less than 4000 letters will be processed.  If multiple sequences\n"
"are submitted at the same time, the total limit is 50,000 bases or 10,000\n"
"letters.\n</P>"
"BLAT on DNA is designed to\n"
"quickly find sequences of 95% and greater similarity of length 40 bases or\n"
"more.  It may miss more divergent or shorter sequence alignments.  It will find\n"
"perfect sequence matches of 33 bases, and sometimes find them down to 22 bases.\n"
"BLAT on proteins finds sequences of 80% and greater similarity of length 20 amino\n"
"acids or more.  In practice DNA BLAT works well on primates, and protein\n"
"blat on land vertebrates\n</P>"
"<P>BLAT is not BLAST.  DNA BLAT works by keeping an index of the entire genome\n"
"in memory.  The index consists of all non-overlapping 11-mers except for\n"
"those heavily involved in repeats.  The index takes up a bit less than\n"
"a gigabyte of RAM.  The genome itself is not kept in memory, allowing\n"
"BLAT to deliver high performance on a reasonably priced Linux box.\n"
"The index is used to find areas of probable homology, which are then\n"
"loaded into memory for a detailed alignment. Protein BLAT works in a similar\n"
"manner, except with 4-mers rather than 11-mers.  The protein index takes a little\n"
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
// get the sequence
char *userSeq = cgiOptionalString("userSeq");

// have no sequence in userSeq
if(userSeq != 0 && userSeq[0] == '\0') {
	// try the file
	userSeq = cgiOptionalString("seqFile");
}

//printf("seqFile: %s\n", cgiOptionalString("seqFile"));
//printf("file   : %s\n", cgiOptionalString("file"));

if(userSeq == NULL || userSeq[0] == '\0')
    {
    enterHtml("BLAT Search");
    askForSeq();
    }
else
    {
    enterHtml("BLAT Results");
    blatSeq(userSeq);
    }
}


void errDoMiddle()
/* Do middle with a nice early warning error handler. */
{
pushWarnHandler(earlyWarning);
dnaUtilOpen();
doMiddle();
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
htmlSetBackground("../images/floret.jpg");
htmEmptyShell(errDoMiddle, NULL);
return 0;
}
