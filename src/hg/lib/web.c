#include "common.h"
#include "dnautil.h"
#include "errabort.h"
#include "htmshell.h"
#include "web.h"
#include "hdb.h"
#include "cheapcgi.h"
#include "dbDb.h"
#include "axtInfo.h"

static char const rcsid[] = "$Id: web.c,v 1.39 2003/06/22 03:06:26 kent Exp $";

/* flag that tell if the CGI header has already been outputed */
boolean webHeadAlreadyOutputed = FALSE;
/* flag that tell if text CGI header hsa been outputed */
boolean webInTextMode = FALSE;
static char *dbCgiName = "db";
static char *orgCgiName = "org";

void textVaWarn(char *format, va_list args)
{
vprintf(format, args);
puts("\n");
}

void softAbort()
{
exit(0);
}

void webPushErrHandlers()
/* Push warn and abort handler for errAbort(). */
{
if (webInTextMode)
    pushWarnHandler(textVaWarn);
else
    pushWarnHandler(webVaWarn);
pushAbortHandler(softAbort);
}

void webPopErrHandlers()
/* Pop warn and abort handler for errAbort(). */
{
popWarnHandler();
popAbortHandler();
}

void webStartText()
/* output the head for a text page */
{
/*printf("Content-Type: text/plain\n\n");*/

webHeadAlreadyOutputed = TRUE;
webInTextMode = TRUE;
webPushErrHandlers();
}

void webStartWrapperGatewayHeader(struct cart *theCart, char *headerText,
	char *format, va_list args, boolean withHttpHeader,
	boolean withLogo, boolean skipSectionHeader)
/* output a CGI and HTML header with the given title in printf format */
{
char uiState[256];

/* don't output two headers */
if(webHeadAlreadyOutputed)
    return;

/* Preamble. */
dnaUtilOpen();

if (withHttpHeader)
    puts("Content-type:text/html\n");

puts("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2//EN\">");
puts(
    "<HTML>" "\n"
    "<HEAD>" "\n"
    );
printf("\t%s\n", headerText);
puts("\t<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html;CHARSET=iso-8859-1\">" "\n"
     "\t<TITLE>"
     );

vprintf(format, args);

puts(
    "	</TITLE>" "\n"
    "	<LINK REL=\"STYLESHEET\" HREF=\"/style/HGStyle.css\">" "\n"
    "</HEAD>" "\n"
    "<BODY BGCOLOR=\"FFF9D2\" LINK=\"0000CC\" VLINK=\"#330066\" ALINK=\"#6600FF\">" "\n"
    "<A NAME=\"TOP\"></A>" "\n"
    "" "\n"
    "<TABLE BORDER=0 CELLPADDING=0 CELLSPACING=0 WIDTH=\"100%\">" "\n");

if (withLogo)
    puts(
    "<TR><TH COLSPAN=1 ALIGN=\"left\"><IMG SRC=\"/images/title.jpg\"></TH></TR>" "\n"
    "" "\n"
    );

if (NULL != theCart)
    {
    snprintf(uiState, sizeof(uiState), "?%s=%u", 
	     cartSessionVarName(), cartSessionId(theCart));
    }
else
    {
    uiState[0] = 0;
    uiState[1] = 0;
    }

puts(
       "<!--HOTLINKS BAR---------------------------------------------------------->" "\n"
       "<TR><TD COLSPAN=3 HEIGHT=40 >" "\n"
       "<table bgcolor=\"000000\" cellpadding=\"1\" cellspacing=\"1\" width=\"100%%\" height=\"27\">" "\n"
       "<tr bgcolor=\"2636D1\"><td valign=\"middle\">" "\n"
       "	<table BORDER=0 CELLSPACING=0 CELLPADDING=0 bgcolor=\"2636D1\" height=\"24\"><TR>" "\n	"
       " 	<TD VALIGN=\"middle\"><font color=\"#89A1DE\">&nbsp;" "\n" 
       );

printf("&nbsp;<A HREF=\"/index.html%s\" class=\"topbar\">" "\n", uiState);
puts("           Home</A> &nbsp; - &nbsp;");
printf("       <A HREF=\"/cgi-bin/hgGateway%s\" class=\"topbar\">\n",
       uiState);
puts("           Genome Browser</A> &nbsp; - &nbsp;");
printf("       <A HREF=\"/cgi-bin/hgBlat?command=start&%s\" class=\"topbar\">",
       uiState+1);
puts("           Blat Search</A> &nbsp; - &nbsp;");
printf("       <A HREF=\"/cgi-bin/hgText%s\" class=\"topbar\">\n", uiState);
puts("           Table Browser</A> &nbsp; - &nbsp;");
puts("       <A HREF=\"/FAQ.html\" class=\"topbar\">" "\n"
     "           FAQ</A> &nbsp; - &nbsp;" "\n" 
     "       <A HREF=\"/goldenPath/help/hgTracksHelp.html\" class=\"topbar\">" "\n"
     "           User Guide</A> &nbsp;</font></TD>" "\n"
     "       </TR></TABLE>" "\n"
     "</TD></TR></TABLE>" "\n"
     "</TD></TR>	" "\n"	
     "" "\n"
     );

if(!skipSectionHeader)
/* this HTML must be in calling code if skipSectionHeader is TRUE */
    {
    puts(
	 "<!--Content Tables------------------------------------------------------->" "\n"
	 "<TR><TD COLSPAN=3>	" "\n"
	 "  	<!--outer table is for border purposes-->" "\n"
	 "  	<TABLE WIDTH=\"100%\" BGCOLOR=\"#888888\" BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"1\"><TR><TD>	" "\n"
	 "    <TABLE BGCOLOR=\"fffee8\" WIDTH=\"100%\"  BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"0\"><TR><TD>	" "\n"
	 "	<TABLE BGCOLOR=\"D9E4F8\" BACKGROUND=\"/images/hr.gif\" WIDTH=\"100%\"><TR><TD>" "\n"
	 "		<FONT SIZE=\"4\"><b>&nbsp;"
	 );
    vprintf(format, args);

    puts(
	 "</b></FONT></TD></TR></TABLE>" "\n"
	 "	<TABLE BGCOLOR=\"fffee8\" WIDTH=\"100%\" CELLPADDING=0><TR><TH HEIGHT=10></TH></TR>" "\n"
	 "	<TR><TD WIDTH=10>&nbsp;</TD><TD>" "\n"
	 "	" "\n"
	 );
    };

webPushErrHandlers();
/* set the flag */
webHeadAlreadyOutputed = TRUE;
}

void webStartWrapperGateway(struct cart *theCart, char *format, va_list args, boolean withHttpHeader, boolean withLogo, boolean skipSectionHeader)
/* output a CGI and HTML header with the given title in printf format */
{
webStartWrapperGatewayHeader(theCart, "", format, args, withHttpHeader,
			     withLogo, skipSectionHeader);
}

void webStartWrapper(struct cart *theCart, char *format, va_list args, boolean withHttpHeader, boolean withLogo)
    /* allows backward compatibility with old webStartWrapper that doesn't contain the "skipHeader" arg */
	/* output a CGI and HTML header with the given title in printf format */
{
webStartWrapperGatewayHeader(theCart, "", format, args, withHttpHeader,
			     withLogo, FALSE);
}	

void webStart(struct cart *theCart, char *format, ...)
/* Print out pretty wrapper around things when not
 * from cart. */
{
va_list args;
va_start(args, format);
webStartWrapper(theCart, format, args, TRUE, TRUE);
va_end(args);
}

void webStartHeader(struct cart *theCart, char *headerText, char *format, ...)
/* Print out pretty wrapper around things when not from cart. 
 * Include headerText in the html header. */
{
va_list args;
va_start(args, format);
webStartWrapperGatewayHeader(theCart, headerText, format, args, TRUE, TRUE,
			     FALSE);
va_end(args);
}

void webNewSection(char* format, ...)
/* create a new section on the web page */
{
va_list args;
va_start(args, format);

puts(
    "" "\n"
    "	</TD><TD WIDTH=15></TD></TR></TABLE>" "\n"
    "	<br></TD></TR></TABLE>" "\n"
    "	</TD></TR></TABLE>" "\n"
    "	" "\n"
    "<!--START SECOND SECTION ------------------------------------------------------->" "\n"
    "<BR>" "\n"
    "" "\n"
    "  	<!--outer table is for border purposes-->" "\n"
    "  	<TABLE WIDTH=\"100%\" BGCOLOR=\"#888888\" BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"1\"><TR><TD>	" "\n"
    "    <TABLE BGCOLOR=\"fffee8\" WIDTH=\"100%\"  BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"0\"><TR><TD>	" "\n"
    "	<TABLE BGCOLOR=\"D9E4F8\" BACKGROUND=\"/images/hr.gif\" WIDTH=\"100%\"><TR><TD>" "\n"
    "		<FONT SIZE=\"4\"><b>&nbsp; "
);

vprintf(format, args);

puts(
    "	</b></FONT></TD></TR></TABLE>" "\n"
    "	<TABLE BGCOLOR=\"fffee8\" WIDTH=\"100%\" CELLPADDING=0><TR><TH HEIGHT=10></TH></TR>" "\n"
    "	<TR><TD WIDTH=10>&nbsp;</TD><TD>" "\n"
    "" "\n"
);

va_end(args);
}


void webEnd()
/* output the footer of the HTML page */
{
if(!webInTextMode)
	{
	puts(
	    "" "\n"
	    "	</TD><TD WIDTH=15></TD></TR></TABLE>" "\n"
	    "	<br></TD></TR></TABLE>" "\n"
	    "	</TD></TR></TABLE>" "\n"
	    "<!-- END SECOND SECTION ---------------------------------------------------------->" "\n"
	    "" "\n"
	    "</TD></TR></TABLE>" "\n"
	    "</BODY></HTML>" "\n"
	);
	webPopErrHandlers();
	}
}

void webVaWarn(char *format, va_list args)
/* Warning handler that closes out page and stuff in
 * the fancy form. */
{
if (! webHeadAlreadyOutputed)
    webStart(NULL, "Error");
htmlVaWarn(format, args);
printf("\n<!-- HGERROR -->\n");
printf("\n\n");
webEnd();
}


void webAbort(char* title, char* format, ...)
/* an abort function that outputs a error page */
{
va_list args;
va_start(args, format);

/* output the header */
if(!webHeadAlreadyOutputed)
	webStart(NULL, title);

/* in text mode, have a different error */
if(webInTextMode)
	printf("\n\n\n          %s\n\n", title);

vprintf(format, args);
printf("<!-- HGERROR -->\n");
printf("\n\n");

webEnd();

va_end(args);
exit(0);
}

static boolean haveDatabase(char *db)
/* check if the database server has the specified database */
{
/* list of databases that actually exists (not really worth hashing) */
static struct hash* validDatabases = NULL;
if (validDatabases == NULL)
    {
    struct sqlConnection *sc = hAllocConn();
    struct slName* allDatabases = sqlGetAllDatabase(sc);
    struct slName* dbName = allDatabases;
    validDatabases = hashNew(8);
    for (; dbName != NULL; dbName = dbName->next)
        hashAdd(validDatabases, dbName->name, NULL);
    hFreeConn(&sc);
    slFreeList(&allDatabases);
    }
return (hashLookup(validDatabases, db) != NULL);
}

void printGenomeListHtml(char *db, char *onChangeText)
/* Prints to stdout the HTML to render a dropdown list 
 * containing a list of the possible genomes to choose from.
 * param curGenome - The Genome to choose as selected. 
 * If NULL, no default selection.  param onChangeText - Optional 
 * (can be NULL) text to pass in any onChange javascript. */
{
char *orgList[128];
char *values[128];
int numGenomes = 0;
struct dbDb *dbList = hGetIndexedDatabases();
struct dbDb *cur = NULL;
struct hash *hash = hashNew(7); // 2^^7 entries = 128
char *selGenome = hGenome(db);

for (cur = dbList; cur != NULL; cur = cur->next)
    {
    if (!hashFindVal(hash, cur->genome) && haveDatabase(cur->name))
        {
        hashAdd(hash, cur->genome, cur);
        orgList[numGenomes] = cur->genome;
        values[numGenomes] = cur->genome;
        numGenomes++;
	if (numGenomes >= ArraySize(orgList))
	    internalErr();
        }
    }
cgiMakeDropListFull(orgCgiName, orgList, values, numGenomes, selGenome, onChangeText);
hashFree(&hash);
}

void printSomeAssemblyListHtmlParm(char *db, struct dbDb *dbList, char *dbCgi, char *javascript)
{
/* Find all the assemblies that pertain to the selected genome and that have
BLAT servers set up.
Prints to stdout the HTML to render a dropdown list containing a list of the possible
assemblies to choose from.

param curDb - The assembly (the database name) to choose as selected. 
If NULL, no default selection.
 */
char *assemblyList[128];
char *values[128];
int numAssemblies = 0;
struct dbDb *cur = NULL;
struct hash *hash = hashNew(7); // 2^^7 entries = 128
char *genome = hGenome(db);
char *selAssembly = NULL;

for (cur = dbList; cur != NULL; cur = cur->next)
    {
    if (strstrNoCase(genome, cur->genome)
        && (cur->active || strstrNoCase(cur->name, db))
        && haveDatabase(cur->name))
        {
        assemblyList[numAssemblies] = cur->description;
        values[numAssemblies] = cur->name;
        numAssemblies++;
	if (numAssemblies >= ArraySize(assemblyList))
	    internalErr();
        }

    /* Save a pointer to the current assembly */
    if (strstrNoCase(db, cur->name))
       {
       selAssembly = cur->description;
       }
    }

cgiMakeDropListFull(dbCgi, assemblyList, values, numAssemblies, selAssembly, javascript);
}

void printSomeAssemblyListHtml(char *db, struct dbDb *dbList, char *javascript)
{
printSomeAssemblyListHtmlParm(db, dbList, dbCgiName, javascript);
}

void printAssemblyListHtml(char *db, char *javascript)
/* Find all the assemblies that pertain to the selected genome 
 * Prints to stdout the HTML to render a dropdown list containing 
 * a list of the possible assemblies to choose from.
 * Param db - The assembly (the database name) to choose as selected. 
 * If NULL, no default selection.  */
{
struct dbDb *dbList = hGetIndexedDatabases();
printSomeAssemblyListHtml(db, dbList, javascript);
}

void printAssemblyListHtmlExtra(char *db, char *javascript)
{
/* Find all the assemblies that pertain to the selected genome 
Prints to stdout the HTML to render a dropdown list containing a list of the possible
assemblies to choose from.

param curDb - The assembly (the database name) to choose as selected. 
If NULL, no default selection.
 */
struct dbDb *dbList = hGetIndexedDatabases();
printSomeAssemblyListHtmlParm(db, dbList, dbCgiName, javascript);
}

void printBlatAssemblyListHtml(char *db)
{
/* Find all the assemblies that pertain to the selected genome 
Prints to stdout the HTML to render a dropdown list containing a list of the possible
assemblies to choose from.

param curDb - The assembly (the database name) to choose as selected. 
If NULL, no default selection.
 */
struct dbDb *dbList = hGetBlatIndexedDatabases();
printSomeAssemblyListHtml(db, dbList, NULL);
}

void printOrgAssemblyListAxtInfo(char *dbCgi, char *javascript)
/* Find all the organisms/assemblies that are referenced in axtInfo, 
 * and print the dropdown list. */
{
struct dbDb *dbList = hGetAxtInfoDbs();
char *assemblyList[128];
char *values[128];
int numAssemblies = 0;
struct dbDb *cur = NULL;
char *db = hGetDb();
char *organism = hOrganism(db);
char *assembly = cgiOptionalString(dbCgi);

for (cur = dbList; ((cur != NULL) && (numAssemblies < 128)); cur = cur->next)
    {
    assemblyList[numAssemblies] = cur->description;
    values[numAssemblies] = cur->name;
    numAssemblies++;
    }

// Have to use the "menu" name, not the value, to mark selected:
if (assembly != NULL)
    assembly = hFreezeFromDb(assembly);

cgiMakeDropListFull(dbCgi, assemblyList, values, numAssemblies, assembly,
		    javascript);
}

void printAlignmentListHtml(char *db, char *alCgiName, char *selected)
{
/* Find all the alignments (from axtInfo) that pertain to the selected
 * genome.  Prints to stdout the HTML to render a dropdown list
 * containing a list of the possible alignments to choose from.
 */
char *alignmentList[128];
char *values[128];
int numAlignments = 0;
struct axtInfo *alignList = NULL;
struct axtInfo *cur = NULL;
char *organism = hOrganism(db);

alignList = hGetAxtAlignments(db);

for (cur = alignList; ((cur != NULL) && (numAlignments < 128)); cur = cur->next)
    {
    /* If we are looking at a zoo database then show the zoo database list */
    if ((strstrNoCase(db, "zoo") || strstrNoCase(organism, "zoo")) &&
        strstrNoCase(cur->species, "zoo"))
        {
        alignmentList[numAlignments] = cur->alignment;
        values[numAlignments] = cur->alignment;
        numAlignments++;
        }
    else if (
             !strstrNoCase(cur->species, "zoo") &&
             (strstrNoCase(cur->species, db)))
        {
        alignmentList[numAlignments] = cur->alignment;
        values[numAlignments] = cur->alignment;
        numAlignments++;
        }


    /* Save a pointer to the current alignment */
    if (selected == NULL)
        if (strstrNoCase(db, cur->species))
           {
           selected = cur->alignment;
           }
    }
cgiMakeDropListFull(alCgiName, alignmentList, values, numAlignments, selected, NULL);
}

char *getDbForGenome(char *genome, struct cart *cart)
{
/*
  Function to find the default database for the given Genome.
It looks in the cart first and then, if that database's Genome matches the 
passed-in Genome, returns it. If the Genome does not match, it returns the default
database that does match that Genome.

param Genome - The Genome for which to find a database
param cart - The cart to use to first search for a suitable database name
return - The database matching this Genome type
*/
char *retDb = cartUsualString(cart, dbCgiName, hGetDb());

if (!hDbExists(retDb))
    {
    retDb = hDefaultDb();
    }

/* If genomes don't match, then get the default db for that genome */
if (!containsStringNoCase(genome, hGenome(retDb)))
    {
    retDb = hDefaultDbForGenome(genome);
    }

return retDb;
}

void getDbAndGenome(struct cart *cart, char **retDb, char **retGenome)
/* Get current database and genome from cart.  The database will have
 * precedence over the genome if they are in conflict in the cart.
 * If the database doesn't exist or is bad (moved to archives perhaps?)
 * then get default database for organism (for human if no organism
 * specified). */
{
char *db = cartOptionalString(cart, dbCgiName);
char *org = cartOptionalString(cart, orgCgiName);

/* Get rid of out of date databases. */
if (db && !hDbExists(db))
    db = NULL;
if (db == NULL)
    db = hDefaultDbForGenome(org);  /* This handles NULL organism ok. */
org = hGenome(db);
*retDb = db;
*retGenome = org;
}
