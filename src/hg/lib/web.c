#include "common.h"
#include "dnautil.h"
#include "errabort.h"
#include "htmshell.h"
#include "web.h"
#include "hdb.h"
#include "cheapcgi.h"
#include "dbDb.h"
#include "axtInfo.h"

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

void webStartText()
/* output the head for a text page */
{
/*printf("Content-Type: text/plain\n\n");*/

webHeadAlreadyOutputed = TRUE;
webInTextMode = TRUE;
pushWarnHandler(textVaWarn);
pushAbortHandler(softAbort);
}

void webStartWrapperGateway(struct cart *theCart, char *format, va_list args, boolean withHttpHeader, boolean withLogo, boolean skipSectionHeader)
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

puts(
    "<HTML>" "\n"
    "<HEAD>" "\n"
    "	<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html;CHARSET=iso-8859-1\">" "\n"
    "	<TITLE>"
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
    snprintf(uiState, sizeof(uiState), "?%s=%s&%s=%u", 
	     dbCgiName, cartUsualString(theCart, dbCgiName, hGetDb()),
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
       "	<table BORDER=0 CELLSPACING=0 CELLPADDING=0 bgcolor=\"2636D1\" height=\"24\">" "\n	"
       " 	<TD VALIGN=\"middle\"><font color=\"#89A1DE\">&nbsp;" "\n" 
       );

printf("&nbsp;<A HREF=\"/index.html%s\" class=\"topbar\">" "\n", uiState);
puts("           Home</A> &nbsp; - &nbsp;");
printf("       <A HREF=\"/cgi-bin/hgGateway%s\" class=\"topbar\">\n",
       uiState);
puts("           Genome Browser</A> &nbsp; - &nbsp;");
printf("       <A HREF=\"/cgi-bin/hgBlat?command=start&%s\" class=\"topbar\">",
       uiState+1);
puts("           Blat Search</A> &nbsp; - &nbsp;" "\n"
     "       <A HREF=\"/FAQ.html\" class=\"topbar\">" "\n"
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
        "<TR><TD COLSPAN=3 CELLPADDING=10>	" "\n"
        "  	<!--outer table is for border purposes-->" "\n"
        "  	<TABLE WIDTH=\"100%\" BGCOLOR=\"#888888\" BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"1\"><TR><TD>	" "\n"
        "    <TABLE BGCOLOR=\"fffee8\" WIDTH=\"100%\"  BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"0\"><TR><TD>	" "\n"
        "	<TABLE BGCOLOR=\"D9E4F8\" BACKGROUND=\"/images/hr.gif\" WIDTH=100%><TR><TD>" "\n"
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

pushWarnHandler(webVaWarn);
pushAbortHandler(softAbort);

/* set the flag */
webHeadAlreadyOutputed = TRUE;
}

void webStartWrapper(struct cart *theCart, char *format, va_list args, boolean withHttpHeader, boolean withLogo)
    /* allows backward compatibility with old webStartWrapper that doesn't contain the "skipHeader" arg */
	/* output a CGI and HTML header with the given title in printf format */
{
webStartWrapperGateway(theCart, format, args, withHttpHeader, withLogo, FALSE);
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
    "	<TABLE BGCOLOR=\"D9E4F8\" BACKGROUND=\"/images/hr.gif\" WIDTH=100%><TR><TD>" "\n"
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
	popWarnHandler();
	popAbortHandler();
	}
}

void webVaWarn(char *format, va_list args)
/* Warning handler that closes out page and stuff in
 * the fancy form. */
{
htmlVaWarn(format, args);
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

void printOrgListHtml(char *db, char *onChangeText)
/*
Prints to stdout the HTML to render a dropdown list containing a list of the possible
organisms to choose from.

param curOrganism - The organism to choose as selected. 
If NULL, no default selection.

param onChangeText - Optional (can be NULL) text to pass in any onChange javascript.
 */
{
char *orgList[128];
int numOrganisms = 0;
struct dbDb *dbList = hGetIndexedDatabases();
struct dbDb *cur = NULL;
struct hash *hash = hashNew(7); // 2^^7 entries = 128
char *selOrganism = hOrganism(db);
char *values [128];

for (cur = dbList; cur != NULL; cur = cur->next)
    {
    /* Only add mouse or human to menu */
    if (!hashFindVal(hash, cur->organism) && 
        (strstrNoCase(cur->organism, "mouse") || strstrNoCase(cur->organism, "human")))
        {
        hashAdd(hash, cur->organism, cur);
        orgList[numOrganisms] = cur->organism;
        values[numOrganisms] = cur->organism;
        numOrganisms++;
        }
    }

cgiMakeDropListFull(orgCgiName, orgList, values, numOrganisms, selOrganism, onChangeText);
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
char *organism = hOrganism(db);
char *assembly = NULL;

for (cur = dbList; cur != NULL; cur = cur->next)
    {
    /* If we are looking at a zoo database then show the zoo database list */
    if ((strstrNoCase(db, "zoo") || strstrNoCase(organism, "zoo")) &&
        strstrNoCase(cur->name, "zoo"))
        {
        assemblyList[numAssemblies] = cur->description;
        values[numAssemblies] = cur->name;
        numAssemblies++;
        }
    else if (strstrNoCase(organism, cur->organism)
             && !strstrNoCase(cur->name, "zoo")
             && !strstrNoCase(db, "zoo")
             && (cur->active || strstrNoCase(cur->name, db)))
        {
        assemblyList[numAssemblies] = cur->description;
        values[numAssemblies] = cur->name;
        numAssemblies++;
        }

    /* Save a pointer to the current assembly */
    if (strstrNoCase(db, cur->name))
       {
       assembly = cur->description;
       }
    }

    cgiMakeDropListFull(dbCgi, assemblyList, values, numAssemblies, assembly, javascript);
}

void printSomeAssemblyListHtml(char *db, struct dbDb *dbList)
{
printSomeAssemblyListHtmlParm(db, dbList, dbCgiName, NULL);
}

void printAssemblyListHtml(char *db)
{
/* Find all the assemblies that pertain to the selected genome 
Prints to stdout the HTML to render a dropdown list containing a list of the possible
assemblies to choose from.

param curDb - The assembly (the database name) to choose as selected. 
If NULL, no default selection.
 */
struct dbDb *dbList = hGetIndexedDatabases();
printSomeAssemblyListHtml(db, dbList);
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
printSomeAssemblyListHtml(db, dbList);
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

void printAlignmentListHtml(char *db, char *alCgiName)
{
/* Find all the alignments (from axtInfo) that pertain to the selected
 * genome.  Prints to stdout the HTML to render a dropdown list
 * containing a list of the possible alignments to choose from.
 */
char *alignmentList[128];
char *values[128];
int numAlignments = 0;
struct axtInfo *alignList = hGetAxtAlignments(db);
struct axtInfo *cur = NULL;
char *organism = hOrganism(db);
char *alignment = NULL;

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
    if (strstrNoCase(db, cur->species))
       {
       alignment = cur->alignment;
       }
    }
cgiMakeDropListFull(alCgiName, alignmentList, values, numAlignments, alignment, NULL);
}

char *getDbForOrganism(char *organism, struct cart *cart)
{
/*
  Function to find the default database for the given organism.
It looks in the cart first and then, if that database's organism matches the 
passed-in organism, returns it. If the organism does not match, it returns the default
database that does match that organism.

param organism - The organism for which to find a database
param cart - The cart to use to first search for a suitable database name
return - The database matching this organism type
*/
char *retDb = cartUsualString(cart, dbCgiName, hGetDb());
char *queryOrganism = hOrganism(retDb);

if (!strstrNoCase(organism, queryOrganism))
    {
    retDb = hDefaultDbForOrganism(organism);
    }
//uglyf("\n<BR>GETDB = %s", retDb);
return retDb;
}

void getDbAndOrganism(struct cart *cart, char **retDb, char **retOrganism)
/*
  The order of preference here is as follows:
If we got a request that explicitly names the db, that takes
highest priority, and we synch the organism to that db.
If we get a cgi request for a specific organism then we use that
organism to choose the DB.

In the cart only, we use the same order of preference.
If someone requests an organism we try to give them the same db as
was in their cart, unless the organism doesn't match.
*/
{
*retDb = cgiOptionalString(dbCgiName);
//uglyf("\n<BR>CGI DB = %s", *retDb);
*retOrganism = cgiOptionalString(orgCgiName);
//uglyf("\n<BR>CGI ORG = %s", *retOrganism);

if (*retDb)
    {
    *retOrganism = hOrganism(*retDb);
//    uglyf("\n<BR>HORGANISM = %s", *retOrganism);
    }
else if (*retOrganism)
    {
    *retDb = getDbForOrganism(*retOrganism, cart);
//    uglyf("\n<BR>GETDB = %s", *retDb);
    }
else
    {
    *retDb = cartUsualString(cart, dbCgiName, hGetDb());
    *retOrganism = hOrganism(*retDb);
//    uglyf("\n<BR>RET DB = %s", *retDb);
//    uglyf("\n<BR>RET ORG = %s", *retOrganism);
    }
}
