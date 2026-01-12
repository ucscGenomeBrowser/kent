/* hgPcr - In-silico PCR CGI for UCSC. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "hash.h"
#include "errAbort.h"
#include "errCatch.h"
#include "hCommon.h"
#include "hgConfig.h"
#include "dystring.h"
#include "jksql.h"
#include "linefile.h"
#include "dnautil.h"
#include "fa.h"
#include "psl.h"
#include "genoFind.h"
#include "gfPcrLib.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "hdb.h"
#include "hui.h"
#include "cart.h"
#include "dbDb.h"
#include "blatServers.h"
#include "targetDb.h"
#include "pcrResult.h"
#include "trashDir.h"
#include "web.h"
#include "botDelay.h"
#include "oligoTm.h"
#include "trackHub.h"
#include "hubConnect.h"
#include "obscure.h"
#include "chromAlias.h"
#include "jsHelper.h"


struct cart *cart;	/* The user's ui state. */
struct hash *oldVars = NULL;

/* for earlyBotCheck() function at the beginning of main() */
#define delayFraction   1.0     /* standard penalty for most CGIs */
static boolean issueBotWarning = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgPcr - In-silico PCR CGI for UCSC\n"
  "usage:\n"
  "   hgPcr XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

struct pcrServer
/* Information on a server running on genomic assembly sequence. */
   {
   struct pcrServer *next;  /* Next in list. */
   char *db;		/* Database name. */
   char *genome;	/* Genome name. */
   char *description;	/* Assembly description */
   char *host;		/* Name of machine hosting server. */
   char *port;		/* Port that hosts server. */
   char *seqDir;	/* Directory of sequence files. */
   boolean isDynamic;   /* is a dynamic server */
   char* genomeDataDir; /* genome name for dynamic gfServer  */
   };

struct targetPcrServer
/* Information on a server running on non-genomic sequence, e.g. mRNA,
 * that has been aligned to a particular genomic assembly. */
   {
   struct targetPcrServer *next;  /* Next in list. */
   char *host;		/* Name of machine hosting server. */
   char *port;		/* Port that hosts server. */
   struct targetDb *targetDb;     /* All of the info about the target. */
   };

struct pcrServer *getTrackHubServers()
/* Get the list of track hubs that have PCR services. */
{
struct pcrServer *serverList = NULL, *server;

struct dbDb *dbDbList = trackHubGetPcrServers();

for(; dbDbList; dbDbList = dbDbList->next)
    {
    AllocVar(server);
    server->db = dbDbList->name;
    server->genome = dbDbList->organism;
    server->description = dbDbList->description;
    trackHubGetPcrParams(server->db, &server->host, &server->port, &server->genomeDataDir);
    struct trackHubGenome *genome = trackHubGetGenome(server->db);
    server->seqDir = cloneString(genome->twoBitPath);
    char *ptr = strrchr(server->seqDir, '/');
    // we only want the directory name
    if (ptr != NULL)
         *ptr = 0;
    slAddHead(&serverList, server);
    }

return serverList;
}

struct pcrServer *getServerList()
/* Get list of available servers. */
{
struct pcrServer *serverList = NULL, *server;
struct sqlConnection *conn = hConnectCentral();
struct sqlResult *sr;
char **row;

serverList = getTrackHubServers();

/* Do a little join to get data to fit into the pcrServer.  Check for newer
 * dynamic flag and allow with or without it.  For debugging, one set the
 * variable blatServersTbl to some db.table to pick up settings from somewhere
 * other than dbDb.blatServers. */
char *blatServersTbl = cfgOptionDefault("blatServersTbl", "blatServers");
boolean haveDynamic = sqlColumnExists(conn, blatServersTbl, "dynamic");
char query[512];
sqlSafef(query, sizeof(query),
         "select dbDb.name,dbDb.genome,dbDb.description,blatServers.host,"
         "blatServers.port,dbDb.nibPath, %s "
         "from dbDb, %s blatServers where "
         "dbDb.name = blatServers.db "
         "and blatServers.canPcr = 1 order by dbDb.orderKey",
         (haveDynamic ? "blatServers.dynamic" : "0"), blatServersTbl);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(server);
    server->db = cloneString(row[0]);
    server->genome = cloneString(row[1]);
    server->description = cloneString(row[2]);
    server->host = cloneString(row[3]);
    server->port = cloneString(row[4]);
    server->seqDir = hReplaceGbdbSeqDir(row[5], server->db);
    if (atoi(row[6]))
        {
        server->isDynamic = TRUE;
        server->genomeDataDir = cloneString(server->db);  // directories by database name for database genomes
        }
    slAddHead(&serverList, server);
    }
sqlFreeResult(&sr);
hDisconnectCentral(&conn);
if (serverList == NULL)
    errAbort("Sorry, no PCR servers are available");
slReverse(&serverList);
return serverList;
}

struct pcrServer *findServer(char *db, struct pcrServer *serverList)
/* Return server for given database.  Db can either be
 * database name or description. */
{
struct pcrServer *server;
for (server = serverList; server != NULL; server = server->next)
    {
    if (sameString(db, server->db))
        return server;
    }
errAbort("Can't find a server for PCR database %s\n", db);
return NULL;
}

struct targetPcrServer *getTargetServerList(char *db, char *name)
/* Get list of available non-genomic-assembly target pcr servers associated 
 * with db (and name, if not NULL).  There may be none -- that's fine. */
{
if (trackHubDatabase(db))
    return NULL;
struct targetPcrServer *serverList = NULL, *server;
struct sqlConnection *conn = hConnectCentral();
struct sqlConnection *conn2 = hAllocConn(db);
struct sqlResult *sr;
char **row;
struct dyString *dy = dyStringNew(0);

sqlDyStringPrintf(dy, 
      "select b.host, b.port, t.* from targetDb as t, blatServers as b "
      "where b.db = t.name and t.db = '%s' and b.canPcr = 1 ",
      db);
if (isNotEmpty(name))
    sqlDyStringPrintf(dy, "and t.name = '%s' ", name);
sqlDyStringPrintf(dy, "order by t.priority");
sr = sqlGetResult(conn, dy->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    /* Keep this server only if its timestamp is newer than the tables
     * and file on which it depends. */
    struct targetDb *target = targetDbMaybeLoad(conn2, row+2);
    if (target != NULL)
	{
	AllocVar(server);
	server->host = cloneString(row[0]);
	server->port = cloneString(row[1]);
	server->targetDb = target;
	slAddHead(&serverList, server);
	}
    }
dyStringFree(&dy);
sqlFreeResult(&sr);
hDisconnectCentral(&conn);
hFreeConn(&conn2);
slReverse(&serverList);
return serverList;
}

void doHelp()
/* Print up help page */
{
printf(
"In-Silico PCR V%s searches a sequence database with a pair of\n"
"PCR primers, using the BLAT index for fast performance.\n"
"See an example\n"
"<a href='https://youtu.be/U8_QYwmdGYU'"
"target='_blank'>video</a>\n"
"on our YouTube channel.<br>\n"
"<ul>\n"
"<li>This tool is not guaranteed to find absolutely all off-target locations,\n"
"it is optimized for targets with higher identities. For\n"
"use in primer design, especially in repetitive regions, consider additional validation with tools such as\n"
"<a target='_blank' href='https://www.ncbi.nlm.nih.gov/tools/primer-blast/'>"
"primer blast</a>.</li>\n"
"<li>If you are looking for matches to RT-PCR primers, where primers often straddle intron-exon boundaries, change the <b>Target</b> option and select "
"a gene transcript set. This feature is only available for human and mouse assemblies.</li>\n"
"</ul>\n"
"<H3>Configuration Options</H3>\n"
"<B>Genome and Assembly</B> - The sequence database to search.<BR>\n"
"<B>Target</B> - If available, choose to query transcribed sequences.<BR>\n" 
"<B>Forward Primer</B> - Must be at least 15 bases in length.<BR>\n"
"<B>Reverse Primer</B> - On the opposite strand from the forward primer. Minimum length of 15 bases.<BR>\n"
"<B>Max Product Size</B> - Maximum size of amplified region.<BR>\n"
"<B>Min Perfect Match</B> - Number of bases that match exactly on 3' end of primers.  Minimum match size is 15.<BR>\n"
"<B>Min Good Match</B> - Number of bases on 3' end of primers where at least 2 out of 3 bases match.<BR>\n"
"<B>Flip Reverse Primer</B> - Invert the sequence order of the reverse primer and complement it.<BR>\n"
"<B>Append to existing PCR result</B> - Add this PCR result list to the currently existing track of PCR results.<BR>\n"
"\n"
"<H3>Output</H3>\n"
"When successful, the search returns a sequence output file in fasta format \n"
"containing all sequence in the database that lie between and include the \n"
"primer pair.  The fasta header describes the region in the database\n"
"and the primers.  The fasta body is capitalized in areas where the primer\n"
"sequence matches the database sequence and in lower-case elsewhere.  Here\n"
"is an example from human:<BR>\n"
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
"The + between the coordinates in the fasta header indicates \n"
"this is on the positive strand.  \n", gfVersion
);
}

#define ORGFORM_KEEP_ORG "document.orgForm.org.value = " \
    " document.mainForm.org.options[document.mainForm.org.selectedIndex].value; "
#define ORGFORM_KEEP_DB " document.orgForm.db.value = " \
    " document.mainForm.db.options[document.mainForm.db.selectedIndex].value; "
#define ORGFORM_KEEP_PARAMS \
    " document.orgForm.wp_f.value = document.mainForm.wp_f.value; " \
    " document.orgForm.wp_r.value = document.mainForm.wp_r.value; " \
    " document.orgForm.wp_size.value = document.mainForm.wp_size.value; " \
    " document.orgForm.wp_perfect.value = document.mainForm.wp_perfect.value; " \
    " document.orgForm.wp_good.value = document.mainForm.wp_good.value; "
#define ORGFORM_RESET_DB " document.orgForm.db.value = 0; "
#define ORGFORM_RESET_TARGET " document.orgForm.wp_target.value = \"\"; "
#define ORGFORM_SUBMIT " document.orgForm.submit();"


void showGenomes(char *genome, struct pcrServer *serverList)
/* Put up drop-down list with genomes on it. */
{
struct hash *uniqHash = hashNew(8);
struct pcrServer *server;
char *onChangeText = 
    ORGFORM_KEEP_PARAMS 
    ORGFORM_KEEP_ORG
    ORGFORM_RESET_DB
    ORGFORM_RESET_TARGET
    ORGFORM_SUBMIT 
    ;

printf("<SELECT NAME=\"org\" id='org'>\n");
jsOnEventById("change", "org", onChangeText);
for (server = serverList; server != NULL; server = server->next)
    {
    if (!hashLookup(uniqHash, server->genome))
        {
	hashAdd(uniqHash, server->genome, NULL);
	printf("  <OPTION%s VALUE=\"%s\">%s</OPTION>\n", 
	    (sameWord(genome, server->genome) ? " SELECTED" : ""), 
	    server->genome, hubConnectSkipHubPrefix(server->genome));
	}
    }
printf("</SELECT>\n");
hashFree(&uniqHash);
}

void showAssemblies(char *genome, char *db, struct pcrServer *serverList,
		    boolean submitOnClick)
/* Put up drop-down list with assemblies on it. */
{
struct pcrServer *server;
char *onChangeText =
    ORGFORM_KEEP_PARAMS 
    ORGFORM_KEEP_ORG
    ORGFORM_KEEP_DB
    ORGFORM_RESET_TARGET
    ORGFORM_SUBMIT 
    ;

printf("<SELECT NAME=\"db\" id='db'>\n");
if (submitOnClick)
    jsOnEventById("change", "db", onChangeText);
for (server = serverList; server != NULL; server = server->next)
    {
    if (sameWord(genome, server->genome))
	printf("  <OPTION%s VALUE=%s>%s</OPTION>\n", 
	    (sameString(db, server->db) ? " SELECTED" : ""), 
	    server->db, server->description);
    }
printf("</SELECT>\n");
}

void showTargets(char *target, struct targetPcrServer *serverList)
/* Put up drop-down list with targets on it. */
{
struct targetPcrServer *server;

printf("<SELECT NAME=\"wp_target\">\n");
printf("  <OPTION%s VALUE=genome>genome assembly</OPTION>\n", 
       (sameString(target, "genome") ? " SELECTED" : ""));
for (server = serverList; server != NULL; server = server->next)
    {
    printf("  <OPTION%s VALUE=%s>%s</OPTION>\n", 
	   (sameString(target, server->targetDb->name) ? " SELECTED" : ""), 
	   server->targetDb->name, server->targetDb->description);
    }
printf("</SELECT>\n");
}

void redoDbAndOrgIfNoServer(struct pcrServer *serverList, char **pDb, char **pOrg)
/* Check that database and organism are on our serverList.  If not, then update
 * them to first thing that is. */
{
struct pcrServer *server, *orgServer = NULL;
char *organism = *pOrg;
char *db = *pDb;
boolean gotDb = FALSE;

/*  Find first server for our organism */
for (server = serverList; server != NULL; server = server->next)
    {
    if (sameString(server->genome, organism))
         {
	 orgServer = server;
	 break;
	 }
    }

/* If no server, change our organism to the one of the first server in list. */
if (orgServer == NULL)
    {
    orgServer = serverList;
    *pOrg = organism = orgServer->genome;
    }

/* Search for our database. */
for (server = serverList; server != NULL; server = server->next)
    {
    if (sameString(db, server->db))
        {
	gotDb = TRUE;
	break;
	}
    }

/* If no server for db, change db. */
if (!gotDb)
    {
    if (differentString(db, orgServer->db))
	printf("<HR><P><EM><B>Note:</B> In-Silico PCR is not available for %s %s; "
	       "defaulting to %s %s</EM></P><HR>\n",
	       hGenome(db), hFreezeDate(db), organism, hFreezeDate(orgServer->db));
    *pDb = db = orgServer->db;
    }
}

void doGetPrimers(char *db, char *organism, struct pcrServer *serverList,
	char *fPrimer, char *rPrimer, int maxSize, int minPerfect, int minGood,
	boolean flipReverse, boolean appendToResults)
/* Put up form to get primers. */
{
redoDbAndOrgIfNoServer(serverList, &db, &organism);
struct sqlConnection *conn = hConnectCentral();
boolean gotTargetDb = sqlTableExists(conn, "targetDb");
hDisconnectCentral(&conn);
jsIncludeAutoCompleteLibs();

printf("<FORM ACTION=\"../cgi-bin/hgPcr\" METHOD=\"GET\" NAME=\"mainForm\">\n");
cartSaveSession(cart);

printf("<TABLE BORDER=0 WIDTH=\"96%%\" COLS=7><TR>\n");

printf("<TD class='searchCell'><center>\n");
char *searchPlaceholder = "Search any species, genome or assembly name";
char *searchBarId = "genomeSearch";
printGenomeSearchBar(searchBarId, searchPlaceholder, NULL, TRUE, "Genome:", NULL);
jsInlineF(
    "function isPcrSelect(selectEle, item) {\n"
    "   if (item.disabled || !item.genome) return;\n"
    "   selectEle.innerHTML = item.label;\n"
    ORGFORM_KEEP_PARAMS
    "\n"
    "document.orgForm.org.value = '0';\n"
    "document.orgForm.db.value = item.genome;\n"
    ORGFORM_RESET_TARGET
    "\n"
    ORGFORM_SUBMIT
    "\n"
    "}\n\n"
    "function onSearchError(jqXHR, textStatus, errorThrown, term) {\n"
    "    return [{label: 'No genomes found', value: '', genome: '', disabled: true}];\n"
    "}\n\n"
    "document.addEventListener(\"DOMContentLoaded\", () => {\n"
    "    // bind the actual <select> to the function isPcrSelect, that way\n"
    "    // initSpeciesAutoCompleteDropdown can call the function\n"
    "    let selectEle = document.getElementById(\"genomeLabel\");\n"
    "    let boundSelect = isPcrSelect.bind(null, selectEle);\n"
    "    initSpeciesAutoCompleteDropdown('%s', boundSelect, null, null, null, onSearchError);\n"
    "    // make the search button trigger the autocomplete manually\n"
    "    let btn = document.getElementById(\"%sButton\");\n"
    "    btn.addEventListener(\"click\", () => {\n"
    "        let val = document.getElementById(\"%s\").value;\n"
    "        $(\"[id=\'%s\']\").autocompleteCat(\"search\", val);\n"
    "    });\n"
    "});\n"
    , searchBarId, searchBarId, searchBarId, searchBarId
);

printf("</center></td><TD><CENTER>\n");
printf("Assembly:<BR><span id='genomeLabel'>%s</span>", getCurrentGenomeLabel(db));
printf("</CENTER></TD>\n");

if (gotTargetDb)
    {
    struct targetPcrServer *targetServerList = getTargetServerList(db, NULL);
    if (targetServerList != NULL)
	{
	char *target = cartUsualString(cart, "wp_target", "genome");
	printf("%s", "<TD><CENTER>\n");
	printf("Target:<BR>");
	showTargets(target, targetServerList);
	printf("%s", "</CENTER></TD>\n");
	}
    else
	cgiMakeHiddenVar("wp_target", "genome");
    }
else
    cgiMakeHiddenVar("wp_target", "genome");

printf("%s", "<TD COLWIDTH=2><CENTER>\n");
printf("Forward primer:<BR>");
cgiMakeTextVar("wp_f", fPrimer, 22);
printf("%s", "</CENTER></TD>\n");

printf("%s", "<TD><CENTER COLWIDTH=2>\n");
printf(" Reverse primer:<BR>");
cgiMakeTextVar("wp_r", rPrimer, 22);
printf("%s", "</CENTER></TD>\n");

printf("%s", "<TD><CENTER>\n");
printf("&nbsp;<BR>");
cgiMakeButton("Submit", "Submit");
printf("%s", "</CENTER></TD>\n");

printf("</TR></TABLE><BR>");

printf("<TABLE BORDER=0 WIDTH=\"96%%\" COLS=4><TR>\n");
printf("%s", "<TD><CENTER>\n");
printf("Max product size: ");
cgiMakeIntVar("wp_size", maxSize, 5);
printf("%s", "</CENTER></TD>\n");

printf("%s", "<TD><CENTER>\n");
printf(" Min perfect match: ");
cgiMakeIntVar("wp_perfect", minPerfect, 2);
printf("%s", "</CENTER></TD>\n");

jsOnEventById("click", "Submit", "if ($('#wp_r').val()==='' || $('#wp_f').val()==='') "\
        "{ alert('Please specify at least a forward and reverse primer. Both input boxes need to be filled out.'); event.preventDefault(); }");

printf("%s", "<TD><CENTER>\n");
printf(" Min good match: ");
cgiMakeIntVar("wp_good", minGood, 2);
printf("%s", "</CENTER></TD>\n");

printf("%s", "<TD><CENTER>\n");
printf(" Flip reverse primer: ");
cgiMakeCheckBox("wp_flipReverse", flipReverse);
printf("%s", "</CENTER></TD>\n");

printf("%s", "<TD><CENTER>\n");
printf(" Append to existing PCR result: ");
cgiMakeCheckBox("wp_append", appendToResults);
printf("%s", "</CENTER></TD>\n");

printf("</TR></TABLE><BR>");

printf("</FORM>\n");

/* Put up a second form who's sole purpose is to preserve state
 * when the user flips the genome button. */
printf("<FORM ACTION=\"../cgi-bin/hgPcr\" METHOD=\"GET\" NAME=\"orgForm\">"
       "<input type=\"hidden\" name=\"wp_target\" value=\"\">\n"
       "<input type=\"hidden\" name=\"db\" value=\"\">\n"
       "<input type=\"hidden\" name=\"org\" value=\"\">\n"
       "<input type=\"hidden\" name=\"wp_f\" value=\"\">\n"
       "<input type=\"hidden\" name=\"wp_r\" value=\"\">\n"
       "<input type=\"hidden\" name=\"wp_size\" value=\"\">\n"
       "<input type=\"hidden\" name=\"wp_perfect\" value=\"\">\n"
       "<input type=\"hidden\" name=\"wp_good\" value=\"\">\n"
       "<input type=\"hidden\" name=\"wp_showPage\" value=\"true\">\n");
cartSaveSession(cart);
printf("</FORM>\n");
webNewSection("About In-Silico PCR");
doHelp();
printf("%s", "<P><H3>Author</H3>\n");
printf("%s", "<P>In-Silico PCR was written by "
"<A HREF=\"mailto:kent@soe.ucsc.edu\">Jim Kent</A>.\n"
"Interactive use on this web server is free to all.\n"
"Sources and executables to run batch jobs on your own server are available free\n"
"for academic, personal, and non-profit purposes.  Non-exclusive commercial\n"
"licenses are also available.  Contact Jim for details.</P>\n");
}

void writePrimers(struct gfPcrOutput *gpo, char *fileName)
/* Write primer sequences to file.  Look at only the first gpo because there
 * is only one set of primers in the input form. */
{
if (gpo == NULL)
    return;
FILE *f = mustOpen(fileName, "a");
fprintf(f, "%s\t%s\n", gpo->fPrimer, gpo->rPrimer);
carefulClose(&f);
}

void writePcrResultTrack(struct gfPcrOutput *gpoList, char *db, char *target, boolean appendToResults)
/* Write trash files and store their name in a cart variable. */
{
char *cartVar = pcrResultCartVar(db);
struct tempName bedTn, primerTn;
char buf[2048];
char *pslFile, *txtFile, *cartTarget, *cartResult;
if ( (cartResult = cartOptionalString(cart, cartVar)) != NULL && appendToResults)
    {
    char *pcrFiles[3];
    chopByWhite(cloneString(cartResult), pcrFiles, 3);
    pslFile = pcrFiles[0];
    txtFile = pcrFiles[1];
    cartTarget = pcrFiles[2];
    // if the old result is from a saved session, we can't append to it
    // because we want the session to not change. Copy the old results
    // into a new file and append these results to it
    char *sessionDataDir = cfgOption("sessionDataDir");
    if (sessionDataDir && startsWith(sessionDataDir, pslFile))
        {
        trashDirFile(&bedTn, "hgPcr", "hgPcr", ".psl");
        trashDirFile(&primerTn, "hgPcr", "hgPcr", ".txt");
        // copy the old to the new
        copyFile(pslFile, bedTn.forCgi);
        copyFile(txtFile, primerTn.forCgi);
        gfPcrOutputWriteAll(gpoList, "psl", NULL, bedTn.forCgi);
        writePrimers(gpoList, primerTn.forCgi);
        if (isNotEmpty(target))
            safef(buf, sizeof(buf), "%s %s %s", bedTn.forCgi, primerTn.forCgi, target);
        else
            safef(buf, sizeof(buf), "%s %s", bedTn.forCgi, primerTn.forCgi);
        cartSetString(cart, cartVar, buf);
        }
    else
        {
        gfPcrOutputWriteAll(gpoList, "psl", NULL, pslFile);
        writePrimers(gpoList, txtFile);
        if (isNotEmpty(target) && isEmpty(cartTarget))
            {
            /* User is adding a targetDb search */
            safef(buf, sizeof(buf), "%s %s %s", pslFile, txtFile, target);
            cartSetString(cart, cartVar, buf);
            }
        }
    }
else
    {
    trashDirFile(&bedTn, "hgPcr", "hgPcr", ".psl");
    trashDirFile(&primerTn, "hgPcr", "hgPcr", ".txt");
    gfPcrOutputWriteAll(gpoList, "psl", NULL, bedTn.forCgi);
    writePrimers(gpoList, primerTn.forCgi);
    if (isNotEmpty(target))
        safef(buf, sizeof(buf), "%s %s %s", bedTn.forCgi, primerTn.forCgi, target);
    else
        safef(buf, sizeof(buf), "%s %s", bedTn.forCgi, primerTn.forCgi);
    cartSetString(cart, cartVar, buf);
    }
}

static void printHelpLinks(struct gfPcrOutput *gpoList) {
    /* print links to our docs for special chromosome names */
    // if you modify this, also modify hgBlat.c:showAliPlaces, which implements a similar feature, for hgBlat
    boolean isAlt = FALSE;
    boolean isFix = FALSE;
    boolean isRandom = FALSE;
    boolean isChrUn = FALSE;

    if (gpoList != NULL)
        {
        struct gfPcrOutput *gpo;
        for (gpo = gpoList;  gpo != NULL;  gpo = gpo->next)
            {
            char *seq = gpo->seqName;
            if (endsWith(seq, "_fix"))
                isFix = TRUE;
            else if (endsWith(seq, "_alt"))
                isAlt = TRUE;
            else if (endsWith(seq, "_random"))
                isRandom = TRUE;
            else if (startsWith(seq, "chrUn"))
                isChrUn = TRUE;
            }
        }

    if (isFix || isRandom || isAlt || isChrUn)
        webNewSection("Notes on the results above");

    if (isFix)
        printf("<A target=_blank HREF=\"../FAQ/FAQdownloads#downloadFix\">What is chrom_fix?</A><BR>");
    if (isAlt)
        printf("<A target=_blank HREF=\"../FAQ/FAQdownloads#downloadAlt\">What is chrom_alt?</A><BR>");
    if (isRandom)
        printf("<A target=_blank HREF=\"../FAQ/FAQdownloads#download10\">What is chrom_random?</A><BR>");
    if (isChrUn)
        printf("<A target=_blank HREF=\"../FAQ/FAQdownloads#download11\">What is a chrUn sequence?</A><BR>");
}

void doQuery(struct pcrServer *server, struct gfPcrInput *gpi,
	     int maxSize, int minPerfect, int minGood, boolean appendToResults)
/* Send a query to a genomic assembly PCR server and print the results. */
{
struct gfConnection *conn = gfConnect(server->host, server->port,
                                      trackHubDatabaseToGenome(server->db), server->genomeDataDir);
struct gfPcrOutput *gpoList =
    gfPcrViaNet(conn, server->seqDir, gpi,
		maxSize, minPerfect, minGood);

// translate native names to chromAuthority names
struct gfPcrOutput *gpo;
for(gpo = gpoList ; gpo; gpo = gpo->next)
    {
    char *displayChromName = cloneString(chromAliasGetDisplayChrom(server->db, cart, gpo->seqName));
    gpo->seqName = displayChromName;
    }

if (gpoList != NULL)
    {
    char urlFormat[2048];
    safef(urlFormat, sizeof(urlFormat), "%s?%s&db=%s&position=%%s:%%d-%%d"
	  "&hgPcrResult=pack", 
	  hgTracksName(), cartSidUrlString(cart), server->db);
    printf("<TT><PRE>");
    gfPcrOutputWriteAll(gpoList, "fa", urlFormat, "stdout");
    printf("</PRE></TT>");
    
    printHelpLinks(gpoList);
    writePcrResultTrack(gpoList, server->db, NULL, appendToResults);
    }
else
    {
    printf("<p>No matches to %s %s in %s %s.</p>"
            "<p>To find RT-PCR primers that straddle intron splice sites, go back and change the <b>Target</b> option to a gene transcript set.</p>",
            gpi->fPrimer, gpi->rPrimer, 
	   server->genome, server->description);
    }
gfDisconnect(&conn);
}

void doTargetQuery(struct targetPcrServer *server, struct gfPcrInput *gpi,
		   int maxSize, int minPerfect, int minGood, boolean appendToResults)
/* Send a query to a non-genomic target PCR server and print the results. */
{
struct gfConnection *conn = gfConnect(server->host, server->port, NULL, NULL);
struct gfPcrOutput *gpoList;
char seqDir[PATH_LEN];
splitPath(server->targetDb->seqFile, seqDir, NULL, NULL);
if (endsWith("/", seqDir))
    seqDir[strlen(seqDir) - 1] = '\0';
gpoList = gfPcrViaNet(conn, seqDir, gpi, maxSize, minPerfect, minGood);

if (gpoList != NULL)
    {
    struct gfPcrOutput *gpo;
    char urlFormat[2048];
    printf("The sequences and coordinates shown below are from %s, "
	   "not from the genome assembly.  The links lead to the "
	   "Genome Browser at the position of the entire target "
	   "sequence.<BR>\n",
	   server->targetDb->description);
    printf("<TT><PRE>");
    for (gpo = gpoList;  gpo != NULL;  gpo = gpo->next)
	{
	/* Not used as a format here; we modify the name used for position: */
	safef(urlFormat, sizeof(urlFormat), "%s?%s&db=%s&position=%s"
	      "&hgPcrResult=pack", 
	      hgTracksName(), cartSidUrlString(cart), server->targetDb->db,
	      pcrResultItemAccession(gpo->seqName));
	if (gpo->strand == '-')
	    printf("<EM>Warning: this amplification is on the reverse-"
		   "complement of %s</EM>.\n", gpo->seqName);
	gfPcrOutputWriteOne(gpo, "fa", urlFormat, stdout);
	printf("\n");
	}

    printf("</PRE></TT>");
    writePcrResultTrack(gpoList, server->targetDb->db, server->targetDb->name, appendToResults);
    }
else
    {
    printf("No matches to %s %s in %s", gpi->fPrimer, gpi->rPrimer, 
	   server->targetDb->description);
    }
gfDisconnect(&conn);
}

boolean doPcr(struct pcrServer *server, struct targetPcrServer *targetServer,
	char *fPrimer, char *rPrimer, 
	int maxSize, int minPerfect, int minGood, boolean flipReverse, boolean appendToResults)
/* Do the PCR, and show results. */
{
struct errCatch *errCatch = errCatchNew();
boolean ok = FALSE;

if (issueBotWarning)
    {
    char *ip = getenv("REMOTE_ADDR");
    botDelayMessage(ip, botDelayMillis);
    }

if (flipReverse)
    reverseComplement(rPrimer, strlen(rPrimer));
if (errCatchStart(errCatch))
    {
    struct gfPcrInput *gpi;

    AllocVar(gpi);
    gpi->fPrimer = fPrimer;
    gpi->rPrimer = rPrimer;
    if (server != NULL)
	doQuery(server, gpi, maxSize, minPerfect, minGood, appendToResults);
    if (targetServer != NULL)
	doTargetQuery(targetServer, gpi, maxSize, minPerfect, minGood, appendToResults);
    ok = TRUE;
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    warn("%s", errCatch->message->string);
errCatchFree(&errCatch); 
if (flipReverse)
    reverseComplement(rPrimer, strlen(rPrimer));
webNewSection("Primer Melting Temperatures");
printf("<TT>");
printf("<B>Forward:</B> %4.1f C %s<BR>\n", oligoTm(fPrimer, 50.0, 50.0), fPrimer);
printf("<B>Reverse:</B> %4.1f C %s<BR>\n", oligoTm(rPrimer, 50.0, 50.0), rPrimer);
printf("</TT>");
printf("The temperature calculations are done assuming 50 mM salt and 50 nM annealing "
       "oligo concentration.  The code to calculate the melting temp comes from "
       "<A HREF=\"https://primer3.org/webinterface.html\" target=_blank>"
       "Primer3</A>, the formula by Rychlik W, Spencer WJ and Rhoads RE NAR 1990, which can "
       "be activated in Primer3 with PRIMER_TM_FORMULA=0</A>.");
webNewSection("Help");
printf("<a href='../FAQ/FAQblat.html#blat1c'>What is chr_alt & chr_fix?</a><BR>");
printf("<a href='../FAQ/FAQblat.html#blat5'>Replicating in-Silico PCR results on local machine</a>");
return ok;
}

void dispatch()
/* Look at input variables and figure out which page to show. */
{
char *db, *organism;
int maxSize = 4000;
int minPerfect = 15;
int minGood = 15;
char *fPrimer = cartUsualString(cart, "wp_f", "");
char *rPrimer = cartUsualString(cart, "wp_r", "");
boolean flipReverse = cartUsualBoolean(cart, "wp_flipReverse", FALSE);
boolean appendToResults = cartUsualBoolean(cart, "wp_append", TRUE);
struct pcrServer *serverList = getServerList();

getDbAndGenome(cart, &db, &organism, oldVars);
chromAliasSetup(db);

/* Get variables. */
maxSize = cartUsualInt(cart, "wp_size", maxSize);
minPerfect = cartUsualInt(cart, "wp_perfect", minPerfect);
minGood = cartUsualInt(cart, "wp_good", minGood);
if (minPerfect < 15)
     minPerfect = 15;
if (minGood < minPerfect)
     minGood = minPerfect;

/* Decide based on transient variables what page to put up. 
 * By default put up get primer page. */
if (isNotEmpty(fPrimer) && isNotEmpty(rPrimer) &&
	!cartVarExists(cart, "wp_showPage"))
    {
    struct pcrServer *server = NULL;
    struct targetPcrServer *targetServer = NULL;
    char *target = cartUsualString(cart, "wp_target", "genome");
    if (isEmpty(target) || sameString(target, "genome"))
	server = findServer(db, serverList);
    else
	{
	targetServer = getTargetServerList(db, target);
	if (targetServer == NULL)
	    errAbort("Can't find targetPcr server for db=%s, target=%s",
		     db, target);
	}

    fPrimer = gfPcrMakePrimer(fPrimer);
    rPrimer = gfPcrMakePrimer(rPrimer);
    if (doPcr(server, targetServer, fPrimer, rPrimer,
	      maxSize, minPerfect, minGood, flipReverse, appendToResults))
         return;
    }
doGetPrimers(db, organism, serverList,
	fPrimer, rPrimer, maxSize, minPerfect, minGood, flipReverse, appendToResults);
}

void doMiddle(struct cart *theCart)
/* Write header and body of html page. */
{
cart = theCart;
dnaUtilOpen();
cartWebStart(cart, NULL, "UCSC In-Silico PCR");
dispatch();
cartWebEnd();
}


char *excludeVars[] = {"Submit", "submit", "wp_f", "wp_r", "wp_showPage", NULL};

int main(int argc, char *argv[])
/* Process command line. */
{
long enteredMainTime = clock1000();
/* 0, 0, == use default 10 second for warning, 20 second for immediate exit */
issueBotWarning = earlyBotCheck(enteredMainTime, "hgPcr", delayFraction, 0, 0, "html");
oldVars = hashNew(10);
cgiSpoof(&argc, argv);
cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldVars);
cgiExitTime("hgPcr", enteredMainTime);
return 0;
}

