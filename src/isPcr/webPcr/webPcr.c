/* webPcr - Web CGI Program For Fast In-Silico PCR Using gfServer. */
/* Copyright 2004 Jim Kent.  All rights reserved. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "errAbort.h"
#include "errCatch.h"
#include "htmshell.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "cheapcgi.h"
#include "genoFind.h"
#include "gfPcrLib.h"
#include "gfWebLib.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "webPcr - Web CGI Program For Fast In-Silico PCR Using gfServer\n"
  "This program is not generally meant to be run from the command line.\n"
  );
}

struct gfWebConfig *cfg;	/* Our configuration. */

void doGetPrimers(char *fPrimer, char *rPrimer, int maxSize, int minPerfect, int minGood)
/* Put up form to get primers. */
{
struct gfServerAt *server;
printf("<H1>%s In Silico PCR</H1>", cfg->company);
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
printf("  <OPTION SELECTED>%s</OPTION>\n", cfg->serverList->name);
for (server = cfg->serverList->next; server != NULL; server = server->next)
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
    struct gfServerAt *server = gfWebFindServer(cfg->serverList, "wp_db");

    AllocVar(gpi);
    gpi->fPrimer = fPrimer;
    gpi->rPrimer = rPrimer;
    struct gfConnection *conn = gfConnect(server->host, server->port, NULL, NULL);
    gpoList = gfPcrViaNet(conn, server->seqDir, gpi, maxSize, minPerfect, minGood);
    gfDisconnect(&conn);
    if (gpoList != NULL)
	{
	printf("<TT><PRE>");
	gfPcrOutputWriteAll(gpoList, "fa", NULL, "stdout");
	printf("</PRE></TT>");
        ok = TRUE;
	}
    else
	errAbort("No matches to %s %s in %s", fPrimer, rPrimer, server->name);
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
     {
     warn("%s", errCatch->message->string);
     }
errCatchFree(&errCatch); 
return ok;
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
/* Parse out CGI variables and decide which page to put up. */
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
    fPrimer = gfPcrMakePrimer(cgiString("wp_f"));
    rPrimer = gfPcrMakePrimer(cgiString("wp_r"));
    if (doPcr(fPrimer, rPrimer, maxSize, minPerfect, minGood))
         return;
    }
doGetPrimers(fPrimer, rPrimer, maxSize, minPerfect, minGood);
}

int main(int argc, char *argv[])
/* Process command line. */
{
boolean isFromWeb = cgiIsOnWeb();
htmlPushEarlyHandlers();
dnaUtilOpen();
cfg = gfWebConfigRead("webPcr.cfg");
if (!isFromWeb && !cgiSpoof(&argc, argv))
    usage();
if (cfg->background != NULL)
    {
    htmlSetBackground(cfg->background);
    }
htmShell("In-Silico PCR", doMiddle, NULL);
return 0;
}
