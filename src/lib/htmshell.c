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
#include "errAbort.h"
#include "dnautil.h"
#include "base64.h"


jmp_buf htmlRecover;

boolean htmlWarnBoxSetUpAlready=FALSE;

static bool NoEscape = FALSE;

static bool errorsNoHeader = FALSE;

void htmlSuppressErrors()
/* Do not output a http header for error messages. Makes sure that very early
 * errors are not shown back to the user but trigger a 500 error, */
{
errorsNoHeader = TRUE;
}

void htmlNoEscape()
{
NoEscape = TRUE;
}

void htmlDoEscape()
{
NoEscape = FALSE;
}

void htmlVaEncodeErrorText(char *format, va_list args)
/* Write an error message encoded against XSS. */
{
va_list argscp;
va_copy(argscp, args);
char warning[1024];

struct dyString *ds = newDyString(1024);
vaHtmlDyStringPrintf(ds, format, args);
int n = ds->stringSize;
int nLimit = sizeof(warning) - 1;
if (ds->stringSize > nLimit)
    n = nLimit;
safencpy(warning, sizeof warning, ds->string, n);
if (ds->stringSize > nLimit)
    strcpy(warning+n-5,"[...]");  // indicated trucation
freeDyString(&ds);

fprintf(stdout, "%s\n", warning);
/* write warning/error message to stderr so they get logged. */
vfprintf(stderr, format, argscp);
fprintf(stderr, "\n");
fflush(stderr);
va_end(argscp);
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
while (*from!='\0')
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

char *htmlTextStripJavascriptCssAndTags(char *s)
/* Returns a cloned string with all inline javascript, css, and html tags stripped out */
{
if (s == NULL)
    return NULL;
char *scrubbed = needMem(strlen(s));
char *from=s;
char *to=scrubbed;
while (*from!='\0')
    {
    if (startsWithNoCase("<script", from))
        {
        from++;
        while (*from!='\0' && !startsWithNoCase("</script>", from))
            from++;
        if (*from == '\0')  // The last open tag was never closed!
            break;
        from += strlen("</script>");
        *to++ = ' ';
        }
    else if (startsWithNoCase("<style", from))
        {
        from++;
        while (*from!='\0' && !startsWithNoCase("</style>", from))
            from++;
        if (*from == '\0')  // The last open tag was never closed!
            break;
        from += strlen("</style>");
        *to++ = ' ';
        }
    else if (*from == '<')
        {
        from++;
        while (*from!='\0' && *from != '>')
            from++;
        if (*from == '\0')  // The last open tag was never closed!
            break;
        from++;
        *to++ = ' ';
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


char *htmlWarnEncode(char *s)
/* Returns a cloned string with newlines replaced by BR tag.
   Meant to be displayed with warn popup box. */
{
int size = strlen(s);
size += countChars(s,'\n') * 4;
char *cleanQuote = needMem(size+1);
safecpy(cleanQuote,size+1,s);

strSwapStrs(cleanQuote, size,"\n","<BR>" ); // use BR tag for new lines

return cleanQuote;
}



int htmlEncodeTextExtended(char *s, char *out, int outSize)
/* Replaces required punctuation characters with html entities to fight XSS. 
 * out result must be large enough to receive the encoded string.
 * Returns size of encoded string or -1 if output larger than outSize. 
 * To just get the final encoded size, pass in NULL for out and 0 for outSize. 
 * To output without checking sizes, pass in non-NULL for out and 0 for outSize. 
 */
{
int total = 0;
char c = 0;
do
    {
    c=*s++;
    int size = 1;
    char *newString = NULL; 
    if (c == '&') { size = 5; newString = "&amp;"; } // '&' start a control char
    if (c == '>') { size = 4; newString = "&gt;" ; } // '>' close of tag
    if (c == '<') { size = 4; newString = "&lt;" ; } // '<' open  of tag
    if (c == '/')  { size = 6; newString = "&#x2F;"; } // forward slash helps end an HTML entity
    if (c == '"')  { size = 6; newString = "&quot;"; } // double quote
    if (c == '\'') { size = 5; newString = "&#39;" ; } // single quote
    if (out)
	{
	if (outSize > 0 && (total+size+1) > outSize) // 1 for terminator
	    {
	    *out = 0;
	    return -1;
	    }
	if (size == 1)
	    *out++ = c;
	else
	    {
	    strncpy(out, newString, size);
	    out += size;
	    }
	}
    total += size;
    } while (c != 0);
return total - 1; // do not count terminating 0
}

int htmlEncodeTextSize(char *s)
/* Returns what the encoded size will be after replacing characters with html codes or entities. */
{
return htmlEncodeTextExtended(s, NULL, 0);
}


char *htmlEncode(char *s)
/* Returns a cloned string with quotes replaced by html codes.
   Changes ',",\n and >,<,& to code equivalents.
   This differs from cgiEncode as it handles text that will
   be displayed in an html page or tooltip style title.  */
{
int size = htmlEncodeTextSize(s);
char *out = needMem(size+1);
htmlEncodeTextExtended(s, out, size+1);
return out;
}

int nonAlphaNumericHexEncodeText(char *s, char *out, int outSize, 
   char *prefix, char *postfix)
/* For html tag attributes, it replaces non-alphanumeric characters
 * with <prefix>HH<postfix> hex codes to fight XSS.
 * out result must be large enough to receive the encoded string.
 * Returns size of encoded string or -1 if output larger than outSize. 
 * To just get the final encoded size, pass in NULL for out and 0 for outSize. 
 * To output without checking sizes, pass in non-NULL for out and 0 for outSize. 
 */
{
int encodedSize = strlen(prefix) + 2 + strlen(postfix);
int total = 0;
char c = 0;
do
    {
    c=*s++;
    int size = 1;
    if (!isalnum(c)) // alpha-numeric
	{
	size = encodedSize;
	}
    if (c == 0)
	size = 1;    // do not encode the terminating 0
    if (out)
	{
	if (outSize > 0 && (total+size+1) > outSize) // 1 for terminator
	    {
	    *out = 0;
	    return -1;
	    }
	if (size == 1)
	    *out++ = c;
	else
	    {
	    char x;
	    char *pf = prefix;
	    while ((x = *pf++) != 0)
		*out++ = x;
	    // use (unsigned char) to shift without sign-extension. We want zeros to be added on left side.
	    char h1 = ((unsigned char) c >> 4 ) + 0x30; if (h1 > 0x39) h1 += 7; 
	    *out++ = h1;
	    char h2 = (c & 0xF) + 0x30; if (h2 > 0x39) h2 += 7;
	    *out++ = h2;
	    pf = postfix;
	    while ((x = *pf++) != 0)
		*out++ = x;
	    }
	}
    total += size;
    } while (c != 0);
return total - 1; // do not count terminating 0
}

static boolean decodeOneHexChar(char c, char *h)
/* Return true if c is a hex char and decode it to h. */
{
*h = *h << 4;
if (c >= '0' && c <= '9')
    *h += (c - '0');	    
else if (c >= 'A' && c <= 'F')
    *h += (c - 'A' + 10);	    
else if (c >= 'a' && c <= 'f')
    *h += (c - 'a' + 10);
else
    return FALSE;
return TRUE;
}

static boolean decodeTwoHexChars(char *s, char *h)
/* Return true if hex char */
{
*h = 0;
if (decodeOneHexChar(*s++, h)
&& (decodeOneHexChar(*s  , h)))
    return TRUE;
return FALSE;
}

void nonAlphaNumericHexDecodeText(char *s, char *prefix, char *postfix)
/* For html tag attributes, it decodes non-alphanumeric characters
 * with <prefix>HH<postfix> hex codes.
 * Decoding happens in-place, changing the input string s.
 * prefix must not be empty string or null, but postfix can be empty string.
 * Because the decoded string is always equal to or shorter than the input string,
 * the decoding is just done in-place modifying the input string.
 * Accepts upper and lower case values in entities.
 */
{
char *d = s;  // where are we decoding to right now
int pfxLen = strlen(prefix);
int postLen = strlen(postfix);
while (isNotEmpty(s))
    {
    char h;
    if (startsWithNoCase(prefix, s) &&
        decodeTwoHexChars(s+pfxLen, &h) &&
        startsWithNoCase(postfix, s+pfxLen+2))
        {
        *d++ = h;
        s += pfxLen + 2 + postLen;
        }
    else
        *d++ = *s++;
    }
*d = 0;
}

int attrEncodeTextExtended(char *s, char *out, int outSize)
/* For html tag attribute values, it replaces non-alphanumeric characters
 * with html entities &#xHH; to fight XSS.
 * out result must be large enough to receive the encoded string.
 * Returns size of encoded string or -1 if output larger than outSize. 
 * To just get the final encoded size, pass in NULL for out and 0 for outSize. 
 * To output without checking sizes, pass in non-NULL for out and 0 for outSize. 
 */
{
return nonAlphaNumericHexEncodeText(s, out, outSize, "&#x", ";");
}

int attrEncodeTextSize(char *s)
/* Returns what the encoded size will be after replacing characters with escape codes. */
{
return attrEncodeTextExtended(s, NULL, 0);
}

char *attributeEncode(char *s)
/* Returns a cloned string with non-alphanumeric characters replaced by escape codes. */
{
int size = attrEncodeTextSize(s);
char *out = needMem(size+1);
attrEncodeTextExtended(s, out, size+1);
return out;
}

void attributeDecode(char *s)
/* For html tag attribute values decode html entities &#xHH; */
{
return nonAlphaNumericHexDecodeText(s, "&#x", ";");
}



int cssEncodeTextExtended(char *s, char *out, int outSize)
/* For CSS values, it replaces non-alphanumeric characters with "\HH " to fight XSS.
 * (Yes, the trailing space is critical.)
 * out result must be large enough to receive the encoded string.
 * Returns size of encoded string or -1 if output larger than outSize. 
 * To just get the final encoded size, pass in NULL for out and 0 for outSize. 
 * To output without checking sizes, pass in non-NULL for out and 0 for outSize. 
 */
{
return nonAlphaNumericHexEncodeText(s, out, outSize, "\\", " ");
}

int cssEncodeTextSize(char *s)
/* Returns what the encoded size will be after replacing characters with escape codes. */
{
return cssEncodeTextExtended(s, NULL, 0);
}

char *cssEncode(char *s)
/* Returns a cloned string with non-alphanumeric characters replaced by escape codes. */
{
int size = cssEncodeTextSize(s);
char *out = needMem(size+1);
cssEncodeTextExtended(s, out, size+1);
return out;
}

void cssDecode(char *s)
/* For CSS values decode "\HH " 
 * (Yes, the trailing space is critical.) */
{
return nonAlphaNumericHexDecodeText(s, "\\", " ");
}



int javascriptEncodeTextExtended(char *s, char *out, int outSize)
/* For javascript string values, it replaces non-alphanumeric characters with "\xHH" to fight XSS.
 * out result must be large enough to receive the encoded string.
 * Returns size of encoded string or -1 if output larger than outSize. 
 * To just get the final encoded size, pass in NULL for out and 0 for outSize. 
 * To output without checking sizes, pass in non-NULL for out and 0 for outSize. 
 */
{
return nonAlphaNumericHexEncodeText(s, out, outSize, "\\x", "");
}

int javascriptEncodeTextSize(char *s)
/* Returns what the encoded size will be after replacing characters with escape codes. */
{
return javascriptEncodeTextExtended(s, NULL, 0);
}

char *javascriptEncode(char *s)
/* Returns a cloned string with non-alphanumeric characters replaced by escape codes. */
{
int size = javascriptEncodeTextSize(s);
char *out = needMem(size+1);
javascriptEncodeTextExtended(s, out, size+1);
return out;
}

void jsDecode(char *s)
/* For JS string values decode "\xHH" */
{
return nonAlphaNumericHexDecodeText(s, "\\x", "");
}


int urlEncodeTextExtended(char *s, char *out, int outSize)
/* For URL parameter values, it replaces non-alphanumeric characters with "%HH" to fight XSS.
 * out result must be large enough to receive the encoded string.
 * Returns size of encoded string or -1 if output larger than outSize. 
 * To just get the final encoded size, pass in NULL for out and 0 for outSize. 
 * To output without checking sizes, pass in non-NULL for out and 0 for outSize. 
 */
{
return nonAlphaNumericHexEncodeText(s, out, outSize, "%", "");
}

int urlEncodeTextSize(char *s)
/* Returns what the encoded size will be after replacing characters with escape codes. */
{
return urlEncodeTextExtended(s, NULL, 0);
}

char *urlEncode(char *s)
/* Returns a cloned string with non-alphanumeric characters replaced by escape codes. */
{
int size = urlEncodeTextSize(s);
char *out = needMem(size+1);
urlEncodeTextExtended(s, out, size+1);
return out;
}

void urlDecode(char *s)
/* For URL paramter values decode "%HH" */
{
return nonAlphaNumericHexDecodeText(s, "%", "");
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
if (htmlWarnBoxSetUpAlready)
    return;
htmlWarnBoxSetUpAlready=TRUE;

// NOTE: There is a duplicate of this function in hg/js/utils.js

// NOTE: Making both IE and FF work is almost impossible.  Currently, in IE, if the message
// is forced to the top (calling this routine after <BODY> then the box is not resizable
// (dynamically adjusting to its contents). But if this setup is done later in the page
// (at first warning), then IE does resize it.  Why?
// FF3.0 (but not FF2.0) was resizable with the following, but it took some experimentation.
// Remember what worked nicely on FF3.0:
//      "var app=navigator.appName.substr(0,9); "
//      "if(app == 'Microsoft') {warnBox.style.display='';} 
//       else {warnBox.style.display=''; warnBox.style.width='auto';}"
struct dyString *dy = dyStringNew(2048);

fprintf(f,"<center>"
            "<div id='warnBox' style='display:none;'>"
            "<CENTER><B id='warnHead'></B></CENTER>"
            "<UL id='warnList'></UL>"
            "<CENTER><button id='warnOK'></button></CENTER>"
            "</div></center>\n");
// TODO we should just move these warnBox functions to utils.js or warning.js or something.
dyStringPrintf(dy,"function showWarnBox() {"
            "document.getElementById('warnOK').innerHTML='&nbsp;OK&nbsp;';"
            "var warnBox=document.getElementById('warnBox');"
            "warnBox.style.display=''; warnBox.style.width='65%%';"
            "document.getElementById('warnHead').innerHTML='Warning/Error(s):';"
            "window.scrollTo(0, 0);"
          "}\n");
dyStringPrintf(dy,"function hideWarnBox() {"
            "var warnBox=document.getElementById('warnBox');"
            "warnBox.style.display='none';"
            "var warnList=document.getElementById('warnList');"
	    "warnList.innerHTML='';"
            "var endOfPage = document.body.innerHTML.substr(document.body.innerHTML.length-20);"  
// TODO maybe just looking at the last 20 characters of the html page is no longer enough
// because the final js inline trash temp gets emitted. Looks like it is 93 characters long but could be longer.
// This might be old cruft needed for a browser issue that no longer exists?
            "if(endOfPage.lastIndexOf('-- ERROR --') > 0) { history.back(); }"
          "}\n"); // Note OK button goes to prev page when this page is interrupted by the error.
// Added by Galt
dyStringPrintf(dy,"document.getElementById('warnOK').onclick = function() {hideWarnBox();return false;};\n");
dyStringPrintf(dy,"window.onunload = function(){}; // Trick to avoid FF back button issue.\n");

jsInline(dy->string);
dyStringFree(&dy);
}

void htmlVaWarn(char *format, va_list args)
/* Write an error message. */
{
va_list argscp;
va_copy(argscp, args);
htmlWarnBoxSetup(stdout); // sets up the warnBox if it hasn't already been done.
char warning[1024];

// html-encode arguments to fight XSS
struct dyString *ds = newDyString(1024);
vaHtmlDyStringPrintf(ds, format, args);
int n = ds->stringSize;
int nLimit = sizeof(warning) - 1;
if (ds->stringSize > nLimit)
    n = nLimit;
safencpy(warning, sizeof warning, ds->string, n);
if (ds->stringSize > nLimit)
    strcpy(warning+n-5,"[...]"); // show truncation
freeDyString(&ds);

// Replace newlines with BR tag
char *warningBR = htmlWarnEncode(warning); 

// Javascript-encode the entire message because it is
// going to appear as a javascript string literal
// as it gets appended to the warnList html.
// JS-encoding here both allows us to use any character in the message
// and keeps js-encodings in events like onmouseover="stuff %s|js| stuff" secure.
char *jsEncodedMessage = javascriptEncode (warningBR); 
freeMem(warningBR);
struct dyString *dy = dyStringNew(2048);
dyStringPrintf(dy,
    "showWarnBox();"
    "var warnList=document.getElementById('warnList');"
    "warnList.innerHTML += '<li>%s</li>';\n", jsEncodedMessage);
jsInline(dy->string);  
// TODO GALT does --ERROR -- tag still work?
printf("<!-- ERROR -->\n");
// NOTE that "--ERROR --" is needed at the end of this print!!
dyStringFree(&dy);
freeMem(jsEncodedMessage);

/* Log useful CGI info to stderr */
logCgiToStderr();

/* write warning/error message to stderr so they get logged. */
vfprintf(stderr, format, argscp);
fprintf(stderr, "\n");
fflush(stderr);
va_end(argscp);
}

void htmlVaBadRequestAbort(char *format, va_list args)
/* Print out an HTTP header 400 status code (Bad Request) and message, then exit with error.
 * NOTE: This must be installed using pushWarnHandler (pushAbortHandler optional) because
 * vaErrAbort calls vaWarn and then noWarnAbort.  So if the defaut warn handler is used, then
 * the error message will be printed out by defaultVaWarn before this prints out the header. */
{
puts("Status: 400\r");
puts("Content-Type: text/plain; charset=UTF-8\r");
puts("\r");
if (format != NULL && args != NULL)
    {
    vfprintf(stdout, format, args);
    fprintf(stdout, "\n");
    }
exit(-1);
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
if (!initted && !errorsNoHeader)
    {
    htmlStart("Very Early Error");
    initted = TRUE;
    }
printf("%s", htmlWarnStartPattern());
fputs("<P>", stdout);
htmlVaEncodeErrorText(format,args);
fputs("</P>\n", stdout);
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

void printBodyTag(FILE *f)
{
// print starting BODY tag, including any appropriate attributes (class, background and bgcolor). 
fprintf(f, "<BODY");
struct slName *classes = NULL;

slNameAddHead(&classes, "cgi");
char *scriptName = cgiScriptName();
if(isNotEmpty(scriptName))
    {
    char buf[FILENAME_LEN];
    splitPath(scriptName, NULL, buf, NULL);
    slNameAddHead(&classes, cloneString(buf));
}
if (htmlFormClass != NULL )
    slNameAddHead(&classes, htmlFormClass);
fprintf(f, " CLASS=\"%s\"", slNameListToString(classes, ' '));

if (htmlBackground != NULL )
    fprintf(f, " BACKGROUND=\"%s\"", htmlBackground);
if (gotBgColor)
    fprintf(f, " BGCOLOR=\"#%X\"", htmlBgColor);
fputs(">\n",f);
}

//--- NONCE and CSP routines -------------

char *makeRandomKey(int numBits)
/* Generate base64 encoding of a random key of at least size numBits returning string to be freed when done */
{
int numBytes = (numBits + 7) / 8;  // round up to nearest whole byte.
numBytes = ((numBytes+2)/3)*3;  // round up to the nearest multiple of 3 to avoid equals-char padding in base64 output
FILE *f = mustOpen("/dev/urandom", "r");   // open random system device for read-only access.
char *binaryString = needMem(numBytes);
mustRead(f, binaryString, numBytes);
carefulClose(&f);
char * result = base64Encode(binaryString, numBytes); // converts 3 binary bytes into 4 printable characters
int len = strlen(result);
memSwapChar(result, len, '+', 'A'); // replace + and / with characters that are URL-friendly.
memSwapChar(result, len, '/', 'a');
freeMem(binaryString);
return result;
}

char *nonce = NULL;

char *getNonce()
/* make nonce one-use-per-page */
{
if (!nonce)
    {
    nonce = makeRandomKey(128+33); // at least 128 bits of protection, 33 for the world population size.
    }
return nonce;
}

char *getNoncePolicy()
/* get nonce policy clause */
{
char nonceClause[1024];
safef(nonceClause, sizeof nonceClause, "'nonce-%s'", getNonce());
return cloneString(nonceClause);
}

/* CSP2 Usage Notes
One can add js scripts to a DOM dynamically.
It is critical to use script.setAttribute('nonce', pageNonce);
instead of script.nonce = pageNonce; because nonce is an html attribute,
not a javascript attribute.  In places where this is used (like alleles.js)
we should mark this with CSP2 comment because someday CSP3 will automatically
be able to pass the nonce to script children and the command will no longer be 
needed.
*/

/* CSP3 Usage Notes
 Because of technical backwards-compatibility issues, we cannot use CSP3 until virtually all browsers
 our users use support it.  That could mean perhaps around the year 2021.
 See README-CSP3.txt for details.
*/


char *getCspPolicyString()
/* get the policy string */
{
// example "default-src 'self'; child-src 'none'; object-src 'none'"
struct dyString *policy = dyStringNew(1024);
dyStringAppend(policy, "default-src *;");

/* more secure method not used yet 
dyStringAppend(policy, "default-src 'self';");

dyStringAppend(policy, "  child-src 'self';");
*/

dyStringAppend(policy, " script-src 'self' blob:");
// Trick for backwards compatibility with browsers that understand CSP1 but not nonces (CSP2).
dyStringAppend(policy, " 'unsafe-inline'");
// For browsers that DO understand nonces and CSP2, they ignore 'unsafe-inline' in script if nonce is present.
char *noncePolicy=getNoncePolicy();
dyStringPrintf(policy, " %s", noncePolicy);
freeMem(noncePolicy);
dyStringAppend(policy, " code.jquery.com");          // used by hgIntegrator jsHelper and others
dyStringAppend(policy, " www.google-analytics.com"); // used by google analytics
// cirm cdw lib and web browse
dyStringAppend(policy, " www.samsarin.com/project/dagre-d3/latest/dagre-d3.js");
dyStringAppend(policy, " cdnjs.cloudflare.com/ajax/libs/d3/3.4.4/d3.min.js");
dyStringAppend(policy, " cdnjs.cloudflare.com/ajax/libs/jquery/1.12.1/jquery.min.js");
dyStringAppend(policy, " cdnjs.cloudflare.com/ajax/libs/jstree/3.2.1/jstree.min.js");
dyStringAppend(policy, " cdnjs.cloudflare.com/ajax/libs/bowser/1.6.1/bowser.min.js");
dyStringAppend(policy, " cdnjs.cloudflare.com/ajax/libs/jstree/3.3.4/jstree.min.js");
dyStringAppend(policy, " cdnjs.cloudflare.com/ajax/libs/jstree/3.3.7/jstree.min.js");
dyStringAppend(policy, " login.persona.org/include.js");
dyStringAppend(policy, " cdnjs.cloudflare.com/ajax/libs/popper.js/1.12.9/umd/popper.min.js");
// expMatrix
dyStringAppend(policy, " ajax.googleapis.com");
dyStringAppend(policy, " maxcdn.bootstrapcdn.com");
dyStringAppend(policy, " d3js.org/d3.v3.min.js");
// jsHelper
dyStringAppend(policy, " cdn.datatables.net");

dyStringAppend(policy, ";");


dyStringAppend(policy, " style-src * 'unsafe-inline';");

/* more secure method not used yet 
dyStringAppend(policy, " style-src 'self' 'unsafe-inline'");
dyStringAppend(policy, " code.jquery.com");          // used by hgIntegrator
dyStringAppend(policy, " netdna.bootstrapcdn.com");  // used by hgIntegrator
dyStringAppend(policy, " fonts.googleapis.com");    // used by hgGateway
dyStringAppend(policy, " maxcdn.bootstrapcdn.com"); // used by hgGateway
dyStringAppend(policy, ";");
*/

// The data: protocol is used by popular browser extensions.
// It seems to be safe and it is too bad that it must be explicitly included.
dyStringAppend(policy, " font-src * data:;");

/* more secure method not used yet 
dyStringAppend(policy, " font-src 'self'");
dyStringAppend(policy, " netdna.bootstrapcdn.com");  // used by hgIntegrator
dyStringAppend(policy, " maxcdn.bootstrapcdn.com"); // used by hgGateway
dyStringAppend(policy, " fonts.gstatic.com");       // used by hgGateway
dyStringAppend(policy, ";");

dyStringAppend(policy, " object-src 'none';");

*/

dyStringAppend(policy, " img-src * data:;");  

/* more secure method not used yet 
dyStringAppend(policy, " img-src 'self'");
// used by hgGene for modbaseimages in hg/hgc/lowelab.c hg/protein/lib/domains.c hg/hgGene/domains.c
dyStringAppend(policy, " modbase.compbio.ucsf.edu");  
dyStringAppend(policy, " hgwdev.gi.ucsc.edu"); // used by visiGene
dyStringAppend(policy, " genome.ucsc.edu"); // used by visiGene
dyStringAppend(policy, " code.jquery.com");          // used by hgIntegrator
dyStringAppend(policy, " www.google-analytics.com"); // used by google analytics
dyStringAppend(policy, " stats.g.doubleclick.net"); // used by google analytics
dyStringAppend(policy, ";");
*/
return dyStringCannibalize(&policy);
}

char *getCspMetaString(char *policy)
/* get the policy string as an html header meta tag */
{
char meta[1024];
safef(meta, sizeof meta, "<meta http-equiv='Content-Security-Policy' content=\"%s\">\n", policy); 
// use double quotes around policy because it contains single-quoted values.
return cloneString(meta);
}

char *getCspMetaResponseHeader(char *policy)
/* get the policy string as an http response header */
{
char response[4096];
safef(response, sizeof response, "Content-Security-Policy: %s\n", policy); 
return cloneString(response);
}

char *getCspMetaHeader()
/* return meta CSP header string */
{
char *policy = getCspPolicyString();
char *meta = getCspMetaString(policy);
freeMem(policy);
return meta;
}

void generateCspMetaHeader(FILE *f)
/* generate meta CSP header */
{
char *meta = getCspMetaHeader();
fputs(meta, f);
freeMem(meta);
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
              "\"http://www.w3.org/TR/html4/loose.dtd\">\n",f);
    // Strict would be nice since it fixes atleast one IE problem (use of :hover CSS pseudoclass)
#endif///ndef TOO_TIMID_FOR_CURRENT_HTML_STANDARDS
    }
fputs("<HTML>\n", f);
fputs("<HEAD>\n", f);
// CSP header
generateCspMetaHeader(f);

fputs(head, f);
htmlFprintf(f,"<TITLE>%s</TITLE>\n", title); 
if (endsWith(title,"Login - UCSC Genome Browser")) 
    fprintf(f,"\t<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html;CHARSET=iso-8859-1\">\n");
fprintf(f, "\t<META http-equiv=\"Content-Script-Type\" content=\"text/javascript\">\n");
if (htmlStyle != NULL)
    fputs(htmlStyle, f);
if (htmlStyleSheet != NULL)
    fprintf(f,"<link href=\"%s\" rel=\"stylesheet\" type=\"text/css\">\n", htmlStyleSheet);
if (htmlStyleTheme != NULL)
    fputs(htmlStyleTheme, f);

fputs("</HEAD>\n\n",f);
printBodyTag(f);
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
if (f == stdout)  // not html for a frame of a frameset
    jsInlineFinish();
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
printBodyTag(stdout);

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


/* ===== HTML printf-style escaping functions ===== */

int htmlSafefAbort(boolean noAbort, int errCode, char *format, ...)
/* handle noAbort stderror logging and errAbort */
{
va_list args;
va_start(args, format);
if (noAbort)
    {
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    fflush(stderr);
    }
else
    {
    vaErrAbort(format, args);
    }
va_end(args);
return errCode;
}



#define htmlSafefPunc 0x01  // using char 1 as special char to denote strings needing escaping
enum htmlSafefEncoding {dummyzero, none, html, js, css, attr, url};

int htmlEscapeAllStrings(char *buffer, char *s, int bufSize, boolean noAbort, boolean noWarnOverflow)
/* Escape all strings. *
 * Returns final size not including terminating 0. 
 * User needs to pre-allocate enough space that escape functions will never run out of space.
 * This function should be efficient on statements with many strings to be escaped. */
{
char *sOrig = s;
int sz = 0;
int remainder = bufSize;
boolean done = FALSE;
while (1)
    {
    char *start = strchr(s, htmlSafefPunc);
    char *end = NULL;
    if (start)
	{
    	end = strchr(start+1, htmlSafefPunc); // skip over punc mark
	if (!end)
	    {
	    return htmlSafefAbort(noAbort, -2, "Unexpected error in htmlEscapeAllStrings. s=[%s]", sOrig);
	    }
	}
    else
	{
	// just copy remainder of the input string to output
    	start = strchr(s, 0); // find end of string
	done = TRUE;	
	}
    // move any non-escaped part
    int moveSize = start - s;
    if (moveSize > remainder)
	{
	if (noWarnOverflow) return -1; // speed
	return htmlSafefAbort(noAbort, -1, "Buffer too small in htmlEscapeAllStrings. s=[%s] bufSize = %d", sOrig, bufSize);
	}
    memmove(buffer, s, moveSize);
    buffer += moveSize;
    sz += moveSize;
    remainder -= moveSize;
    if (done)
	{
	if (remainder < 1)
	    {
	    if (noWarnOverflow) return -1; // speed
	    return htmlSafefAbort(noAbort, -1, "Buffer too small for terminating zero in htmlEscapeAllStrings. s=[%s] bufSize = %d", sOrig, bufSize);
	    }
	--remainder;
	*buffer++ = 0;  // terminating 0
	// do not include term 0 in sz count;
	break;
	}
    // escape the quoted part
    s = start + 1;
    *end = 0;  // mark end of "input" string, replacing htmlSafefPunc. input string is temporary anyway.

    int escSize;
    char enc = *(end+1);
    if (enc == (enum htmlSafefEncoding) html)
	{
	escSize = htmlEncodeTextExtended(s,buffer,remainder);
	}
    else if (enc == (enum htmlSafefEncoding) js)
	escSize = javascriptEncodeTextExtended(s,buffer,remainder);
    else if (enc == (enum htmlSafefEncoding) css)
	escSize = cssEncodeTextExtended(s,buffer,remainder);
    else if (enc == (enum htmlSafefEncoding) attr)
	escSize = attrEncodeTextExtended(s,buffer,remainder);
    else if (enc == (enum htmlSafefEncoding) url)
	{
	escSize = urlEncodeTextExtended(s,buffer,remainder);
	}
    else 
	{
	return htmlSafefAbort(noAbort, -2, "Unexpected error in htmlEscapeAllStrings. (enum htmlSafefEncoding)=%c", *(end+1));
	}
    *end = htmlSafefPunc;  // restore mark, helps error message
	
    if (escSize < 0)
	{
	if (noWarnOverflow) return -1; // speed
	return htmlSafefAbort(noAbort, -1, "Buffer too small for escaping in htmlEscapeAllStrings. s=[%s] bufSize = %d", sOrig, bufSize);
	}

    buffer += escSize;
    sz += escSize;
    remainder -= escSize;
    s = end + 2; // skip past htmlSafefPunc and htmlSafefEncoding (encoding type)
    }
return sz;
}

char htmlSpecifierToEncoding(char *format, int *pI, boolean noAbort)
/* translate specifier to encoding type */
{
int i = *pI + 1;
int cnt =  0;
char enc;
char spec[7+1];  // only check for 7 characters after |spec| starts.
spec[0] = 0;
if (format[i] == '|')
    {
    ++i;
    while (TRUE)
	{
	char c = format[i++];
	if ((c == 0) || (cnt >= 7)) // end of format string
	    {
	    i = *pI + 1;
	    spec[0] = 0;
	    break;
	    }
	if (c == '|')
	    {
	    spec[cnt] = 0; // terminate spec
	    if (cnt == 0) // double || escapes itself
		i--;      // retain the last | char
	    break;
	    }
	else
	    {
	    spec[cnt++] = c;
	    }
	}
    }
if (sameString(spec,"js"))
    enc  = (enum htmlSafefEncoding) js;
else if (sameString(spec,"css"))
    enc = (enum htmlSafefEncoding) css;
else if (sameString(spec,"attr"))
    enc = (enum htmlSafefEncoding) attr;
else if (sameString(spec,"url"))
    enc = (enum htmlSafefEncoding) url;
else if (sameString(spec,""))
    enc = (enum htmlSafefEncoding) html;
else if (sameString(spec,"none"))
    enc = (enum htmlSafefEncoding) none;
else
    {
    htmlSafefAbort(noAbort, -2, "Unknown spec [%s] in format string [%s].", spec, format);
    return 0;
    }

*pI = i - 1;
return enc;
}


int vaHtmlSafefNoAbort(char* buffer, int bufSize, char *format, va_list args, boolean noAbort, boolean noWarnOverflow)
/* VarArgs Format string to buffer, vsprintf style, only with buffer overflow
 * checking.  The resulting string is always terminated with zero byte.
 * Automatically escapes string values.
 * Returns count of bytes written or -1 for overflow or -2 for other errors.
 * This function should be efficient on statements with many strings to be escaped. */
{
int formatLen = strlen(format);

char *newFormat = NULL;
int newFormatSize = 3*formatLen + 1;
newFormat = needMem(newFormatSize);
char *nf = newFormat;
char *lastPct = NULL;
int escStringsCount = 0;

char c = 0;
int i = 0;
boolean inPct = FALSE;
while (i < formatLen)
    {
    c = format[i];
    *nf++ = c;
    if (c == '%' && !inPct)
	{
	inPct = TRUE;
	lastPct = nf - 1;  // remember where the start was.
	}
    else if (c == '%' && inPct)
	inPct = FALSE;
    else if (inPct) 
        {
	if (c == 'l')
	    { // used to handle 'l' long
	    }
	else if (strchr("diuoxXeEfFgGpcs",c))
	    {
	    inPct = FALSE;
	    // we finally have the expected format
	    // finally, the string we care about!
	    if (c == 's')
		{
		char enc = htmlSpecifierToEncoding(format, &i, noAbort);
		if (enc == 0)
		    return -2;
		if (enc != (enum htmlSafefEncoding) none) // Not a Pre-escaped String
		    {
		    // go back and insert htmlSafefPunc before the leading % char saved in lastPct
		    // move the accumulated %s descriptor
		    memmove(lastPct+1, lastPct, nf - lastPct); // this is typically very small, src and dest overlap.
		    ++nf;
		    *lastPct = htmlSafefPunc;
		    *nf++ = htmlSafefPunc;
		    *nf++ = enc;
		    ++escStringsCount;
		    }
		}
	    }
	else if (strchr("+-.1234567890",c))
	    {
	    // Do nothing.
	    }
	else
	    {
	    return htmlSafefAbort(noAbort, -2, "String format not understood in vaHtmlSafef: %s", format);
	    }
	}
    ++i;	    
    }

int sz = 0; 
boolean overflow = FALSE;
if (escStringsCount > 0)
    {
    int tempSize = bufSize + 3*escStringsCount;  // allow for temporary escPunc chars + spectype-char
    char *tempBuf = needMem(tempSize);
    sz = vsnprintf(tempBuf, tempSize, newFormat, args);
    /* note that some versions return -1 if too small */
    if (sz != -1 && sz + 1 <= tempSize)
	{
	sz = htmlEscapeAllStrings(buffer, tempBuf, bufSize, noAbort, noWarnOverflow);
	}
    else
	overflow = TRUE;
    freeMem(tempBuf);
    }
else
    {
    sz = vsnprintf(buffer, bufSize, newFormat, args);
    /* note that some version return -1 if too small */
    if ((sz < 0) || (sz >= bufSize))
	overflow = TRUE;
    }
if (overflow)
    {
    buffer[bufSize-1] = (char) 0;
    if (!noWarnOverflow)
	htmlSafefAbort(noAbort, -1, "buffer overflow, size %d, format: %s", bufSize, format);
    sz = -1;
    }

freeMem(newFormat);
va_end(args);

return sz;

}

int htmlSafef(char* buffer, int bufSize, char *format, ...)
/* Format string to buffer, vsprintf style, only with buffer overflow
 * checking.  The resulting string is always terminated with zero byte. 
 * Escapes string parameters. */
{
int sz;
va_list args;
va_start(args, format);
sz = vaHtmlSafefNoAbort(buffer, bufSize, format, args, FALSE, FALSE);
va_end(args);
return sz;
}


void vaHtmlDyStringPrintf(struct dyString *ds, char *format, va_list args)
/* VarArgs Printf append to dyString
 * Strings are escaped according to format type. */
{
/* attempt to format the string in the current space.  If there
 * is not enough room, increase the buffer size and try again */
int avail, sz;
while (TRUE)
    {
    va_list argscp;
    va_copy(argscp, args);
    avail = ds->bufSize - ds->stringSize;
    if (avail <= 0)
        {
        /* Don't pass zero sized buffers to vsnprintf, because who knows
         * if the library function will handle it. */
        dyStringBumpBufSize(ds, ds->bufSize+ds->bufSize);
        avail = ds->bufSize - ds->stringSize;
        }
    sz = vaHtmlSafefNoAbort(ds->string + ds->stringSize, avail, format, argscp, FALSE, TRUE);
    va_end(argscp);
    /* note that some version return -1 if too small */
    if ((sz < 0) || (sz >= avail))
	{
        dyStringBumpBufSize(ds, ds->bufSize+ds->bufSize);
	}
    else
        {
        ds->stringSize += sz;
        break;
        }
    }
}

void htmlDyStringPrintf(struct dyString *ds, char *format, ...)
/* VarArgs Printf append to dyString
 * Strings are escaped according to format type. */
{
va_list args;
va_start(args, format);
vaHtmlDyStringPrintf(ds, format, args);
va_end(args);
}

void vaHtmlFprintf(FILE *f, char *format, va_list args)
/* fprintf using html encoding types */
{
struct dyString *ds = newDyString(1024);
vaHtmlDyStringPrintf(ds, format, args);
fputs(ds->string, f);  // does not append newline
freeDyString(&ds);
}


void htmlFprintf(FILE *f, char *format, ...)
/* fprintf using html encoding types */
{
va_list args;
va_start(args, format);
vaHtmlFprintf(f, format, args);
va_end(args);
}


void htmlPrintf(char *format, ...)
/* fprintf using html encoding types */
{
va_list args;
va_start(args, format);
vaHtmlFprintf(stdout, format, args);
va_end(args);
}


