#include "common.h"
#include "dnautil.h"
#include "errabort.h"
#include "htmshell.h"
#include "web.h"
#include "hdb.h"

/* flag that tell if the CGI header has already been outputed */
boolean webHeadAlreadyOutputed = FALSE;
/* flag that tell if text CGI header hsa been outputed */
boolean webInTextMode = FALSE;


void webStartText()
/* output the head for a text page */
{
/*printf("Content-Type: text/plain\n\n");*/

webHeadAlreadyOutputed = TRUE;
webInTextMode = TRUE;
}

void webStartWrapperGateway(struct cart *theCart, char *format, va_list args, boolean withHttpHeader, boolean withLogo, boolean skipSectionHeader)
/* output a CGI and HTML header with the given title in printf format */
{
char *db = NULL;

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
    db = cartUsualString(theCart, "db", hGetDb());
    }

puts(
       "<!--HOTLINKS BAR---------------------------------------------------------->" "\n"
       "<TR><TD COLSPAN=3 HEIGHT=40 >" "\n"
       "<table bgcolor=\"000000\" cellpadding=\"1\" cellspacing=\"1\" width=\"100%%\" height=\"27\">" "\n"
       "<tr bgcolor=\"2636D1\"><td valign=\"middle\">" "\n"
       "	<table BORDER=0 CELLSPACING=0 CELLPADDING=0 bgcolor=\"2636D1\" height=\"24\">" "\n	"
       " 	<TD VALIGN=\"middle\"><font color=\"#89A1DE\">&nbsp;" "\n" 
       );

if (NULL != db)
    {
    printf("&nbsp;<A HREF=\"/index.html?db=%s\" class=\"topbar\">" "\n", db);
    }
else
    {
    puts("&nbsp;<A HREF=\"/index.html\" class=\"topbar\">" "\n");
    }
puts(
     "           Home</A> &nbsp; - &nbsp;" "\n"
     "       <A HREF=\"/cgi-bin/hgGateway\" class=\"topbar\">" "\n"
     "           Genome Browser</A> &nbsp; - &nbsp;" "\n"
     "       <A HREF=\"/cgi-bin/hgBlat?command=start\" class=\"topbar\">" "\n"
     "           Blat Search</A> &nbsp; - &nbsp;" "\n" 
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

webEnd();

va_end(args);
exit(1);
}
