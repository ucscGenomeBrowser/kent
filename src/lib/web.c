#include "common.h"
#include "dnautil.h"
#include "web.h"

/* flag that tell if the CGI header has already been outputed */
boolean webHeadAlreadyOutputed = FALSE;
/* flag that tell if text CGI header hsa been outputed */
boolean webInTextMode = FALSE;


void webStartText()
/* output the head for a text page */
{
printf("Content-Type: text/plain\n\n");

webHeadAlreadyOutputed = TRUE;
webInTextMode = TRUE;
}


void webStart(char* format,...)
/* output a CGI and HTML header with the given title in printf format */
{
va_list args;
va_start(args, format);

/* don't output two headers */
if(webHeadAlreadyOutputed)
    return;

/* Preamble. */
dnaUtilOpen();

puts("Content-type:text/html\n");

puts(
	"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2//EN\">" "\n"
	"<HTML>" "\n"
	"<HEAD>" "\n"
	"	<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html;CHARSET=iso-8859-1\">" "\n"
	"	<TITLE>"
);

vprintf(format, args);

puts(
	"</TITLE>" "\n"
	"	<LINK REL=\"STYLESHEET\" HREF=\"/style/HGStyle.css\">" "\n"
	"</HEAD>" "\n"
	"<BODY BGCOLOR=\"FFF9D2\" LINK=\"0000CC\" VLINK=\"#330066\" ALINK=\"#6600FF\">" "\n"
	"<A NAME=\"TOP\"></A>" "\n"
	"" "\n"
	"<TABLE BORDER=0 WIDTH=\"100%\">" "\n"
	"<TR><TH COLSPAN=2 ALIGN=\"left\"><IMG SRC=\"/images/title.jpg\"></TH></TR>" "\n"
	"" "\n"
	"<!--HOTLINKS BAR----------------------------------------------------------->" "\n"
	"<TR><TD COLSPAN=2 HEIGHT=40>" "\n"
	"   <CENTER>" "\n"
	"	<TABLE BGCOLOR=\"2636D1\" WIDTH=100% CELLSPACING=0 CELLPADDING=0 BORDER=1 HEIGHT=\"22\"><TR><TD>" "\n"
	"		<TABLE WIDTH=431 CELLSPACING=4 CELLPADDING=0 BORDER=0><TR>" "\n"
	"		<TD WIDTH=1></TD>" "\n"
	"		<TD WIDTH=75>" "\n"
	"			<A HREF=\"/index.html\" onMouseOver=\"HOME.src='/images/hl_home.jpg'\" onMouseOut=\"HOME.src='/images/h_home.jpg'\">" "\n"
	"			<IMG SRC=\"/images/h_home.jpg\" ALIGN=\"absmiddle\" BORDER=\"0\" NAME=\"HOME\" ALT=\"Home\" ></A></TD>" "\n"
	"		<TD WIDTH=155>" "\n"
	"			<A HREF=\"/goldenPath/hgTracks.html\" onMouseOver=\"BROWSER.src='/images/hl_browser.jpg'\" onMouseOut=\"BROWSER.src='/images/h_browser.jpg'\">" "\n"
	"			<IMG SRC=\"/images/h_browser.jpg\" ALIGN=\"absmiddle\" BORDER=\"0\" NAME=\"BROWSER\" ALT=\"&nbsp;&nbsp; Genome Browser\"></A></TD>		" "\n"
	"		<TD WIDTH=100>" "\n"
	"			<A HREF=\"/cgi-bin/hgBlat?command=start\" onMouseOver=\"SEARCH.src='/images/hl_blat.jpg'\" onMouseOut=\"SEARCH.src='/images/h_blat.jpg'\">" "\n"
	"			<IMG SRC=\"/images/h_blat.jpg\" ALIGN=\"absmiddle\" BORDER=\"0\" NAME=\"SEARCH\" ALT=\"&nbsp;&nbsp; BLAT Search\"></A></TD>	" "\n"
	"		<TD WIDTH=100>" "\n"
	"			<A HREF=\"/FAQ.html\" onMouseOver=\"GUIDE.src='/images/hl_faq.jpg'\" onMouseOut=\"GUIDE.src='/images/h_faq.jpg'\">" "\n"
	"			<IMG SRC=\"/images/h_faq.jpg\" ALIGN=\"absmiddle\" BORDER=\"0\" NAME=\"GUIDE\" ALT=\"&nbsp;&nbsp; FAQ\"></A></TD>" "\n"
	"		</TR></TABLE>" "\n"
	"	 </TR></TABLE>" "\n"
	"	 </CENTER>" "\n"
	"    </TD>" "\n"
	"</TR>" "\n"
	"" "\n"
	"<!--Content Tables------------------------------------------------------->" "\n"
	"<TR><TD CELLPADDING=10>	" "\n"
	"	<TABLE BGCOLOR=\"fffee8\" WIDTH=\"100%\" BORDERCOLOR=\"888888\" BORDER=1><TR><TD>" "\n"
	"	<TABLE BGCOLOR=\"D9E4F8\" BACKGROUND=\"/images/hr.gif\" WIDTH=100%><TR><TD>" "\n"
	"		<FONT SIZE=\"4\"><b>&nbsp;  "
);

vprintf(format, args);

puts(
	"</b></FONT>" "\n"
	"	</TD></TR></TABLE>" "\n"
	"	<TABLE BGCOLOR=\"fffee8\" WIDTH=\"100%\" CELLPADDING=0><TH HEIGHT=10></TH>" "\n"
	"	<TR><TD WIDTH=10>&nbsp;</TD><TD>" "\n"
	"" "\n"
);

/* set the flag */
webHeadAlreadyOutputed = TRUE;

va_end(args);
}


void webNewSection(char* format, ...)
/* create a new section on the web page */
{
va_list args;
va_start(args, format);

puts(
	"" "\n"
	"" "\n"
	"	</TD><TD WIDTH=15></TD></TR></TABLE>" "\n"
	"	<br></TD></TR></TABLE>" "\n"
	"	" "\n"
	"<!--START SECOND SECTION ------------------------------------------------------->" "\n"
	"<BR>" "\n"
	"" "\n"
	"	<TABLE BGCOLOR=\"fffee8\" WIDTH=\"100%\"  BORDERCOLOR=\"888888\" BORDER=1><TR><TD>" "\n"
	"	<TABLE BGCOLOR=\"ffffff\" BACKGROUND=\"/images/hr.gif\" WIDTH=100%><TR><TD>" "\n"
	"		<FONT SIZE=\"4\"><b>&nbsp;  "
);

vprintf(format, args);

puts("</b></FONT>" "\n"
	"	</TD></TR></TABLE>" "\n"
	"	<TABLE BGCOLOR=\"fffee8\" WIDTH=\"100%\" CELLPADDING=0><TH HEIGHT=10></TH>" "\n"
	"	<TR><TD WIDTH=10>&nbsp;</TD><TD>" "\n"
	"" "\n"
);

va_end(args);
}


void webEnd()
/* output the footer of the HTML page */
{
if(!webInTextMode)
	puts(
		"" "\n"
		"	</TD><TD WIDTH=15></TD></TR></TABLE>" "\n"
		"	<br></TD></TR></TABLE>" "\n"
		"	" "\n"
		"</TD></TR></TABLE>" "\n"
		"</BODY></HTML>" "\n"
	);
}


void webAbort(char* title, char* format, ...)
/* an abort function that outputs a error page */
{
va_list args;
va_start(args, format);

/* output the header */
if(!webHeadAlreadyOutputed)
	webStart(title);

/* in text mode, have a different error */
if(webInTextMode)
	printf("\n\n\n          %s\n\n", title);

vprintf(format, args);

webEnd();

va_end(args);
exit(1);
}
