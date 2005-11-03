/* webBlat - CGI Applet for Blat. */
/* Copyright 2004 Jim Kent.  All rights reserved. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "errabort.h"
#include "errCatch.h"
#include "portable.h"
#include "htmshell.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"
#include "nib.h"
#include "twoBit.h"
#include "psl.h"
#include "fuzzyFind.h"
#include "cheapcgi.h"
#include "genoFind.h"
#include "gfPcrLib.h"
#include "gfWebLib.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "webBlat - CGI Applet for Blat\n"
  "This program is not generally meant to be run from the command line.\n"
  );
}

struct gfWebConfig *cfg;	/* Our configuration. */

struct gfServerAt *findServer(boolean txServer)
/* Find gfServer. */
{
if (txServer)
    return gfWebFindServer(cfg->transServerList, "wb_db");
else
    return gfWebFindServer(cfg->serverList, "wb_db");
}

void doHelp()
/* Put up help page. */
{
uglyf("I'm just not very helpful");
}

char *protQueryMenu[] = {"Protein", "Translated DNA", "Translated RNA"};
char *nucQueryMenu[] = {"DNA", "RNA", };
char *bothQueryMenu[] = {"BLAT's Guess", "DNA", "RNA", 
	"Protein", "Translated DNA", "Translated RNA"};
char *sortMenu[] = {"query,score", "query,start", "chrom,score", "chrom,start", "score"};
char *outputMenu[] = {"hyperlink", "psl", "psl no header"};

boolean isTxType(char *type)
/* Return TRUE if it's a query requiring a translated server type */
{
int i;
for (i=0; i<ArraySize(protQueryMenu); ++i)
    {
    if (sameWord(type, protQueryMenu[i]))
	 return TRUE;
    }
return FALSE;
}

int cmpSeqName(char *a, char *b)
/* Compare two sequence names likely to be of form prefix followed by a number. */
{
char cA, cB;
int cSame = countSame(a, b);
int diff;

a += cSame;
b += cSame;
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
int diff = cmpSeqName(a->tName, b->tName);
if (diff == 0)
    diff = pslScore(b) - pslScore(a);
return diff;
}

int pslCmpTargetStart(const void *va, const void *vb)
/* Compare to sort based on target start. */
{
const struct psl *a = *((struct psl **)va);
const struct psl *b = *((struct psl **)vb);
int diff = cmpSeqName(a->tName, b->tName);
if (diff == 0)
    diff = a->tStart - b->tStart;
return diff;
}

char *skipFile(char *fileSeq)
/* Skip over file: */
{
char *s = strchr(fileSeq, ':');
if (s != NULL)
    return s+1;
else
    {
    internalErr();
    return fileSeq;
    }
}

void parseFileSeq(char *spec, char **retFile, char **retSeq)
/* Parse out file:seq into file and seq. */
{
char *seq = skipFile(spec);
*retSeq = cloneString(seq);
*retFile = cloneStringZ(spec, seq - spec - 1);
}


void aliLines(char *pslName, char *faName, char *database,  char *type)
/* Show all the places that align. */
{
char *url = "../cgi-bin/webBlat";
char *sort = cgiUsualString("wb_sort", sortMenu[0]);
char *output = cgiUsualString("wb_output", outputMenu[0]);
boolean pslOut = startsWith("psl", output);
struct lineFile *lf = pslFileOpen(pslName);
struct psl *pslList = NULL, *psl, **pslArray;
int i, pslCount = 0;

while ((psl = pslNext(lf)) != NULL)
    {
    slAddHead(&pslList, psl);
    ++pslCount;
    }
lineFileClose(&lf);
slReverse(&pslList);

if (pslList == NULL)
    errAbort("Sorry, no matches found");

/* Keep an array in unsorted order */
AllocArray(pslArray, pslCount);
for (psl = pslList, i=0; psl != NULL; psl = psl->next, ++i)
    pslArray[i] = psl;


if (sameString(sort, "query,start"))
    {
    slSort(&pslList, pslCmpQuery);
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
	pslWriteHead(stdout);
    for (psl = pslList; psl != NULL; psl = psl->next)
	pslTabOut(psl, stdout);
    }
else
    {
    int lineIx = 0;
    printf("<H2>Web BLAT Search Results</H2>");
    printf("<TT><PRE>");
    printf("QUERY           SCORE START  END QSIZE IDENTITY CHRO STRAND  START    END      SPAN\n");
    printf("---------------------------------------------------------------------------------------------------\n");
    for (psl = pslList; psl != NULL; psl = psl->next)
	{
	printf("<A HREF=\"%s?wb_qType=%s&wb_psl=%s&wb_fa=%s&wb_doDetailLine=%d&wb_db=%s\">",
		url, type, pslName, faName, ptArrayIx(psl, pslArray, pslCount),
		database);
	printf("%-14s %5d %5d %5d %5d %5.1f%%  %4s  %2s  %9d %9d %6d",
	    psl->qName, pslScore(psl), psl->qStart+1, psl->qEnd, psl->qSize,
	    100.0 - pslCalcMilliBad(psl, TRUE) * 0.1,
	    skipFile(psl->tName), psl->strand, psl->tStart+1, psl->tEnd,
	    psl->tEnd - psl->tStart);
	printf("</A>\n");
	++lineIx;
	}
    }
pslFreeList(&pslList);
freez(&pslArray);
printf("</TT></PRE>");
}

struct dnaSeq *faReadNamedSeq(char *fileName, char *name, boolean isProt)
/* Return DNA sequence corresponding to named fasta record. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
DNA *dna;
int dnaSize;
char *dnaName;
struct dnaSeq *seq = NULL;
while (faSomeSpeedReadNext(lf, &dna, &dnaSize, &dnaName, !isProt))
    {
    if (sameString(name, dnaName))
        {
	AllocVar(seq);
	seq->name = cloneString(dnaName);
	seq->size = dnaSize;
	seq->dna = cloneStringZ(dna, dnaSize);
	break;
	}
    }
lineFileClose(&lf);
return seq;
}

struct dnaSeq *readSeqFrag(char *seqDir, char *fileName, char *seqName, int start, int end)
/* Read in fragment of sequence. */
{
char path[PATH_LEN];
safef(path, sizeof(path), "%s/%s", seqDir, fileName);
if (nibIsFile(path))
    {
    return nibLoadPart(path, start, end-start);
    }
else
    {
    struct twoBitFile *tbf = twoBitOpen(path);
    struct dnaSeq *seq = twoBitReadSeqFragLower(tbf, seqName, start, end);
    twoBitClose(&tbf);
    return seq;
    }
}


void doDetailLine()
/* Show alignment details - creating a html frame with
 * two pages: index and body. */
{
char *pslFileName = cgiString("wb_psl");
char *faFileName = cgiString("wb_fa");
char *type = cgiString("wb_qType");
boolean isTx = isTxType(type);
struct gfServerAt *server = findServer(isTx);
int pslLineIx = cgiInt("wb_doDetailLine");
int i;
struct lineFile *lf = pslFileOpen(pslFileName);
struct dnaSeq *qSeq = NULL, *tSeq = NULL;
char *tFileName, *tSeqName;
struct ffAli *ffAli;
int blockCount;
int tStart, tEnd;
boolean tIsRc = FALSE;
boolean protQuery = sameWord(type, "Protein");
char *bodyName = cloneString(rTempName(cfg->tempDir, "body", ".html"));
char *indexName = cloneString(rTempName(cfg->tempDir, "index", ".html"));
FILE *f;

/* Read in psl file and find the alignment line we're looking for. */
struct psl *psl = NULL;
for (i=0; i<=pslLineIx; ++i)
    {
    psl = pslNext(lf);
    if (psl == NULL)
        errAbort("Expecting at least %d lines, got %d in %s", pslLineIx+1, i+1, pslFileName);
    }
lineFileClose(&lf);

/* Read in fa file and find the query sequence. */
qSeq = faReadNamedSeq(faFileName, psl->qName, protQuery);
if (qSeq == NULL)
    errAbort("Couldn't find %s in %s", psl->qName, faFileName);


/* Parse out file:seq into file and seq, and load needed part of target seq. */
parseFileSeq(psl->tName, &tFileName, &tSeqName);
tStart = psl->tStart - 120;
if (tStart < 0) tStart = 0;
tEnd = psl->tEnd + 120;
if (tEnd > psl->tSize)
    tEnd = psl->tSize;
tSeq = readSeqFrag(server->seqDir, tFileName, tSeqName, tStart, tEnd);


/* Write out body. */
f = mustOpen(bodyName, "w");
htmStart(f, psl->qName);
blockCount = pslShowAlignment(psl, protQuery, psl->qName, qSeq, 0, qSeq->size,
	tSeqName, tSeq, tStart, tEnd, f);
htmEnd(f);
carefulClose(&f);
chmod(bodyName, 0666);

/* Write out index. */
f = mustOpen(indexName, "w");
htmStart(f, psl->qName);
fprintf(f, "<H3>Alignment of %s</H3>", psl->qName);
fprintf(f, "<A HREF=\"%s#cDNA\" TARGET=\"body\">%s</A><BR>\n", bodyName, psl->qName);
fprintf(f, "<A HREF=\"%s#genomic\" TARGET=\"body\">%s</A><BR>\n", bodyName, tSeqName);
for (i=1; i<=blockCount; ++i)
    {
    fprintf(f, "<A HREF=\"%s#%d\" TARGET=\"body\">block%d</A><BR>\n",
	    bodyName, i, i);
    }
fprintf(f, "<A HREF=\"%s#ali\" TARGET=\"body\">together</A><BR>\n", bodyName);
carefulClose(&f);
chmod(indexName, 0666);

/* Write (to stdout) the main html page containing just the frame info. */
puts("<FRAMESET COLS = \"13%,87% \" >");
printf("  <FRAME SRC=\"%s\" NAME=\"index\">\n", indexName);
printf("  <FRAME SRC=\"%s\" NAME=\"body\">\n", bodyName);
puts("<NOFRAMES><BODY></BODY></NOFRAMES>");
puts("</FRAMESET>");
puts("</HTML>\n");
}


void doBlat()
/* Do actual blatting */
{
char *seqText = cgiString("wb_seq");
bioSeq *seqList, *seq;
char *type = NULL;
boolean txServer = FALSE, protQuery = FALSE;
struct gfServerAt *server;
int i;
int conn;
FILE *f;
struct gfOutput *gvo;
struct hash *tFileCache = gfFileCacheNew();
char *pslName = cloneString(rTempName(cfg->tempDir, "wb", ".psl"));
char *faName = cloneString(rTempName(cfg->tempDir, "wb", ".fa"));

/* Figure out type and if it is a protein or a DNA based query */
type = cgiUsualString("wb_qType", bothQueryMenu[0]);

/* Get sequence from control into memory and also saved as fasta file. */
if (sameWord(type, bothQueryMenu[0]))
    {
    seqList = faSeqListFromMemText(seqText, FALSE);
    if (seqList == NULL)
	errAbort("Please go back and paste in a sequence");
    if (seqIsDna(seqList))
        {
	for (seq = seqList; seq != NULL; seq = seq->next)
	    {
	    toLowerN(seq->dna, seq->size);
	    dnaFilterToN(seq->dna, seq->dna);
	    }
	}
    else
        {
	protQuery = TRUE;
	type = "Protein";
	}
    }
else
    {
    protQuery = sameWord(type, "Protein");
    seqList = faSeqListFromMemText(seqText, !protQuery);
    if (seqList == NULL)
	errAbort("Please go back and paste in a sequence");
    }
if (seqList->name == NULL || seqList->name[0] == 0)
    {
    freez(&seqList->name);
    seqList->name = cloneString("query");
    }
faWriteAll(faName, seqList);

/* Set up output for blat. */
f = mustOpen(pslName, "w");
gvo = gfOutputPsl(0, protQuery, FALSE, f, FALSE, TRUE);
gvo->includeTargetFile = TRUE;

txServer = isTxType(type);
server = findServer(txServer);

/* Loop through sequences doing alignments and saving to file. */
for (seq = seqList; seq != NULL; seq = seq->next)
    {
    conn = gfConnect(server->host, server->port);
    if (txServer)
        {
	gvo->reportTargetStrand = TRUE;
	if (protQuery)
	    {
	    gfAlignTrans(&conn, server->seqDir, seq, 5, tFileCache, gvo);
	    }
	else
	    {
	    boolean isRna = sameWord(type, "RNA");
	    gfAlignTransTrans(&conn, server->seqDir, seq, FALSE, 5,
			    tFileCache, gvo, isRna);
	    if (!isRna)
	        {
		reverseComplement(seq->dna, seq->size);
		conn = gfConnect(server->host, server->port);
		gfAlignTransTrans(&conn, server->seqDir, seq, TRUE, 5,
		                        tFileCache, gvo, FALSE);
		}
	    }
	}
    else
        {
	gfAlignStrand(&conn, server->seqDir, seq, FALSE, 16, tFileCache, gvo);
	reverseComplement(seq->dna, seq->size);
	conn = gfConnect(server->host, server->port);
	gfAlignStrand(&conn, server->seqDir, seq, TRUE, 16, tFileCache, gvo);
	}
    gfOutputQuery(gvo, f);
    }
carefulClose(&f);

/* Display alignment results. */
aliLines(pslName, faName, server->name, type);
}

void doGetSeq()
/* Put up form that asks them to submit sequence. */
{
char *qType = NULL;
char **queryMenu = NULL;
int queryMenuSize = 0;
struct gfServerAt *serverList = NULL, *server;


printf("<H1>%s Web BLAT</H1>", cfg->company);
printf("<FORM ACTION=\"../cgi-bin/webBlat\" METHOD=\"POST\">\n");

/* Figure out whether we do nucleotide, translated, or both. */
if (cfg->serverList != NULL && cfg->transServerList != NULL)
    {
    queryMenu = bothQueryMenu;
    queryMenuSize = ArraySize(bothQueryMenu);
    serverList = cfg->serverList;
    }
else if (cfg->serverList != NULL)
    {
    queryMenu = nucQueryMenu;
    queryMenuSize = ArraySize(nucQueryMenu);
    serverList = cfg->serverList;
    }
else if (cfg->transServerList != NULL)
    {
    queryMenu = protQueryMenu;
    queryMenuSize = ArraySize(protQueryMenu);
    serverList = cfg->transServerList;
    }
else
    {
    errAbort("No servers configured!");
    }

/* Put up database control */
printf("Database: ");
printf("<SELECT NAME=\"wb_db\">\n");
printf("  <OPTION SELECTED>%s</OPTION>\n", serverList->name);
for (server = serverList->next; server != NULL; server = server->next)
    printf("  <OPTION>%s</OPTION>\n", server->name);
printf("</SELECT>\n");

/* Put up query type control. */
qType = cgiUsualString("wb_qType", queryMenu[0]);
printf("Query Type: ");
cgiMakeDropList("wb_qType", queryMenu, queryMenuSize, qType);
printf("<BR>\n");

/* Put up sort and output type controls. */
printf("Sort By: ");
cgiMakeDropList("wb_sort", sortMenu, ArraySize(sortMenu), sortMenu[0]);
printf("Output: ");
cgiMakeDropList("wb_output", outputMenu, ArraySize(outputMenu), outputMenu[0]);
cgiMakeButton("Submit", "Submit");
printf("<BR>\n");

cgiMakeTextArea("wb_seq", "", 10, 60);
printf("<BR>\n");

printf("Please paste in some sequence and press submit. You can submit multiple "
       "sequences at once if they are in fasta format (where each sequence has "
       "a header line that starts with > and contains the name of the sequence)");

printf("</FORM>");
}

void webBlat()
/* Parse out CGI variables and decide which page to put up. */
{
if (cgiVarExists("wb_help"))
    htmShell("Web BLAT Help", doHelp, NULL);
else if (cgiVarExists("wb_seq"))
    htmShell("Web BLAT Results", doBlat, NULL);
else if (cgiVarExists("wb_doDetailLine"))
    {
    puts("Content-Type:text/html");
    puts("\n");
    doDetailLine();
    }
else
    htmShell("Web BLAT", doGetSeq, NULL);
}

int main(int argc, char *argv[])
/* Process command line. */
{
boolean isFromWeb = cgiIsOnWeb();
htmlPushEarlyHandlers();
dnaUtilOpen();
if (!isFromWeb && !cgiSpoof(&argc, argv))
    usage();
cfg = gfWebConfigRead("webBlat.cfg");
if (cfg->tempDir == NULL)
    errAbort("No tempDir set in webBlat.cfg");
if (cfg->background != NULL)
    htmlSetBackground(cfg->background);
webBlat();
return 0;
}
