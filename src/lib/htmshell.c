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

char *htmlTextStripTags(char *s)
/* Returns a cloned string with all html tags stripped out */
{
if (s == NULL)
    return NULL;
char *scrubbed = needMem(strlen(s));
char *from=s;
char *to=scrubbed;
while(*from!='\0')
    {
    if (*from == '<')
        {
        from++;
        while (*from!='\0' && *from != '>')
            from++;
        if (*from == '\0')  // The last open tag was never closed!
            break;
        from++;
        }
    else
        *to++ = *from++;
    }
return scrubbed;
}

char *htmlTextReplaceTagsWithChar(char *s, char ch)
/* Returns a cloned string with all html tags replaced with given char (useful for tokenizing) */
{
if (s == NULL)
    return NULL;
char *scrubbed = needMem(strlen(s) + 1);
char *from=s;
char *to=scrubbed;
while(*from!='\0')
    {
    if (*from == '<')
        {
        from++;
        *to++ = ch;
        while (*from!='\0' && *from != '>')
            from++;
        if (*from == '\0')  // The last open tag was never closed!
            break;
        from++;
        }
    else
        *to++ = *from++;
    }
*to = '\0';
return scrubbed;
}

char *htmlEncodeText(char *s,boolean tagsOkay)
/* Returns a cloned string with quotes replaced by html codes.
   Changes ',",\n and if not tagsOkay >,<,& to code equivalents.
   This differs from cgiEncode as it handles text that will
   be displayed in an html page or tooltip style title.  */
{
int size = strlen(s) + 3; // Add some slop
if (tagsOkay)
    size += countChars(s,'\n') * 4;
else
    {
    size += countChars(s,'>' ) * 4;
    size += countChars(s,'<' ) * 4;
    size += countChars(s,'&' ) * 5;
    size += countChars(s,'\n') * 6;
    }
size += countChars(s,'"' ) * 6;
size += countChars(s,'\'') * 5;
char *cleanQuote = needMem(size);
safecpy(cleanQuote,size,s);

// NOTE: While some internal HTML should work, a single quote (') will will screw it up!
if (tagsOkay)
    strSwapStrs(cleanQuote, size,"\n","<BR>" ); // new lines also break the html
else
    {
    strSwapStrs(cleanQuote, size,"&","&amp;" );  // '&' is not the start of a control char
    strSwapStrs(cleanQuote, size,">","&gt;"  );  // '>' is not the close of a tag
    strSwapStrs(cleanQuote, size,"<","&lt;"  );  // '<' is not the open of a tag
    if (cgiClientBrowser(NULL,NULL,NULL) == btFF)
        strSwapStrs(cleanQuote, size,"\n","&#124;"); // FF does not support!  Use "&#124;" for '|'
                                                     // instead
    else
        strSwapStrs(cleanQuote, size,"\n","&#x0A;"); // '\n' is supported on some browsers
    }
strSwapStrs(cleanQuote, size,"\"","&quot;"); // Shield double quotes
strSwapStrs(cleanQuote, size,"'" ,"&#39;" ); // Shield single quotes

return cleanQuote;
}

char *attributeEncode(char *str)
{
// encode double and single quotes in a string to be used as an element attribute
return replaceChars(replaceChars(str, "\"", "&quot;"), "'", "&#39;");
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

void htmlWarnBoxSetup(FILE *f)
/* Creates an invisible, empty warning box than can be filled with errors
 * and then made visible. */
{
// Only set this up once per page
static boolean htmlWarnBoxSetUpAlready=FALSE;
if (htmlWarnBoxSetUpAlready)
    return;
htmlWarnBoxSetUpAlready=TRUE;

// NOTE: Making both IE and FF work is almost impossible.  Currently, in IE, if the message
// is forced to the top (calling this routine after <BODY> then the box is not resizable
// (dynamically adjusting to its contents). But if this setup is done later in the page
// (at first warning), then IE does resize it.  Why?
// FF3.0 (but not FF2.0) was resizable with the following, but it took some experimentation.
// Remember what worked nicely on FF3.0:
//      "var app=navigator.appName.substr(0,9); "
//      "if(app == 'Microsoft') {warnBox.style.display='';} 
//       else {warnBox.style.display=''; warnBox.style.width='auto';}"
fprintf(f, "<script type='text/javascript'>\n");
fprintf(f, "document.write(\"<center>"
            "<div id='warnBox' style='display:none; background-color:Beige; "
              "border: 3px ridge DarkRed; width:640px; padding:10px; margin:10px; "
              "text-align:left;'>"
            "<CENTER><B id='warnHead' style='color:DarkRed;'></B></CENTER>"
	    "<UL id='warnList'></UL>"
            "<CENTER><button id='warnOK' onclick='hideWarnBox();return false;'></button></CENTER>"
            "</div></center>\");\n");
fprintf(f,"function showWarnBox() {"
            "document.getElementById('warnOK').innerHTML='&nbsp;OK&nbsp;';"
            "var warnBox=document.getElementById('warnBox');"
            "warnBox.style.display=''; warnBox.style.width='65%%';"
            "document.getElementById('warnHead').innerHTML='Warning/Error(s):';"
            "window.scrollTo(0, 0);"
          "}\n");
fprintf(f,"function hideWarnBox() {"
            "var warnBox=document.getElementById('warnBox');"
            "warnBox.style.display='none';warnBox.innerHTML='';"
            "var endOfPage = document.body.innerHTML.substr(document.body.innerHTML.length-20);"
            "if(endOfPage.lastIndexOf('-- ERROR --') > 0) { history.back(); }"
          "}\n"); // Note OK button goes to prev page when this page is interrupted by the error.
fprintf(f,"window.onunload = function(){}; // Trick to avoid FF back button issue.\n");
fprintf(f,"</script>\n");
}

void htmlVaWarn(char *format, va_list args)
/* Write an error message. */
{
va_list argscp;
va_copy(argscp, args);
htmlWarnBoxSetup(stdout); // sets up the warnBox if it hasn't already been done.
char warning[1024];
vsnprintf(warning,sizeof(warning),format, args);
char *encodedMessage = htmlEncodeText(warning,TRUE); // NOTE: While some internal HTML should work,
                                                     // a single quote (') will will screw it up!
printf("<script type='text/javascript'>{showWarnBox();"
        "var warnList=document.getElementById('warnList');"
        "warnList.innerHTML += '<li>%s</li>';}</script><!-- ERROR -->\n",encodedMessage); 
                                     // NOTE that "--ERROR --" is needed at the end of this print!!
freeMem(encodedMessage);

/* Log useful CGI info to stderr */
logCgiToStderr();

/* write warning/error message to stderr so they get logged. */
vfprintf(stderr, format, argscp);
va_end(argscp);
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

// optional style set by theme, added after main style and thus
// can overwrite main style settings
static char *htmlStyleTheme = NULL;

void htmlSetStyle(char *style)
/* Set document wide style. A favorite style to
 * use for many purposes is htmlStyleUndecoratedLink
 * which will remove underlines from links.
 * Needs to be called before htmlStart or htmShell. */
{
htmlStyle = style;
}

static char *htmlStyleSheet = NULL;
void htmlSetStyleSheet(char *styleSheet)
/* Set document wide style sheet by adding css name to HEAD part.
 * Needs to be called before htmlStart or htmShell. */
{
htmlStyleSheet = styleSheet;
}

static char *htmlFormClass = NULL;
void htmlSetFormClass(char *formClass)
/* Set class in the BODY part. */
{
htmlFormClass = formClass;
}

void htmlSetStyleTheme(char *style)
/* Set theme style. Needs to be called before htmlStart or htmShell. */
{
htmlStyleTheme = style;
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


void _htmStartWithHead(FILE *f, char *head, char *title, boolean printDocType, int dirDepth)
/* Write out bits of header that both stand-alone .htmls
 * and CGI returned .htmls need, including optional head info */
{
if (printDocType)
    {
//#define TOO_TIMID_FOR_CURRENT_HTML_STANDARDS
#ifdef TOO_TIMID_FOR_CURRENT_HTML_STANDARDS
    fputs("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2//EN\">\n", f);
#else///ifndef TOO_TIMID_FOR_CURRENT_HTML_STANDARDS
    char *browserVersion;
    if (btIE == cgiClientBrowser(&browserVersion, NULL, NULL) && *browserVersion < '8')
        fputs("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2//EN\">\n", f);
    else
        fputs("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" "
              "\"http://www.w3.org/TR/html4/loose.dtd\">",f);
    // Strict would be nice since it fixes atleast one IE problem (use of :hover CSS pseudoclass)
#endif///ndef TOO_TIMID_FOR_CURRENT_HTML_STANDARDS
    }
fputs("<HTML>", f);
fprintf(f,"<HEAD>\n%s<TITLE>%s</TITLE>\n", head, title);
fprintf(f, "\t<META http-equiv=\"Content-Script-Type\" content=\"text/javascript\">\n");
if (htmlStyle != NULL)
    fputs(htmlStyle, f);
if (htmlStyleSheet != NULL)
    fprintf(f,"<link href=\"%s\" rel=\"stylesheet\" type=\"text/css\">\n", htmlStyleSheet);
if (htmlStyleTheme != NULL)
    fputs(htmlStyleTheme, f);

fputs("</HEAD>\n\n",f);
fputs("<BODY",f);
if (htmlFormClass != NULL )
    fprintf(f, " CLASS=\"%s\"", htmlFormClass);
if (htmlBackground != NULL )
    fprintf(f, " BACKGROUND=\"%s\"", htmlBackground);
if (gotBgColor)
    fprintf(f, " BGCOLOR=\"#%X\"", htmlBgColor);
fputs(">\n",f);

htmlWarnBoxSetup(f);
}


void htmlStart(char *title)
/* Write the start of an html from CGI */
{
puts("Content-Type:text/html");
puts("\n");
_htmStartWithHead(stdout, "", title, TRUE, 1);
}

void htmStartWithHead(FILE *f, char *head, char *title)
/* Write the start of a stand alone .html file, plus head info */
{
_htmStartWithHead(f, head, title, TRUE, 1);
}

void htmStart(FILE *f, char *title)
/* Write the start of a stand alone .html file. */
{
htmStartWithHead(f, "", title);
}

void htmStartDirDepth(FILE *f, char *title, int dirDepth)
/* Write the start of a stand alone .html file.  dirDepth is the number of levels
 * beneath apache root that caller's HTML will appear to the web client.
 * E.g. if writing HTML from cgi-bin, dirDepth is 1; if trash/body/, 2. */
{
_htmStartWithHead(f, "", title, TRUE, dirDepth);
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

htmlWarnBoxSetup(stdout);// Sets up a warning box which can be filled with errors as they occur

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

