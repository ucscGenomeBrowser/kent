/* hgSeqSearch - CGI-script to manage fast human genome sequence searching. */
#include "common.h"
#include "errabort.h"
#include "hCommon.h"
#include "jksql.h"
#include "portable.h"
#include "linefile.h"
#include "dnautil.h"
#include "fa.h"
#include "psl.h"
#include "genoFind.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "hdb.h"
#include "hui.h"
#include "cart.h"
#include "dbDb.h"
#include "blatServers.h"
#include "web.h"
#include "hash.h"

struct cart *cart;	/* The user's ui state. */
char *defaultDatabase;	/* Default database. */
struct hash *oldVars = NULL;

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

char *typeList[] = {"BLAT's guess", "DNA", "protein", "translated RNA", "translated DNA"};
char *sortList[] = {"query,score", "query,start", "chrom,score", "chrom,start", "score"};
char *outputList[] = {"hyperlink", "psl", "psl no header"};


void getIndexedGenomeDescriptions(char ***retArray, int *retCount)
/* Find out the list of genomes that have blat servers on them. */
{
struct dbDb *dbList = hGetBlatIndexedDatabases();
struct dbDb *db = NULL;
int i = 0;
int count = slCount(dbList);
char **array = NULL;

if (count == 0)
    errAbort("No active blat servers in database");
AllocArray(array, count);
for (i=0, db=dbList; i<count; ++i, db=db->next)
    array[i] = cloneString(db->description);
dbDbFreeList(&dbList);
*retArray = array;
*retCount = count;
}

struct serverTable *findServer(char *db, boolean isTrans)
/* Return server for given database.  Db can either be
 * database name or description. */
{
static struct serverTable st;
struct sqlConnection *conn = hConnectCentral();
char query[256];
struct sqlResult *sr;
char **row;
char dbActualName[32];

/* If necessary convert database description to name. */
sprintf(query, "select name from dbDb where name = '%s'", db);
if (!sqlExists(conn, query))
    {
    sprintf(query, "select name from dbDb where description = '%s'", db);
    if (sqlQuickQuery(conn, query, dbActualName, sizeof(dbActualName)) != NULL)
        db = dbActualName;
    }

/* Do a little join to get data to fit into the serverTable. */
sprintf(query, "select dbDb.name,dbDb.description,blatServers.isTrans"
               ",blatServers.host,blatServers.port,dbDb.nibPath "
	       "from dbDb,blatServers where blatServers.isTrans = %d and "
	       "dbDb.name = '%s' and dbDb.name = blatServers.db", 
	       isTrans, db);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    {
    errAbort("Can't find a server for %s database %s\n",
	    (isTrans ? "translated" : "DNA"), db);
    }
st.db = cloneString(row[0]);
st.genome = cloneString(row[1]);
st.isTrans = atoi(row[2]);
st.host = cloneString(row[3]);
st.port = cloneString(row[4]);
st.nibDir = cloneString(row[5]);
sqlFreeResult(&sr);
hDisconnectCentral(&conn);
return &st;
}

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgSeqSearch - CGI-script to manage fast human genome sequence searching\n");
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

#ifdef OLD
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
#endif /* OLD */


void showAliPlaces(char *pslName, char *faName, char *database, 
	enum gfType qType, enum gfType tType)
/* Show all the places that align. */
{
struct lineFile *lf = pslFileOpen(pslName);
struct psl *pslList = NULL, *psl;
char *browserUrl = hgTracksName();
char *hgcUrl = hgcName();
char uiState[64];
char *vis;
char unhideTrack[64];
char *sort = cartUsualString(cart, "sort", sortList[0]);
char *output = cartUsualString(cart, "output", outputList[0]);
boolean pslOut = startsWith("psl", output);

sprintf(uiState, "%s=%u", cartSessionVarName(), cartSessionId(cart));

/* If user has hidden BLAT track, add a setting that will unhide the 
   track if user clicks on a browser link. */
vis = cartOptionalString(cart, "hgUserPsl");
if (vis != NULL && sameString(vis, "hide"))
    snprintf(unhideTrack, sizeof(unhideTrack), "&hgUserPsl=dense");
else
    unhideTrack[0] = 0;

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
if (pslOut)
    {
    printf("<TT><PRE>");
    if (!sameString(output, "psl no header"))
	pslxWriteHead(stdout, qType, tType);
    for (psl = pslList; psl != NULL; psl = psl->next)
	pslTabOut(psl, stdout);
    }
else
    {
    printf("<H2>BLAT Search Results</H2>");
    printf("<TT><PRE>");
    printf("   ACTIONS      QUERY           SCORE START  END QSIZE IDENTITY CHRO STRAND  START    END  \n");
    printf("--------------------------------------------------------------------------------------------\n");
    for (psl = pslList; psl != NULL; psl = psl->next)
	{
	printf("<A HREF=\"%s?position=%s:%d-%d&db=%s&ss=%s+%s&%s%s\">",
	    browserUrl, psl->tName, psl->tStart + 1, psl->tEnd, database, 
	    pslName, faName, uiState, unhideTrack);
	printf("browser</A> ");
	printf("<A HREF=\"%s?o=%d&g=htcUserAli&i=%s+%s+%s&c=%s&l=%d&r=%d&db=%s&%s\">", 
	    hgcUrl, psl->tStart, pslName, faName, psl->qName,  psl->tName,
	    psl->tStart, psl->tEnd, database, uiState);
	printf("details</A> ");
	printf("%-14s %5d %5d %5d %5d %5.1f%%  %4s  %2s  %9d %9d\n",
	    psl->qName, pslScore(psl), psl->qStart, psl->qEnd, psl->qSize,
	    100.0 - pslCalcMilliBad(psl, TRUE) * 0.1,
	    skipChr(psl->tName), psl->strand, psl->tStart, psl->tEnd);
	}
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
	boolean isEns = (stringIn("ENSEMBL:", seq->name) != NULL);

	nameClone = cloneString(seq->name);
	wordCount = chopString(nameClone, "|", words, ArraySize(words));
	if (wordCount > 1)	/* Looks like it's an Ensembl/NCBI 
		                 * long name alright. */
	    {
	    if (isEns)
		{
	        abbrv = words[0];
		if (abbrv[0] == 0) abbrv = words[1];
		}
	    else
		{
		abbrv = words[wordCount-1];
		if (abbrv[0] == 0) abbrv = words[wordCount-2];
		}
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
char *genome = cgiString("db");
char *type = cgiString("type");
char *seqLetters = cloneString(userSeq);
struct serverTable *serve;
int conn;
int oneSize, totalSize = 0;
boolean isTx = FALSE;
boolean isTxTx = FALSE;
boolean txTxBoth = FALSE;
struct gfOutput *gvo;
boolean qIsProt = FALSE;
enum gfType qType, tType;

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
    qIsProt = TRUE;
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
makeTempName(&pslTn, "hgSs", ".pslx");
f = mustOpen(pslTn.forCgi, "w");
gvo = gfOutputPsl(0, qIsProt, FALSE, f, FALSE, TRUE);
serve = findServer(genome, isTx);
/* Write header for extended (possibly protein) psl file. */
if (isTx)
    {
    if (isTxTx)
        {
	qType = gftDnaX;
	tType = gftDnaX;
	}
    else
        {
	qType = gftProt;
	tType = gftDnaX;
	}
    }
else
    {
    qType = gftDna;
    tType = gftDna;
    }
pslxWriteHead(f, qType, tType);

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
	gvo->reportTargetStrand = TRUE;
	if (isTxTx)
	    {
	    gfAlignTransTrans(&conn, serve->nibDir, seq, FALSE, 5, gvo, !txTxBoth);
	    if (txTxBoth)
		{
		reverseComplement(seq->dna, seq->size);
		conn = gfConnect(serve->host, serve->port);
		gfAlignTransTrans(&conn, serve->nibDir, seq, TRUE, 5, gvo, FALSE);
		}
	    }
	else
	    {
	    gfAlignTrans(&conn, serve->nibDir, seq, 5, gvo);
	    }
	}
    else
	{
	gfAlignStrand(&conn, serve->nibDir, seq, FALSE, 16, gvo);
	reverseComplement(seq->dna, seq->size);
	conn = gfConnect(serve->host, serve->port);
	gfAlignStrand(&conn, serve->nibDir, seq, TRUE, 16, gvo);
	}
    gfOutputQuery(gvo, f);
    }
carefulClose(&f);
showAliPlaces(pslTn.forCgi, faTn.forCgi, serve->db, qType, tType);
}

void askForSeq()
/* Put up a little form that asks for sequence.
 * Call self.... */
{
char *db = NULL; //cartUsualString(cart, "db", defaultDatabase);
struct serverTable *serve = NULL; //findServer(db, FALSE);
char **genomeList;
int genomeCount;
char *organism = NULL; //hOrganism(db);
char *assemblyList[128];
char *values[128];
int numAssemblies = 0;
struct dbDb *dbList = NULL; // hGetBlatIndexedDatabases();
struct dbDb *cur = NULL;
char *assembly = NULL;

/* JavaScript to copy input data on the change genome button to a hidden form
This was done in order to be able to flexibly arrange the UI HTML
*/
char *onChangeText = "onchange=\"document.orgForm.org.value = document.mainForm.org.options[document.mainForm.org.selectedIndex].value; document.orgForm.submit();\"";

getDbAndGenome(cart, &db, &organism);
serve = findServer(db, FALSE);

printf( 
"<FORM ACTION=\"../cgi-bin/hgBlat\" METHOD=\"POST\" ENCTYPE=\"multipart/form-data\" NAME=\"mainForm\">\n"
"<H1 ALIGN=CENTER>BLAT Search Genome</H1>\n"
"<P>\n"
"<TABLE BORDER=0 WIDTH=\"96%%\" COLS=6>\n"
"<TR>\n");
cartSaveSession(cart);


printf("%s", "<TD><CENTER>\n");
printf("Genome:<BR>");
printGenomeListHtml(db, onChangeText);
printf("%s", "</TD><TD><CENTER>\n");
printf("Assembly:<BR>");
printBlatAssemblyListHtml(db);
printf("%s", "</TD><TD><CENTER>\n");
printf("Query type:<BR>");
cgiMakeDropList("type", typeList, ArraySize(typeList), NULL);
printf("%s", "</TD><TD><CENTER>\n");
printf("Sort output:<BR>");
cgiMakeDropList("sort", sortList, ArraySize(sortList), cartOptionalString(cart, "sort"));
printf("%s", "</TD>\n");
printf("%s", "<TD><CENTER>\n");
printf("Output type:<BR>");
cgiMakeDropList("output", outputList, ArraySize(outputList), cartOptionalString(cart, "output"));
puts("</TD>\n");

puts("<TD><CENTER>&nbsp;<BR>\n"
    "<INPUT TYPE=SUBMIT NAME=Submit VALUE=Submit Align=\"bottom\">\n"
    "</TD>\n"
    "</TR>\n"
    "<TR>\n"
    "<TD COLSPAN=5>\n"
    "Please paste in a query sequence to see where it is located in the\n"
    "the genome.  Multiple sequences can be searched \n"
    "at once if separated by a line starting with > and the sequence name.\n"
    "</TD>\n"
    "<TD>\n"
    "<INPUT TYPE=RESET NAME=Reset VALUE=Reset Align=\"bottom\">\n"
    "</TD>\n"
    "</TR>\n"
    "</TABLE>\n"
);

puts("<P>");

puts("<TEXTAREA NAME=userSeq ROWS=14 COLS=80></TEXTAREA>\n");
puts("<P>");

puts("Rather than pasting a sequence, you can choose to upload a text file containing "
	 "the sequence.<BR>");
puts("Upload sequence: <INPUT TYPE=FILE NAME=\"seqFile\">");
puts(" <INPUT TYPE=SUBMIT Name=Submit VALUE=\"Submit File\"><P>\n");

printf("%s", 
"<P>Only DNA sequences of 25,000 or less bases and protein or translated \n"
"sequence of 5000 or less letters will be processed.  If multiple sequences\n"
"are submitted at the same time, the total limit is 50,000 bases or 12,500\n"
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

printf("<FORM ACTION=\"/cgi-bin/hgBlat\" METHOD=\"GET\" NAME=\"orgForm\"><input type=\"hidden\" name=\"org\" value=\"%s\">\n", organism);
cartSaveSession(cart);
puts("</FORM>");
}

void doMiddle(struct cart *theCart)
{
char *userSeq;

cart = theCart;
dnaUtilOpen();

cartWebStart(theCart, "BLAT Search");

/* Get sequence - from userSeq variable, or if 
 * that is empty from a file. */
userSeq = cartOptionalString(cart, "userSeq");
if(userSeq != 0 && userSeq[0] == '\0')
    userSeq = cartOptionalString(cart, "seqFile");

if(userSeq == NULL || userSeq[0] == '\0')
    {
    askForSeq();
    }
else
    {
    blatSeq(skipLeadingSpaces(userSeq));
    }
cartWebEnd();
}

/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
char *excludeVars[] = {"Submit", "submit", "type", "genome", "userSeq", "seqFile", NULL};

int main(int argc, char *argv[])
/* Process command line. */
{
oldVars = hashNew(8);
cgiSpoof(&argc, argv);
defaultDatabase = hGetDb();
htmlSetBackground("../images/floret.jpg");
cartEmptyShell(doMiddle, "hguid", excludeVars, oldVars);
return 0;
}

