#include "common.h"
#include "hCommon.h"
#include "obscure.h"
#include "dnautil.h"
#include "errabort.h"
#include "htmshell.h"
#include "web.h"
#include "hdb.h"
#include "hui.h"
#include "cheapcgi.h"
#include "dbDb.h"
#include "axtInfo.h"
#include "hgColors.h"
#include "wikiLink.h"

static char const rcsid[] = "$Id: web.c,v 1.143 2008/05/01 23:19:10 angie Exp $";

/* flag that tell if the CGI header has already been outputed */
boolean webHeadAlreadyOutputed = FALSE;
/* flag that tell if text CGI header hsa been outputed */
boolean webInTextMode = FALSE;
static char *dbCgiName = "db";
static char *orgCgiName = "org";
static char *cladeCgiName = "clade";
static char *extraStyle = NULL;

/* global: a cart for use in error handlers. */
static struct cart *errCart = NULL;

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

void webPushErrHandlersCart(struct cart *cart)
/* Push warn and abort handler for errAbort(); save cart for use in handlers. */
{
errCart = cart;
webPushErrHandlers();
}

void webPopErrHandlers()
/* Pop warn and abort handler for errAbort(). */
{
popWarnHandler();
popAbortHandler();
}

void webSetStyle(char *style)
/* set a style to add to the header */
{
extraStyle = style;
} 

void webStartText()
/* output the head for a text page */
{
/*printf("Content-Type: text/plain\n\n");*/

webHeadAlreadyOutputed = TRUE;
webInTextMode = TRUE;
webPushErrHandlers();
}

static void webStartWrapperDetailedInternal(struct cart *theCart,
	char *headerText, char *textOutBuf,
	boolean withHttpHeader, boolean withLogo, boolean skipSectionHeader,
	boolean withHtmlHeader)
/* output a CGI and HTML header with the given title in printf format */
{
char uiState[256];
char *scriptName = cgiScriptName();
char *db = NULL;
boolean isEncode = FALSE;
boolean isGsid   = hIsGsidServer();

if (scriptName == NULL)
    scriptName = cloneString("");
/* don't output two headers */
if(webHeadAlreadyOutputed)
    return;

if (sameString(cgiUsualString("action",""),"encodeReleaseLog") ||
    rStringIn("EncodeDataVersions", scriptName))
        isEncode = TRUE;

/* Preamble. */
dnaUtilOpen();

if (withHttpHeader)
    puts("Content-type:text/html\n");

if (withHtmlHeader)
    {
    char *newString, *ptr1, *ptr2;

    puts("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2//EN\">");
    puts(
	"<HTML>" "\n"
	"<HEAD>" "\n"
	);
    printf("\t%s\n", headerText);
    puts("\t<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html;CHARSET=iso-8859-1\">" "\n"
	 "\t<META http-equiv=\"Content-Script-Type\" content=\"text/javascript\">" "\n"
         "\t<META HTTP-EQUIV=\"Pragma\" CONTENT=\"no-cache\">" "\n"
         "\t<META HTTP-EQUIV=\"Expires\" CONTENT=\"-1\">" "\n"
	 "\t<TITLE>"
	 );
    /* we need to take any HTML formatting out of the titlebar string */
    newString = cloneString(textOutBuf);

    for(ptr1=newString, ptr2=textOutBuf; *ptr2 ; ptr2++)
	{
	if (*ptr2 == '<')
	    {
	    for(; *ptr2 && (*ptr2 != '>'); ptr2++)
		;
	    }
	else
	    *ptr1++ = *ptr2;
	}
    *ptr1 = 0;
    htmlTextOut(newString);
    puts(
	"	</TITLE>" "\n"
	"	<LINK REL=\"STYLESHEET\" HREF=\"../style/HGStyle.css\" TYPE=\"text/css\">" "\n");
    if (extraStyle != NULL)
        puts(extraStyle);
    puts(
	"</HEAD>" "\n"
	"<BODY BGCOLOR=\"#"HG_COL_OUTSIDE"\" LINK=\"0000CC\" VLINK=\"#330066\" ALINK=\"#6600FF\">" 
	);
    }
puts(
    "<A NAME=\"TOP\"></A>" "\n"
    "" "\n"
    "<TABLE BORDER=0 CELLPADDING=0 CELLSPACING=0 WIDTH=\"100%\">" "\n");

if (withLogo)
    {
    puts("<TR><TH COLSPAN=1 ALIGN=\"left\">");
    if (isEncode)
	{
	puts("<A HREF=\"http://www.genome.gov/10005107\" TARGET=\"_BLANK\">"
	     "<IMG SRC=\"../images/encodelogo.gif\" height=50 ALT=\"ENCODE Project at NHGRI\">"
	     "</A>");
	puts("<IMG SRC=\"../images/encode.jpg\" ALT=\"ENCODE Project at UCSC\">");
	}
    else
	{
	puts("<IMG SRC=\"../images/title.jpg\">");
	}
    puts("</TH></TR>" "\n"
    	 "" "\n" );
    }
if (theCart)
    {
    char *theGenome = NULL;
    char *genomeEnc = NULL;

    getDbAndGenome(theCart, &db, &theGenome, NULL);
    genomeEnc = cgiEncode(theGenome);

    snprintf(uiState, sizeof(uiState), "?%s=%s&%s=%s&%s=%u", 
	     orgCgiName, genomeEnc,
	     dbCgiName, db,
	     cartSessionVarName(), cartSessionId(theCart));
    }
else
    {
    uiState[0] = 0;
    uiState[1] = 0;
    }

/* Put up the hot links bar. */
if (isGsid) 
    {
    printf("<TABLE WIDTH=\"100%%\" BGCOLOR=\"#000000\" BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"1\"><TR><TD>\n");
    printf("<TABLE WIDTH=\"100%%\" BGCOLOR=\"#2636D1\" BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"2\"><TR>\n");

    /* Home */
    printf("<TD ALIGN=CENTER><A HREF=\"../index.html\" class=\"topbar\"><FONT COLOR=\"#FFFFFF\">Home</FONT></A></TD>");

    /* Blat */
    printf("<TD ALIGN=CENTER><A HREF=\"../cgi-bin/hgBlat?command=start\" class=\"topbar\"><FONT COLOR=\"#FFFFFF\">Blat</FONT></A></TD>");

    /* Subject  View */
    printf("<TD ALIGN=CENTER><A HREF=\"../cgi-bin/gsidSubj\" class=\"topbar\">%s</A></TD>", "<FONT COLOR=\"#FFFFFF\">Subject View</FONT>");

    /* Sequence View */
    printf("<TD ALIGN=CENTER><A HREF=\"../cgi-bin/hgTracks%s\" class=\"topbar\"><FONT COLOR=\"#FFFFFF\">Sequence View</FONT></A></TD>", 
	   uiState);

    /* Table View */
    printf("<TD ALIGN=CENTER><A HREF=\"../cgi-bin/gsidTable\" class=\"topbar\">%s</A></TD>", "<FONT COLOR=\"#FFFFFF\">Table View</FONT>");
    
    /* Help */

    if (endsWith(scriptName, "hgBlat"))
    	{
	printf("<TD ALIGN=CENTER><A HREF=\"/goldenPath/help/gsidTutorial.html#BLAT\" TARGET=_blank class=\"topbar\">%s</A></TD>", "<FONT COLOR=\"#FFFFFF\">Help</FONT>");
    	}
    else
	{    
    	printf("<TD ALIGN=CENTER><A HREF=\"/goldenPath/help/sequenceViewHelp.html\" TARGET=_blank class=\"topbar\">%s</A></TD>", "<FONT COLOR=\"#FFFFFF\">Help</FONT>");
	}
    printf("</TR></TABLE>");
    printf("</TD></TR></TABLE>\n");
    }
else
{

puts(
       "<!-- +++++++++++++++++++++ HOTLINKS BAR +++++++++++++++++++ -->" "\n"
       "<TR><TD COLSPAN=3 HEIGHT=40 >" "\n"
       "<table bgcolor=\"#000000\" cellpadding=\"1\" cellspacing=\"1\" width=\"100%%\" height=\"27\">" "\n"
       "<tr bgcolor=\"#"HG_COL_HOTLINKS"\"><td valign=\"middle\">" "\n"
       "	<table BORDER=0 CELLSPACING=0 CELLPADDING=0 bgcolor=\"#"HG_COL_HOTLINKS"\" height=\"24\"><TR>" "\n	"
       " 	<TD VALIGN=\"middle\"><font color=\"#89A1DE\">&nbsp;" "\n" 
       );

if (isEncode)
    {
    printf("&nbsp;<A HREF=\"../encode/\" class=\"topbar\">" "\n");
    puts("           Home</A>");
    }
else
    {
    printf("&nbsp;<A HREF=\"../index.html%s\" class=\"topbar\">" "\n", uiState);
    puts("           Home</A> &nbsp;&nbsp;&nbsp;");
    if (isGsid) 
	{
    	printf("       <A HREF=\"../cgi-bin/gsidSubj%s\" class=\"topbar\">\n",
	       uiState);
	puts("           Subject View</A> &nbsp;&nbsp;&nbsp;");
	}
    if (!isGsid)
	{
	printf("       <A HREF=\"../cgi-bin/hgGateway%s\" class=\"topbar\">\n",
	       uiState);
    	puts("           Genomes</A> &nbsp;&nbsp;&nbsp;");
    	}
    if (endsWith(scriptName, "hgTracks") || endsWith(scriptName, "hgGene") ||
	endsWith(scriptName, "hgTables") || endsWith(scriptName, "hgTrackUi") ||
	endsWith(scriptName, "hgSession") || endsWith(scriptName, "hgCustom") ||
	endsWith(scriptName, "hgc"))
	{
	printf("       <A HREF=\"../cgi-bin/hgTracks%s\" class=\"topbar\">\n",
	       uiState);
	puts("           Genome Browser</A> &nbsp;&nbsp;&nbsp;");
	}
    if (!endsWith(scriptName, "hgBlat"))
	{
    	printf("       <A HREF=\"../cgi-bin/hgBlat?command=start%s%s\" class=\"topbar\">", 
		theCart ? "&" : "", uiState+1 );
    	puts("           Blat</A> &nbsp;&nbsp;&nbsp;");
	}
}
    {
    char *table = (theCart == NULL ? NULL :
		   (endsWith(scriptName, "hgGene") ?
		    cartOptionalString(theCart, "hgg_type") :
		    cartOptionalString(theCart, "g")));
    if (table && theCart &&
	(endsWith(scriptName, "hgc") || endsWith(scriptName, "hgTrackUi") ||
	 endsWith(scriptName, "hgGene")))
	{
	struct trackDb *tdb = hTrackDbForTrack(table);
	if (tdb != NULL)
	    printf("       <A HREF=\"../cgi-bin/hgTables%s&hgta_doMainPage=1&"
		   "hgta_group=%s&hgta_track=%s&hgta_table=%s\" "
		   "class=\"topbar\">\n",
		uiState, tdb->grp, table, table);
	else
	    printf("       <A HREF=\"../cgi-bin/hgTables%s&hgta_doMainPage=1\" "
		   "class=\"topbar\">\n",
		uiState);
	trackDbFree(&tdb);
	}
    else
	printf("       <A HREF=\"../cgi-bin/hgTables%s%shgta_doMainPage=1\" "
	       "class=\"topbar\">\n",
	       uiState, theCart ? "&" : "?" );
    }
    if (!isGsid) puts("           Tables</A> &nbsp;&nbsp;&nbsp;");
    if (!endsWith(scriptName, "hgNear")) 
    /*  possible to make this conditional: if (db != NULL && hgNearOk(db))	*/
        if (db != NULL && hgNearOk(db))
	{
	if (isGsid)
	    {
	    printf("       <A HREF=\"../cgi-bin/gsidTable%s\" class=\"topbar\">\n",
	           uiState);
	    puts("           Table View</A> &nbsp;&nbsp;&nbsp;");
	    }
	else
	    {
	    printf("       <A HREF=\"../cgi-bin/hgNear%s\" class=\"topbar\">\n",
	           uiState);
	    puts("           Gene Sorter</A> &nbsp;&nbsp;&nbsp;");
	    }
	}
    if ((!endsWith(scriptName, "hgPcr")) && (db == NULL || hgPcrOk(db)))
	{
	printf("       <A HREF=\"../cgi-bin/hgPcr%s\" class=\"topbar\">\n",
	       uiState);
	puts("           PCR</A> &nbsp;&nbsp;&nbsp;");
	}
    if (endsWith(scriptName, "hgGenome"))
	{
	printf("       <A HREF=\"../cgi-bin/hgGenome%s&hgGenome_doPsOutput=on\" class=\"topbar\">\n",
	       uiState);
	puts("           PDF/PS</A> &nbsp;&nbsp;&nbsp;");
	}
    if (endsWith(scriptName, "hgHeatmap"))
	{
	printf("       <A HREF=\"../cgi-bin/hgHeatmap%s&hgHeatmap_doPsOutput=on\" class=\"topbar\">\n",
	       uiState);
	puts("           PDF/PS</A> &nbsp;&nbsp;&nbsp;");
	}
    if (wikiLinkEnabled() && !endsWith(scriptName, "hgSession"))
	{
	printf("<A HREF=\"../cgi-bin/hgSession%s%shgS_doMainPage=1\" "
	       "class=\"topbar\">Session</A>",
	       uiState, theCart ? "&" : "?" );
	puts("&nbsp;&nbsp;&nbsp;");
	}
    if (!isGsid) puts("       <A HREF=\"../FAQ/\" class=\"topbar\">" "\n"
	 "           FAQ</A> &nbsp;&nbsp;&nbsp;" "\n" 
	 );
    if (!isGsid)
	{
    	if (endsWith(scriptName, "hgBlat"))
	    puts("       <A HREF=\"../goldenPath/help/hgTracksHelp.html#BLATAlign\"");
    	else if (endsWith(scriptName, "hgText"))
	    puts("       <A HREF=\"../goldenPath/help/hgTextHelp.html\"");
    	else if (endsWith(scriptName, "hgNear"))
	    puts("       <A HREF=\"../goldenPath/help/hgNearHelp.html\"");
    	else if (endsWith(scriptName, "hgTables"))
	    puts("       <A HREF=\"../goldenPath/help/hgTablesHelp.html\"");
    	else if (endsWith(scriptName, "hgGenome"))
	    puts("       <A HREF=\"../goldenPath/help/hgGenomeHelp.html\"");
    	else if (endsWith(scriptName, "hgSession"))
	    puts("       <A HREF=\"../goldenPath/help/hgSessionHelp.html\"");
    	else if (endsWith(scriptName, "hgVisiGene"))
	    puts("       <A HREF=\"../goldenPath/help/hgTracksHelp.html#VisiGeneHelp\"");
    	else 
	    puts("       <A HREF=\"../goldenPath/help/hgTracksHelp.html\"");
	puts("       class=\"topbar\">");
    	puts("           Help</A> ");
    	}
    }
puts("&nbsp;</font></TD>" "\n"
     "       </TR></TABLE>" "\n"
     "</TD></TR></TABLE>" "\n"
     "</TD></TR>	" "\n"	
     "" "\n"
     );

if(!skipSectionHeader)
/* this HTML must be in calling code if skipSectionHeader is TRUE */
    {
    puts(
         "<!-- +++++++++++++++++++++ CONTENT TABLES +++++++++++++++++++ -->" "\n"
	 "<TR><TD COLSPAN=3>	" "\n"
	 "  	<!--outer table is for border purposes-->" "\n"
	 "  	<TABLE WIDTH=\"100%\" BGCOLOR=\"#"HG_COL_BORDER"\" BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"1\"><TR><TD>	" "\n"
	 "    <TABLE BGCOLOR=\"#"HG_COL_INSIDE"\" WIDTH=\"100%\"  BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"0\"><TR><TD>	" "\n"
	 "	<TABLE BGCOLOR=\"#"HG_COL_HEADER"\" BACKGROUND=\"../images/hr.gif\" WIDTH=\"100%\"><TR><TD>" "\n"
	 "		<FONT SIZE=\"4\"><b>&nbsp;"
	 );
    htmlTextOut(textOutBuf);

    puts(
	 "</b></FONT></TD></TR></TABLE>" "\n"
	 "	<TABLE BGCOLOR=\"#"HG_COL_INSIDE"\" WIDTH=\"100%\" CELLPADDING=0><TR><TH HEIGHT=10></TH></TR>" "\n"
	 "	<TR><TD WIDTH=10>&nbsp;</TD><TD>" "\n"
	 "	" "\n"
	 );

    };
webPushErrHandlers();
/* set the flag */
webHeadAlreadyOutputed = TRUE;
}	/*	static void webStartWrapperDetailedInternal()	*/

void webStartWrapperDetailedArgs(struct cart *theCart, char *headerText,
	char *format, va_list args, boolean withHttpHeader,
	boolean withLogo, boolean skipSectionHeader, boolean withHtmlHeader)
/* output a CGI and HTML header with the given title in printf format */
{
char textOutBuf[512];
va_list argscp;

va_copy(argscp,args);
vasafef(textOutBuf, sizeof(textOutBuf), format, argscp);
va_end(argscp);

webStartWrapperDetailedInternal(theCart, headerText, textOutBuf,
	withHttpHeader, withLogo, skipSectionHeader, withHtmlHeader);
}

void webStartWrapperDetailedNoArgs(struct cart *theCart, char *headerText,
	char *format, boolean withHttpHeader,
	boolean withLogo, boolean skipSectionHeader, boolean withHtmlHeader)
/* output a CGI and HTML header with the given title in printf format */
{
char textOutBuf[512];

safef(textOutBuf, sizeof(textOutBuf), format);
webStartWrapperDetailedInternal(theCart, headerText, textOutBuf,
	withHttpHeader, withLogo, skipSectionHeader, withHtmlHeader);
}

void webStartWrapperGatewayHeader(struct cart *theCart, char *headerText,
    char *format, va_list args, boolean withHttpHeader,
	boolean withLogo, boolean skipSectionHeader)
{
webStartWrapperDetailedArgs(theCart, headerText, format, args, withHttpHeader,
	withLogo, skipSectionHeader, TRUE);
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

static void webEndSection()
/* Close down a section */
{
puts(
    "" "\n"
    "	</TD><TD WIDTH=15></TD></TR></TABLE>" "\n"
//    "<BR>"
    "	</TD></TR></TABLE>" "\n"
    "	</TD></TR></TABLE>" "\n"
    "	" );
}

void webNewSection(char* format, ...)
/* create a new section on the web page */
{
va_list args;
va_start(args, format);

webEndSection();
puts("<!-- +++++++++++++++++++++ START NEW SECTION +++++++++++++++++++ -->");
puts(
    "<BR>" "\n"
    "" "\n"
    "  	<!--outer table is for border purposes-->" "\n"
    "  	<TABLE WIDTH=\"100%\" BGCOLOR=\"#"HG_COL_BORDER"\" BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"1\"><TR><TD>	" "\n"
    "    <TABLE BGCOLOR=\"#"HG_COL_INSIDE"\" WIDTH=\"100%\"  BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"0\"><TR><TD>	" "\n"
    "	<TABLE BGCOLOR=\"#"HG_COL_HEADER"\" BACKGROUND=\"../images/hr.gif\" WIDTH=\"100%\"><TR><TD>" "\n"
    "		<FONT SIZE=\"4\"><b>&nbsp; "
);

vprintf(format, args);

puts(
    "	</b></FONT></TD></TR></TABLE>" "\n"
    "	<TABLE BGCOLOR=\"#"HG_COL_INSIDE"\" WIDTH=\"100%\" CELLPADDING=0><TR><TH HEIGHT=10></TH></TR>" "\n"
    "	<TR><TD WIDTH=10>&nbsp;</TD><TD>" "\n"
    "" "\n"
);

va_end(args);
}

void webEndSectionTables()
/* Finish with section tables (but don't do /BODY /HTML lik
 * webEnd does. */
{
webEndSection();
puts("</TD></TR></TABLE>\n");
}

void webEnd()
/* output the footer of the HTML page */
{
if(!webInTextMode)
    {
    webEndSectionTables();
    puts( "</BODY></HTML>");
    webPopErrHandlers();
    }
}

void webVaWarn(char *format, va_list args)
/* Warning handler that closes out page and stuff in
 * the fancy form. */
{
if (! webHeadAlreadyOutputed)
    webStart(errCart, "Error");
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
	webStart(errCart, title);

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

void printCladeListHtml(char *genome, char *onChangeText)
/* Make an HTML select input listing the clades. */
{
struct sqlConnection *conn = hConnectCentral();
struct sqlResult *sr = NULL;
char **row = NULL;
char *clades[128];
char *labels[128];
char *defaultClade = hClade(genome);
char *defaultLabel = NULL;
int numClades = 0;

sr = sqlGetResult(conn, "select name, label from clade order by priority");
while ((row = sqlNextRow(sr)) != NULL)
    {
    clades[numClades] = cloneString(row[0]);
    labels[numClades] = cloneString(row[1]);
    if (sameWord(defaultClade, clades[numClades]))
	defaultLabel = clades[numClades];
    numClades++;
    if (numClades >= ArraySize(clades))
	internalErr();
    }

cgiMakeDropListFull(cladeCgiName, labels, clades, numClades, 
		    defaultLabel, onChangeText);
}

static void printSomeGenomeListHtmlNamedMaybeCheck(char *customOrgCgiName,
	 char *db, struct dbDb *dbList, char *onChangeText, boolean doCheck)
/* Prints to stdout the HTML to render a dropdown list 
 * containing a list of the possible genomes to choose from.
 * param db - a database whose genome will be the default genome.
 *                       If NULL, no default selection.  
 * param onChangeText - Optional (can be NULL) text to pass in 
 *                              any onChange javascript. */
{
char *orgList[1024];
int numGenomes = 0;
struct dbDb *cur = NULL;
struct hash *hash = hashNew(10); // 2^^10 entries = 1024
char *selGenome = hGenomeOrArchive(db);
char *values [1024];
char *cgiName;

for (cur = dbList; cur != NULL; cur = cur->next)
    {
    if (!hashFindVal(hash, cur->genome) &&
	(!doCheck || haveDatabase(cur->name)))
        {
        hashAdd(hash, cur->genome, cur);
        orgList[numGenomes] = cur->genome;
        values[numGenomes] = cur->genome;
        numGenomes++;
	if (numGenomes >= ArraySize(orgList))
	    internalErr();
        }
    }

cgiName = (customOrgCgiName != NULL) ? customOrgCgiName : orgCgiName;
cgiMakeDropListFull(cgiName, orgList, values, numGenomes, 
                    selGenome, onChangeText);
hashFree(&hash);
}

void printSomeGenomeListHtmlNamed(char *customOrgCgiName, char *db, struct dbDb *dbList, char *onChangeText)
/* Prints to stdout the HTML to render a dropdown list 
 * containing a list of the possible genomes to choose from.
 * param db - a database whose genome will be the default genome.
 *                       If NULL, no default selection.  
 * param onChangeText - Optional (can be NULL) text to pass in 
 *                              any onChange javascript. */
{
return printSomeGenomeListHtmlNamedMaybeCheck(customOrgCgiName, db, dbList,
					      onChangeText, TRUE);
}

void printLiftOverGenomeList(char *customOrgCgiName, char *db,
			     struct dbDb *dbList, char *onChangeText)
/* Prints to stdout the HTML to render a dropdown list 
 * containing a list of the possible genomes to choose from.
 * Databases in dbList do not have to exist.
 * param db - a database whose genome will be the default genome.
 *                       If NULL, no default selection.  
 * param onChangeText - Optional (can be NULL) text to pass in 
 *                              any onChange javascript. */
{
return printSomeGenomeListHtmlNamedMaybeCheck(customOrgCgiName, db, dbList,
					      onChangeText, FALSE);
}

void printSomeGenomeListHtml(char *db, struct dbDb *dbList, char *onChangeText)
/* Prints the dropdown list using the orgCgiName */
{
printSomeGenomeListHtmlNamed(NULL, db, dbList, onChangeText);
}

void printGenomeListHtml(char *db, char *onChangeText)
/* Prints to stdout the HTML to render a dropdown list 
 * containing a list of the possible genomes to choose from.
 * param db - a database whose genome will be the default genome.
 *                       If NULL, no default selection.  
 * param onChangeText - Optional (can be NULL) text to pass in 
 *                              any onChange javascript. */
{
printSomeGenomeListHtml(db, hGetIndexedDatabases(), onChangeText);
}

void printBlatGenomeListHtml(char *db, char *onChangeText)
/* Prints to stdout the HTML to render a dropdown list 
 * containing a list of the possible genomes to choose from.
 * param db - a database whose genome will be the default genome.
 *                       If NULL, no default selection.  
 * param onChangeText - Optional (can be NULL) text to pass in 
 *                              any onChange javascript. */
{
printSomeGenomeListHtml(db, hGetBlatIndexedDatabases(), onChangeText);
}


void printGenomeListForCladeHtml(char *db, char *onChangeText)
/* Prints to stdout the HTML to render a dropdown list containing 
 * a list of the possible genomes from selOrganism's clade to choose from.  
 * selOrganism is the default for the select.
 */
{
printSomeGenomeListHtml(db, hGetIndexedDatabasesForClade(db), onChangeText);
}

void printAllAssemblyListHtmlParm(char *db, struct dbDb *dbList, 
                            char *dbCgi, bool allowInactive, char *javascript)
/* Prints to stdout the HTML to render a dropdown list containing the list 
 * of assemblies for the current genome to choose from.  By default,
 * this includes only active assemblies with a database (with the
 * exception of the default assembly, which will be included even
 * if it isn't active).
 *  param db - The default assembly (the database name) to choose as selected. 
 *             If NULL, no default selection.
 *  param allowInactive - if set, print all assemblies for this genome,
 *                        even if they're inactive or have no database
 */
{

char *assemblyList[128];
char *values[128];
int numAssemblies = 0;
struct dbDb *cur = NULL;
char *genome = hGenomeOrArchive(db);
char *selAssembly = NULL;

if (genome == NULL)
#ifdef LOWELAB
    genome = "Pyrococcus furiosus";
#else 
    genome = "Human";
#endif
for (cur = dbList; cur != NULL; cur = cur->next)
    {
    /* Only for this genome */
    if (!sameWord(genome, cur->genome))
        continue;

    /* Save a pointer to the current assembly */
    if (sameWord(db, cur->name))
        selAssembly = cur->name;

    if (allowInactive ||
        ((cur->active || sameWord(cur->name, db)) 
                && haveDatabase(cur->name)))
        {
        assemblyList[numAssemblies] = cur->description;
        values[numAssemblies] = cur->name;
        numAssemblies++;
	if (numAssemblies >= ArraySize(assemblyList))
	    internalErr();
        }

    }
cgiMakeDropListFull(dbCgi, assemblyList, values, numAssemblies, 
                                selAssembly, javascript);
}

void printSomeAssemblyListHtmlParm(char *db, struct dbDb *dbList, 
                                        char *dbCgi, char *javascript)
/* Find all the assemblies from the list that are active.
 * Prints to stdout the HTML to render a dropdown list containing the list 
 * of the possible assemblies to choose from.
 * param db - The default assembly (the database name) to choose as selected. 
 *    If NULL, no default selection.  */
{

    printAllAssemblyListHtmlParm(db, dbList, dbCgi, TRUE, javascript);
}
 
void printSomeAssemblyListHtml(char *db, struct dbDb *dbList, char *javascript)
/* Find all assemblies from the list that are active, and print
 * HTML to render dropdown list 
 * param db - default assembly.  If NULL, no default selection */
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
char *assembly = cgiOptionalString(dbCgi);
char orgAssembly[256];

for (cur = dbList; ((cur != NULL) && (numAssemblies < 128)); cur = cur->next)
    {
    safef(orgAssembly, sizeof(orgAssembly), "%s %s",
	  cur->organism, cur->description);
    assemblyList[numAssemblies] = cloneString(orgAssembly);
    values[numAssemblies] = cur->name;
    numAssemblies++;
    }

#ifdef OLD
// Have to use the "menu" name, not the value, to mark selected:
if (assembly != NULL)
    {
    char *selOrg    = hOrganism(assembly);
    char *selFreeze = hFreezeFromDb(assembly);
    safef(orgAssembly, sizeof(orgAssembly), "%s %s", selOrg, selFreeze);
    assembly = cloneString(orgAssembly);
    }
#endif /* OLD */

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
if (differentWord(genome, hGenome(retDb)))
    {
    retDb = hDefaultDbForGenome(genome);
    }

return retDb;
}


void getDbGenomeClade(struct cart *cart, char **retDb, char **retGenome,
		      char **retClade, struct hash *oldVars)
/* Examine CGI and cart variables to determine which db, genome, or clade
 *  has been selected, and then adjust as necessary so that all three are 
 * consistent.  Detect changes and reset db-specific cart variables.
 * Save db, genome and clade in the cart so it will be consistent hereafter.
 * The order of preference here is as follows:
 * If we got a request that explicitly names the db, that takes
 * highest priority, and we synch the organism to that db.
 * If we get a cgi request for a specific organism then we use that
 * organism to choose the DB.  If just clade, go from there.

 * In the cart only, we use the same order of preference.
 * If someone requests an Genome we try to give them the same db as
 * was in their cart, unless the Genome doesn't match.
 */
{
boolean gotClade = hGotClade();
*retDb = cgiOptionalString(dbCgiName);
*retGenome = cgiOptionalString(orgCgiName);
*retClade = cgiOptionalString(cladeCgiName);

/* Was the database passed in as a cgi param?
 * If so, it takes precedence and determines the genome. */
if (*retDb && hDbExists(*retDb))
    {
    *retGenome = hGenome(*retDb);
    }
/* If no db was passed in as a cgi param then was the organism (a.k.a. genome)
 * passed in as a cgi param?
 * If so, the we use the proper database for that genome. */
else if (*retGenome && !sameWord(*retGenome, "0"))
    {
    *retDb = getDbForGenome(*retGenome, cart);
    *retGenome = hGenome(*retDb);
    }
else if (*retClade && gotClade)
    {
    *retGenome = hDefaultGenomeForClade(*retClade);
    *retDb = getDbForGenome(*retGenome, cart);
    }
/* If no cgi params passed in then we need to inspect the session */
else
    {
    *retDb = cartOptionalString(cart, dbCgiName);
    *retGenome = cartOptionalString(cart, orgCgiName);
    *retClade = cartOptionalString(cart, cladeCgiName);
    /* If there was a db found in the session that determines everything. */
    if (*retDb && hDbExists(*retDb))
        {
        *retGenome = hGenome(*retDb);
        }
    else if (*retGenome && !sameWord(*retGenome, "0"))
	{
	*retDb = hDefaultDbForGenome(*retGenome);
	}
    else if (*retClade && gotClade)
	{
        *retGenome = hDefaultGenomeForClade(*retClade);
	*retDb = getDbForGenome(*retGenome, cart);
	}
    /* If no organism in the session then get the default db and organism. */
    else
	{
	*retDb = hDefaultDb();
	*retGenome = hGenome(*retDb);
        }
    }
*retDb = cloneString(*retDb);
*retGenome = cloneString(*retGenome);
*retClade = hClade(*retGenome);

/* Detect change of database and reset db-specific cart variables: */
if (oldVars)
    {
    char *oldDb = hashFindVal(oldVars, "db");
    char *oldOrg = hashFindVal(oldVars, "org");
    char *oldClade = hashFindVal(oldVars, "clade");
    if ((oldDb    && differentWord(oldDb, *retDb)) ||
	(oldOrg   && differentWord(oldOrg, *retGenome)) ||
	(oldClade && differentWord(oldClade, *retClade)))
	{
	/* Change position to default -- unless it was passed in via CGI: */
	if (cgiOptionalString("position") == NULL)
	    cartSetString(cart, "position", hDefaultPos(*retDb));
	/* hgNear search term -- unless it was passed in via CGI: */
	if (cgiOptionalString("near_search") == NULL)
	    cartRemove(cart, "near_search");
	/* hgBlat results (hgUserPsl track): */
	cartRemove(cart, "ss");
	/* hgTables correlate: */
	cartRemove(cart, "hgta_correlateTrack");
	cartRemove(cart, "hgta_correlateTable");
	cartRemove(cart, "hgta_correlateGroup");
	cartRemove(cart, "hgta_correlateOp");
	cartRemove(cart, "hgta_nextCorrelateTrack");
	cartRemove(cart, "hgta_nextCorrelateTable");
	cartRemove(cart, "hgta_nextCorrelateGroup");
	cartRemove(cart, "hgta_nextCorrelateOp");
	cartRemove(cart, "hgta_corrWinSize");
	cartRemove(cart, "hgta_corrMaxLimitCount");
	}
    }

/* Save db, genome (as org) and clade in cart. */
cartSetString(cart, "db", *retDb);
cartSetString(cart, "org", *retGenome);
if (gotClade)
    cartSetString(cart, "clade", *retClade);
}

void getDbAndGenome(struct cart *cart, char **retDb, char **retGenome,
		    struct hash *oldVars)
/* Get just the db and genome. */
{
char *garbage = NULL;
getDbGenomeClade(cart, retDb, retGenome, &garbage, oldVars);
freeMem(garbage);
}

boolean webIncludeFile(char *file)
/* Include an HTML file in a CGI.
 *   The file path may begin with hDocumentRoot(); if it doesn't, it is
 *   assumed to be relative and hDocumentRoot() will be prepended. */
{
char *str = NULL;
size_t len = 0;
char path[PATH_LEN];
char *docRoot = hDocumentRoot();

if (file == NULL)
    {
    printf("<BR>Program Error: Empty file name for include file<BR>");
    return FALSE;
    }
if (startsWith(docRoot, file))
    safecpy(path, sizeof path, file);
else
    safef(path, sizeof path, "%s/%s", docRoot, file);
if (!fileExists(path))
    {
   printf("<BR>Program Error: Missing file %s</BR>", path); 
   return FALSE;
   }
readInGulp(path, &str, &len);
if (len <= 0)
    {
    printf("<BR>Program Error: Unable to read included file: %s</BR>", path);
    return FALSE;
    }
puts(str);
freeMem(str);
return TRUE;
}

boolean webIncludeHelpFile(char *fileRoot, boolean addHorizLine)
/* Given a help file root name (e.g. "hgPcrResult" or "cutters"),
 * print out the contents of the file.  If addHorizLine, print out an
 * <HR> first. */
{
if (addHorizLine)
    htmlHorizontalLine();
return webIncludeFile(hHelpFile(fileRoot));
}

void webPrintLinkTableStart()
/* Print link table start in our colors. */
{
printf("<TABLE><TR><TD BGCOLOR=#888888>\n");
printf("<TABLE CELLSPACING=1 CELLPADDING=3><TR>\n");
}

void webPrintLinkTableEnd()
/* Print link table end in our colors. */
{
printf("</TR></TABLE>\n");
printf("</TD></TR></TABLE>\n");
}

void webPrintLinkOutCellStart()
/* Print link cell that goes out of our site. End with 
 * webPrintLinkTableEnd. */
{
printf("<TD BGCOLOR=\"#"HG_COL_LOCAL_TABLE"\">");
}

void webPrintWideCellStart(int colSpan, char *bgColorRgb)
/* Print link multi-column cell start in our colors. */
{
printf("<TD BGCOLOR=\"#%s\"", bgColorRgb);
if (colSpan > 1)
    printf(" COLSPAN=%d", colSpan);
printf(">");
}

void webPrintLinkCellStart()
/* Print link cell start in our colors. */
{
webPrintWideCellStart(1, HG_COL_TABLE);
}

void webPrintLinkCellRightStart()
/* Print right-justified cell start in our colors. */
{
printf("<TD BGCOLOR=\"#"HG_COL_TABLE"\" ALIGN=\"right\">");
}

void webPrintLinkCellEnd()
/* Print link cell end in our colors. */
{
printf("</TD>");
}

void webPrintLinkCell(char *link)
/* Print link cell in our colors, if links is null, print empty cell */
{
webPrintLinkCellStart();
if (link != NULL)
    puts(link);
webPrintLinkCellEnd();
}

void webPrintIntCell(int val)
/* Print right-justified int cell in our colors. */
{
webPrintLinkCellRightStart();
printf("%d", val);
webPrintLinkCellEnd();
}

void webPrintDoubleCell(double val)
/* Print right-justified cell in our colors with two digits to right of decimal. */
{
webPrintLinkCellRightStart();
printf("%4.2f", val);
webPrintLinkCellEnd();
}

void webPrintWideLabelCell(char *label, int colSpan)
/* Print label cell over multiple columns in our colors. */
{
printf("<TD BGCOLOR=\"#"HG_COL_TABLE_LABEL"\"");
if (colSpan > 1)
    printf(" COLSPAN=%d", colSpan);
printf("><FONT COLOR=\"#FFFFFF\"><B>%s</B></FONT></TD>", label);
}

void webPrintLabelCell(char *label)
/* Print label cell in our colors. */
{
webPrintWideLabelCell(label, 1);
}

void webPrintLinkTableNewRow()
/* start a new row */
{
printf("</TR>\n<TR>");
}

void finishPartialTable(int rowIx, int itemPos, int maxPerRow,
	void (*cellStart)())
/* Fill out partially empty last row. */
{
if (rowIx != 0 && itemPos < maxPerRow)
    {
    int i;
    for (i=itemPos; i<maxPerRow; ++i)
        {
	cellStart();
	webPrintLinkCellEnd();
	}
    }
}

void webFinishPartialLinkOutTable(int rowIx, int itemPos, int maxPerRow)
/* Fill out partially empty last row. */
{
finishPartialTable(rowIx, itemPos, maxPerRow, webPrintLinkOutCellStart);
}

void webFinishPartialLinkTable(int rowIx, int itemPos, int maxPerRow)
/* Fill out partially empty last row. */
{
finishPartialTable(rowIx, itemPos, maxPerRow, webPrintLinkCellStart);
}

