/* webPcr - Web CGI Program For Fast In-Silico PCR Using gfServer. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "errAbort.h"
#include "errCatch.h"
#include "htmshell.h"
#include "dnautil.h"
#include "cheapCgi.h"
#include "gfPcrLib.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "webPcr - Web CGI Program For Fast In-Silico PCR Using gfServer\n"
  "This program is not generally meant to be run from the command line.\n"
  );
}

struct gfServer
/* A gfServer with an in-memory index. */
    {
    struct gfServer *next;
    char *host;		/* IP Address of machine running server. */
    char *port;		/* IP Port on host. */
    char *seqDir;	/* Where associated sequence lives. */
    char *name;		/* Name user sees. */
    };

char *company = "World Famous";
char *background = NULL;
struct gfServer *serverList;

struct gfServer *findServer()
/* Find active server */
{
struct gfServer *server = serverList;
if (cgiVarExists("wp_db"))
     {
     char *db = cgiString("wp_db");
     for (server = serverList; server != NULL; server = server->next)
          {
	  if (sameString(db, server->name))
	      break;
	  }
     if (server == NULL)
          errAbort("wp_db %s not found", db);
     }
return server;
}

void readConfig(char *fileName)
/* Read configuration file into globals. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line, *word;
struct hash *uniqHash = newHash(0);

while (lineFileNextReal(lf, &line))
    {
    word = nextWord(&line);
    if (sameWord("company", word))
        {
	company = cloneString(trimSpaces(line));
	}
    else if (sameWord("gfServer", word))
        {
	struct gfServer *server;
	char *dupe = cloneString(line);
	AllocVar(server);
	server->host = nextWord(&dupe);
	server->port = nextWord(&dupe);
	server->seqDir = nextWord(&dupe);
	server->name = trimSpaces(dupe);
	if (server->name == NULL || server->name[0] == 0)
	    errAbort("Badly formed gfServer command line %d of %s:\n%s",
	    	lf->lineIx, fileName, line);
	if (hashLookup(uniqHash, server->name))
	    errAbort("Duplicate gfServer name %s line %d of %s",
	    	server->name, lf->lineIx, fileName);
	hashAdd(uniqHash, server->name, NULL);
	slAddTail(&serverList, server);
	}
    else if (sameWord("background", word))
        {
	background = cloneString(trimSpaces(line));
	}
    else
        {
	errAbort("Unrecognized command %s line %d of  %s", word, lf->lineIx, fileName);
	}
    }

if (serverList == NULL)
    errAbort("no gfServer's specified in %s", fileName);
freeHash(&uniqHash);
}

void doGetPrimers(char *fPrimer, char *rPrimer, int maxSize, int minPerfect, int minGood)
/* Put up form to get primers. */
{
struct gfServer *server;
printf("<H1>%s In Silico PCR</H1>", company);
printf("<FORM ACTION=\"../cgi-bin/webPcr\" METHOD=\"GET\">\n");
printf("Forward Primer: ");
cgiMakeTextVar("wp_f", fPrimer, 22);
printf(" Reverse Primer: ");
cgiMakeTextVar("wp_r", rPrimer, 22);
printf("<BR>");
printf("Max Product Size: ");
cgiMakeIntVar("wp_size", maxSize, 5);
printf(" Min Perfect Match Size: ");
cgiMakeIntVar("wp_perfect", minPerfect, 2);
printf(" Min Good Match Size: ");
cgiMakeIntVar("wp_good", minGood, 2);
printf("<BR>");
printf("Database: ");
printf("<SELECT NAME=\"wp_db\">\n");
printf("  <OPTION SELECTED>%s</OPTION>\n", serverList->name);
for (server = serverList->next; server != NULL; server = server->next)
    printf("  <OPTION>%s</OPTION>\n", server->name);
printf("</SELECT>\n");
printf(" <A HREF=\"../cgi-bin/webPcr?wp_help=on\" TARGET=\"_blank\">User Guide</A> \n");
cgiMakeButton("Submit", "Submit");
printf("</FORM>\n");
}

boolean doPcr(char *fPrimer, char *rPrimer, int maxSize, int minPerfect, int minGood)
/* Do the PCR, and show results. */
{
struct errCatch *errCatch = errCatchNew();
boolean ok = FALSE;

if (errCatchStart(errCatch))
    {
    struct gfPcrInput *gpi;
    struct gfPcrOutput *gpoList;
    struct gfServer *server = findServer();

    AllocVar(gpi);
    gpi->fPrimer = fPrimer;
    gpi->rPrimer = rPrimer;
    gpoList = gfPcrViaNet(server->host, server->port, 
    	server->seqDir, gpi, maxSize, minPerfect, minGood);
    if (gpoList != NULL)
	{
	printf("<TT><PRE>");
	gfPcrOutputWriteAll(gpoList, "fa", "stdout");
	printf("</PRE></TT>");
        ok = TRUE;
	}
    else
	errAbort("No matches to %s %s in %s", fPrimer, rPrimer, server->name);
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
     {
     warn(errCatch->message->string);
     }
errCatchFree(&errCatch); 
return ok;
}

char *makePrimer(char *s)
/* Make primer (lowercased DNA) out of text.  Complain if
 * it is too short or too long. */
{
int size = dnaFilteredSize(s);
int realSize;
char *primer = needMem(size+1);
dnaFilter(s, primer);
realSize = size - countChars(primer, 'n');
if (realSize < 10 || realSize < size/2)
   errAbort("%s does not seem to be a good primer", s);
return primer;
}

void doHelp()
/* Print up help page */
{
puts(
"<H1>In-Silico PCR User Guide</H1>\n"
"In-Silico PCR searches a sequence database with a pair of\n"
"PCR primers.  It uses an indexing strategy to do this quickly.\n"
"\n"
"<H2>Controls</H2>\n"
"<B>Forward Primer</B> - The primers must be at least 15 bases long.<BR>\n"
"<B>Reverse Primer</B> - This is on the opposite strand from the forward primer.<BR>\n"
"<B>Max Product Size</B> - Maximum size of amplified region.<BR>\n"
"<B>Min Perfect Match</B> - Number of bases that match exactly on 3' end of primers.  Must be at least 15.<BR>\n"
"<B>Min Good Match</B> - Number of bases on 3' end of primers where at least 2 out of 3 bases match.<BR>\n"
"<B>Database</B> - Which sequence database to search.<BR>\n"
"\n"
"<H2>Output</H2>\n"
"When the search is successful the output is a fasta format sequence\n"
"file containing all the regions in the database that lie between the \n"
"primer pair.  The fasta header describes the region in the database\n"
"and the primers.  The fasta body is capitalized where the primer\n"
"sequence matches the database sequence and lowercase elsewhere.  Here\n"
"is an example:<BR>\n"
"<TT><PRE>\n"
">chr22:31000551+31001000  TAACAGATTGATGATGCATGAAATGGG CCCATGAGTGGCTCCTAAAGCAGCTGC\n"
"TtACAGATTGATGATGCATGAAATGGGgggtggccaggggtggggggtga\n"
"gactgcagagaaaggcagggctggttcataacaagctttgtgcgtcccaa\n"
"tatgacagctgaagttttccaggggctgatggtgagccagtgagggtaag\n"
"tacacagaacatcctagagaaaccctcattccttaaagattaaaaataaa\n"
"gacttgctgtctgtaagggattggattatcctatttgagaaattctgtta\n"
"tccagaatggcttaccccacaatgctgaaaagtgtgtaccgtaatctcaa\n"
"agcaagctcctcctcagacagagaaacaccagccgtcacaggaagcaaag\n"
"aaattggcttcacttttaaggtgaatccagaacccagatgtcagagctcc\n"
"aagcactttgctctcagctccacGCAGCTGCTTTAGGAGCCACTCATGaG\n"
"</PRE></TT>\n"
"The + between the coordinates in the fasta header indicates that\n"
"this is on the positive strand.  \n"
);
}

void doMiddle()
/* Decide which page to put up. */
/* Put up web page etc. */
{
int maxSize = 4000;
int minPerfect = 15;
int minGood = 15;
char *fPrimer = "";
char *rPrimer = "";

if (cgiVarExists("wp_size"))
     maxSize = cgiInt("wp_size");
if (cgiVarExists("wp_perfect"))
     minPerfect = cgiInt("wp_perfect");
if (cgiVarExists("wp_good"))
     minGood = cgiInt("wp_good");
if (minPerfect < 15)
     minPerfect = 15;
if (minGood < minPerfect)
     minGood = minPerfect;

if (cgiVarExists("wp_help"))
    {
    doHelp();
    return;
    }
else if (cgiVarExists("wp_f") && cgiVarExists("wp_r"))
    {
    fPrimer = makePrimer(cgiString("wp_f"));
    rPrimer = makePrimer(cgiString("wp_r"));
    if (doPcr(fPrimer, rPrimer, maxSize, minPerfect, minGood))
         return;
    }
doGetPrimers(fPrimer, rPrimer, maxSize, minPerfect, minGood);
}

static void earlyWarningHandler(char *format, va_list args)
/* Write an error message so user can see it before page is really started. */
{
static boolean initted = FALSE;
if (!initted)
    {
    htmlStart("Very Early Error");
    initted = TRUE;
    }
printf("%s", htmlWarnStartPattern());
htmlVaParagraph(format,args);
printf("%s", htmlWarnEndPattern());
}

void earlyAbortHandler()
/* Exit close web page during early abort. */
{
printf("</BODY></HTML>");
exit(0);
}


int main(int argc, char *argv[])
/* Process command line. */
{
boolean isFromWeb = cgiIsOnWeb();
pushWarnHandler(earlyWarningHandler);
pushAbortHandler(earlyAbortHandler);
dnaUtilOpen();
readConfig("webPcr.cfg");
if (!isFromWeb && !cgiSpoof(&argc, argv))
    usage();
if (background != NULL)
    {
    htmlSetBackground(background);
    }
htmShell("In-Silico PCR", doMiddle, NULL);
return 0;
}
