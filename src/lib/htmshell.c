/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* htmshell - a shell to wrap around programs that generate
 * html files.  Write the html initial stuff (<head>, <body>, etc.)
 * and the final stuff too.  Also catch errors here so that
 * the html final stuff is written even if the program has
 * to abort. 
 *
 * This also includes a few routines to write commonly used
 * html constructs such as images, horizontal lines. etc. */

#include "common.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "errabort.h"
#include "dnautil.h"

jmp_buf htmlRecover;

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
htmlParagraph("<HR ALIGN=\"CENTER\">");
}

void htmHorizontalLine(FILE *f)
/* Print a horizontal line. */
{
fprintf(f, "<P><HR ALIGN=\"CENTER\"></P>");
}


void htmlVaWarn(char *format, va_list args)
/* Write an error message. */
{
htmlHorizontalLine();
htmlVaParagraph(format,args);
htmlHorizontalLine();
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

static char *htmlBackground = NULL;

void htmlSetBackground(char *imageFile)
/* Set background - needs to be called before htmlStart
 * or htmShell. */
{
htmlBackground = imageFile;
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

void _htmStart(FILE *f, char *title)
/* Write out bits of header that both stand-alone .htmls
 * and CGI returned .htmls need. */
{
fputs("<HTML>", f);
fprintf(f,"<HEAD>\n<TITLE>%s</TITLE>\n</HEAD>\n\n", title);
if (htmlBackground == NULL)
    fputs("<BODY>\n",f);
else
    fprintf(f, "<BODY BACKGROUND=\"%s\">\n", htmlBackground);
}

/* Write the start of an html from CGI */
void htmlStart(char *title)
{
puts("Content-Type:text/html");
puts("\n");

_htmStart(stdout, title);
}

void htmStart(FILE *f, char *title)
/* Write the start of a stand alone .html file. */
{
fputs("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2//EN\">\n", f);
_htmStart(f, title);
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

