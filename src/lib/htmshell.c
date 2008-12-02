/* htmshell - a shell to wrap around programs that generate
 * html files.  Write the html initial stuff (<head>, <body>, etc.)
 * and the final stuff too.  Also catch errors here so that
 * the html final stuff is written even if the program has
 * to abort. 
 *
 * This also includes a few routines to write commonly used
 * html constructs such as images, horizontal lines. etc. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "obscure.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "errabort.h"
#include "dnautil.h"

static char const rcsid[] = "$Id: htmshell.c,v 1.46 2008/12/02 01:34:56 markd Exp $";

jmp_buf htmlRecover;

static bool NoEscape = FALSE;

void htmlNoEscape()
{
NoEscape = TRUE;
}

void htmlDoEscape()
{
NoEscape = FALSE;
}

void htmlVaParagraph(char *line, va_list args)
/* Print a line in it's own paragraph. */
{
fputs("<P>", stdout);
vfprintf(stdout, line, args);
fputs("</P>\n", stdout);
}

void htmlParagraph(char *line, ...)
{
va_list args;
va_start(args, line);
htmlVaParagraph(line, args);
va_end(args);
}

void htmlVaCenterParagraph(char *line, va_list args)
/* Center a line in it's own paragraph. */
{
fputs("<P ALIGN=\"CENTER\">", stdout);
vfprintf(stdout, line, args);
fputs("</P>\n", stdout);
}

void htmlCenterParagraph(char *line, ...)
{
va_list args;
va_start(args, line);
htmlVaCenterParagraph(line, args);
va_end(args);
}

void htmlHorizontalLine()
/* Print a horizontal line. */
{
printf("<HR ALIGN=\"CENTER\">");
}

void htmHorizontalLine(FILE *f)
/* Print a horizontal line. */
{
fprintf(f, "<HR ALIGN=\"CENTER\">");
}

void htmlNbSpaces(int count)
/* Print a number of non-breaking spaces. */
{
int i;
for (i=0; i<count; ++i)
    printf("&nbsp;");
}

void htmTextOut(FILE *f, char *s)
/* Print out string to file, if necessary replacing > with &gt; and the like */
{
char c;
if (NoEscape)
    {
    fputs(s, f);
    return;
    }

while ((c = *s++) != 0)
    {
    switch (c)
        {
	case '>':
	    fputs("&gt;", f);
	    break;
	case '<':
	    fputs("&lt;", f);
	    break;
	case '&':
	    fputs("&amp;", f);
	    break;
	case '"':
	    fputs("&quot;", f);
	    break;
	default:
	    fputc(c, f);
	    break;
	}
    }
}

void htmlTextOut(char *s)
/* Print out string, if necessary replacing > with &gt; and the like */
{
htmTextOut(stdout, s);
}

char *htmlEncode(char *s)
/* Return a clone of s but if necessary replacing > with &gt; and the like */
{
size_t len = 0;
char c;
char *encStr;
char *p = s;
if (s == NULL)	/*	do not try to encode a NULL pointer */
    return NULL;
/* First pass through s to determine encoded length to allocate: */
/* [as a shortcut, we could simply allocate 6*length of s] */
while ((c = *p++) != 0)
    {
    switch (c)
        {
	case '>':
	case '<':
	    len += 4;
	    break;
	case '&':
	    len += 5;
	    break;
	case '"':
	    len += 6;
	    break;
	default:
	    len++;
	    break;
	}
    }
encStr = needMem(len+1);
/* Second pass through s to encode: */
len = 0;
p = s;
while ((c = *p++) != 0)
    {
    switch (c)
        {
	case '>':
	    strcat(encStr+len, "&gt;");
	    len += 4;
	    break;
	case '<':
	    strcat(encStr+len, "&lt;");
	    len += 4;
	    break;
	case '&':
	    strcat(encStr+len, "&amp;");
	    len += 5;
	    break;
	case '"':
	    strcat(encStr+len, "&quot;");
	    len += 6;
	    break;
	default:
	    encStr[len++] = c;
	    break;
	}
    }
return encStr;
}


char *htmlWarnStartPattern()
/* Return starting pattern for warning message. */
{
return "<!-- HGERROR-START -->\n";
}

char *htmlWarnEndPattern()
/* Return ending pattern for warning message. */
{
return "<!-- HGERROR-END -->\n";
}

void htmlVaWarn(char *format, va_list args)
/* Write an error message. */
{
va_list argscp;
va_copy(argscp, args);

htmlHorizontalLine();
printf("%s", htmlWarnStartPattern());
htmlVaParagraph(format,args);
printf("%s", htmlWarnEndPattern());
htmlHorizontalLine();

/* Log useful CGI info to stderr */
logCgiToStderr();

/* write warning/error message to stderr so they get logged. */
vfprintf(stderr, format, argscp);
va_end(argscp);
fputc('\n', stderr);
fflush(stderr);
}

void htmlAbort()
/* Terminate HTML file. */
{
longjmp(htmlRecover, -1);
}

void htmlMemDeath()
{
errAbort("Out of memory.");
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

static void earlyAbortHandler()
/* Exit close web page during early abort. */
{
printf("</BODY></HTML>");
exit(0);
}

void htmlPushEarlyHandlers()
/* Push stuff to close out web page to make sensible error
 * message during initialization. */
{
pushWarnHandler(earlyWarningHandler);
pushAbortHandler(earlyAbortHandler);
}


static char *htmlStyle = 
    "<STYLE TYPE=\"text/css\">"
    ".hiddenText {background-color: silver}"
    ".normalText {background-color: white}"
    "</STYLE>\n";

char *htmlStyleUndecoratedLink =
/* Style that gets rid of underline of links. */
   "<STYLE TYPE=\"text/css\"> "
   "<!-- "
   "A {text-decoration: none} "
   "-->"
   "</STYLE>\n";

void htmlSetStyle(char *style)
/* Set document wide style. A favorite style to
 * use for many purposes is htmlStyleUndecoratedLink
 * which will remove underlines from links. 
 * Needs to be called before htmlStart or htmShell. */
{
htmlStyle = style;
}

static char *htmlBackground = NULL;

void htmlSetBackground(char *imageFile)
/* Set background - needs to be called before htmlStart
 * or htmShell. */
{
htmlBackground = imageFile;
}

static int htmlBgColor = 0xFFFFFF;
boolean gotBgColor = FALSE;

void htmlSetBgColor(int color)
/* Set background color - needs to be called before htmlStart
 * or htmShell. */
{
htmlBgColor = color;
gotBgColor = TRUE;
}

void htmlSetCookie(char* name, char* value, char* expires, char* path, char* domain, boolean isSecure)
/* create a cookie with the given stats */
{
char* encoded_name;
char* encoded_value;
char* encoded_path = NULL;

encoded_name = cgiEncode(name);
encoded_value = cgiEncode(value);
if(path != NULL)
	encoded_path = cgiEncode(path);

printf("Set-Cookie: %s=%s; ", encoded_name, encoded_value);

if(expires != NULL)
    printf("expires=%s; ", expires);

if(path != NULL)
    printf("path=%s; ", encoded_path);

if(domain != NULL)
    printf("domain=%s; ", domain);

if(isSecure == TRUE)
    printf("secure");

printf("\n");
}


void _htmStartWithHead(FILE *f, char *head, char *title)
/* Write out bits of header that both stand-alone .htmls
 * and CGI returned .htmls need, including optional head info */
{
fputs("<HTML>", f);
fprintf(f,"<HEAD>\n%s<TITLE>%s</TITLE>\n", head, title);
fprintf(f, "\t<META http-equiv=\"Content-Script-Type\" content=\"text/javascript\">\n");
if (htmlStyle != NULL)
    fputs(htmlStyle, f);
fputs("</HEAD>\n\n",f);
fputs("<BODY",f);
if (htmlBackground != NULL )
    fprintf(f, " BACKGROUND=\"%s\"", htmlBackground);
if (gotBgColor)
    fprintf(f, " BGCOLOR=\"%X\"", htmlBgColor);
fputs(">\n",f);
}

void _htmStart(FILE *f, char *title)
/* Write out bits of header that both stand-alone .htmls
 * and CGI returned .htmls need. */
{
_htmStartWithHead(f, "", title);
}



void htmlStart(char *title)
/* Write the start of an html from CGI */
{
puts("Content-Type:text/html");
puts("\n");

puts("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2//EN\">");
_htmStart(stdout, title);
}

void htmStartWithHead(FILE *f, char *head, char *title)
/* Write the start of a stand alone .html file, plus head info */
{
fputs("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2//EN\">\n", f);
_htmStartWithHead(f, head, title);
}

void htmStart(FILE *f, char *title)
/* Write the start of a stand alone .html file. */
{
htmStartWithHead(f, "", title);
}

/* Write the end of an html file */
void htmEnd(FILE *f)
{
fputs("\n</BODY>\n</HTML>\n", f);
}

/* Write the end of a stand-alone html file */
void htmlEnd()
{
htmEnd(stdout);
}

void htmlEchoInput()
{
}

void htmlBadVar(char *varName)
{
cgiBadVar(varName);
}

/* Display centered image file. */
void htmlImage(char *fileName, int width, int height)
{
printf("<P ALIGN=\"CENTER\"><IMG SRC=\"%s\" WIDTH=\"%d\" HEIGHT=\"%d\" ALIGN=\"BOTTOM\" BORDER=\"0\"></P>", fileName, width, height);
}


void htmErrOnlyShell(void (*doMiddle)())
/* Wrap error recovery around call to doMiddle. */
{
int status;

/* Set up error recovery. */
status = setjmp(htmlRecover);

/* Do your main thing. */
if (status == 0)
    {
    doMiddle();
    }
}

void htmEmptyShell(void (*doMiddle)(), char *method)
/* Wrap error recovery and and input processing around call to doMiddle. */
{
int status;

/* Set up error recovery (for out of memory and the like)
 * so that we finish web page regardless of problems. */
pushAbortHandler(htmlAbort);
pushWarnHandler(htmlVaWarn);
status = setjmp(htmlRecover);

/* Do your main thing. */
if (status == 0)
    {
    doMiddle();
    }

popWarnHandler();
popAbortHandler();
}


/* Wrap an html file around the passed in function.
 * The passed in function is already in the body. It
 * should just make paragraphs and return. 
 */
void htmShell(char *title, void (*doMiddle)(), char *method)
{
/* Preamble. */
dnaUtilOpen();
htmlStart(title);

/* Call wrapper for error handling. */
htmEmptyShell(doMiddle, method);

/* Post-script. */
htmlEnd();
}

/* Wrap an html file around the passed in function.
 * The passed in function is already in the body. It
 * should just make paragraphs and return. 
 * Method should be "query" or "get" or "post".
param title - The HTML page title
param head - The head text: can be a refresh directive or javascript
param method - The function pointer to execute in the middle
param method - The browser request method to use
 */
void htmShellWithHead( char *title, char *head, void (*doMiddle)(), char *method)
{
/* Preamble. */
dnaUtilOpen();

puts("Content-Type:text/html");
puts("\n");

puts("<HTML>");
printf("<HEAD>%s<TITLE>%s</TITLE>\n</HEAD>\n\n", head, title);
if (htmlBackground == NULL)
    puts("<BODY>\n");
else
    printf("<BODY BACKGROUND=\"%s\">\n", htmlBackground);

/* Call wrapper for error handling. */
htmEmptyShell(doMiddle, method);

/* Post-script. */
htmlEnd();
}

/* Include an HTML file in a CGI */
void htmlIncludeFile(char *path)
{
char *str = NULL;
size_t len = 0;

if (path == NULL)
    errAbort("Program error: including null file");
if (!fileExists(path))
   errAbort("Missing file %s", path); 
readInGulp(path, &str, &len);

if (len <= 0)
    errAbort("Error reading included file: %s", path);

puts(str);
freeMem(str);
}

/* Include an HTML file in a CGI.
 *   The file path is relative to the web server document root */
void htmlIncludeWebFile(char *file)
{
char path[256];
char *docRoot = "/usr/local/apache/htdocs";

safef(path, sizeof path, "%s/%s", docRoot, file);
htmlIncludeFile(path);
}

