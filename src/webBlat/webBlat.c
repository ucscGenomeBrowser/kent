/* webBlat - CGI Applet for Blat. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "errAbort.h"
#include "errCatch.h"
#include "portable.h"
#include "htmshell.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"
#include "psl.h"
#include "cheapCgi.h"
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

void doHelp()
/* Put up help page. */
{
uglyf("I'm just not very helpful");
}

char *protQueryMenu[] = {"Protein", "Translated DNA", "Translated RNA"};
char *nucQueryMenu[] = {"DNA", "RNA", };
char *bothQueryMenu[] = {"DNA", "RNA", "Protein", "Translated DNA", "Translated RNA"};
char *sortMenu[] = {"query,score", "query,start", "chrom,score", "chrom,start", "score"};
char *outputMenu[] = {"hyperlink", "psl", "psl no header"};

int countSame(char *a, char *b)
/* Count number of characters that from start in a,b that are same. */
{
char c;
int i;
int count = 0;
for (i=0; ; ++i)
   {
   c = a[i];
   if (b[i] != c)
       break;
   if (c == 0)
       break;
   ++count;
   }
return count;
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


void aliLines(char *pslName, char *faName, char *database,  char *type)
/* Show all the places that align. */
{
char *url = "../cgi-bin/webBlat";
char *sort = cgiUsualString("wb_sort", sortMenu[0]);
char *output = cgiUsualString("wb_output", outputMenu[0]);
boolean pslOut = startsWith("psl", output);
struct lineFile *lf = pslFileOpen(pslName);
struct psl *pslList = NULL, *psl;

while ((psl = pslNext(lf)) != NULL)
    {
    slAddHead(&pslList, psl);
    }
lineFileClose(&lf);
if (pslList == NULL)
    errAbort("Sorry, no matches found");

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
	printf("<A HREF=\"%s?wb_qType=%s&wb_psl=%s&wb_fa=%s&wb_doDetailLine=%d\">",
		url, type, pslName, faName, lineIx);
	printf("%-14s %5d %5d %5d %5d %5.1f%%  %4s  %2s  %9d %9d %6d",
	    psl->qName, pslScore(psl), psl->qStart+1, psl->qEnd, psl->qSize,
	    100.0 - pslCalcMilliBad(psl, TRUE) * 0.1,
	    psl->tName, psl->strand, psl->tStart+1, psl->tEnd,
	    psl->tEnd - psl->tStart);
	printf("</A>\n");
	++lineIx;
	}
    }
pslFreeList(&pslList);
printf("</TT></PRE>");
}

void doDetailLine()
/* Show alignment details. */
{
uglyf("Doing details");
}


boolean doBlat()
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
char *trashDir = "../trash";
char *pslName = cloneString(rTempName(trashDir, "wb", ".psl"));
char *faName = cloneString(rTempName(trashDir, "wb", ".fa"));

/* Figure out type and if it is a protein or a DNA based query */
type = cgiUsualString("wb_qType", nucQueryMenu[0]);
for (i=0; i<ArraySize(protQueryMenu); ++i)
    {
    if (sameWord(type, protQueryMenu[i]))
         txServer = TRUE;
    }
protQuery = sameWord(type, "Protein");

/* Get sequence from control into memory and also saved as fasta file. */
seqList = faSeqListFromMemText(seqText, !protQuery);
if (seqList == NULL)
    {
    warn("Please paste in a sequence");
    return FALSE;
    }
faWriteAll(faName, seqList);

/* Set up output for blat. */
f = mustOpen(pslName, "w");
gvo = gfOutputPsl(0, protQuery, FALSE, f, FALSE, TRUE);

/* Find gfServer. */
if (txServer)
    server = gfWebFindServer(cfg->transServerList, "wb_db");
else
    server = gfWebFindServer(cfg->serverList, "wb_db");

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
	    boolean isRna = sameWord("type", "RNA");
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
return TRUE;
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

/* Put up sort and output type controls. */
printf("Sort By: ");
cgiMakeDropList("wb_sort", sortMenu, ArraySize(sortMenu), sortMenu[0]);
printf("Output: ");
cgiMakeDropList("wb_output", outputMenu, ArraySize(outputMenu), outputMenu[0]);

cgiMakeButton("Submit", "Submit");

cgiMakeTextArea("wb_seq", "", 10, 60);

printf("</FORM>");
}

void doMiddle()
/* Parse out CGI variables and decide which page to put up. */
{
if (cgiVarExists("wb_help"))
    {
    doHelp();
    return;
    }
else if (cgiVarExists("wb_seq"))
    {
    if (doBlat())
        return;
    }
else if (cgiVarExists("wb_doDetailLine"))
    {
    doDetailLine();
    return;
    }
doGetSeq();
}

int main(int argc, char *argv[])
/* Process command line. */
{
boolean isFromWeb = cgiIsOnWeb();
htmlPushEarlyHandlers();
dnaUtilOpen();
cfg = gfWebConfigRead("webBlat.cfg");
if (!isFromWeb && !cgiSpoof(&argc, argv))
    usage();
if (cfg->background != NULL)
    {
    htmlSetBackground(cfg->background);
    }
htmShell("Web BLAT", doMiddle, NULL);
return 0;
}
