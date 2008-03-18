/* Routines for getting variables passed in from web page
 * forms via CGI. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "hash.h"
#include "cheapcgi.h"
#include "portable.h"
#include "linefile.h"
#include "errabort.h"
#include "mime.h"
#include <signal.h>

static char const rcsid[] = "$Id: cheapcgi.c,v 1.96 2008/03/18 23:34:08 angie Exp $";

/* These three variables hold the parsed version of cgi variables. */
static char *inputString = NULL;
static unsigned long inputSize;
static struct hash *inputHash = NULL;
static struct cgiVar *inputList = NULL;

static boolean haveCookiesHash = FALSE;
static struct hash *cookieHash = NULL;
static struct cgiVar *cookieList = NULL;

/* should cheapcgi use temp files to store uploaded files */
static boolean doUseTempFile = FALSE;

void dumpCookieList()
/* Print out the cookie list. */
{
struct cgiVar *v;
for (v=cookieList; v != NULL; v = v->next)
    printf("%s=%s (%d)\n", v->name, v->val, v->saved);
}

void useTempFile() 
/* tell cheapcgi to use temp files */
{
doUseTempFile = TRUE;
}

boolean cgiIsOnWeb()
/* Return TRUE if looks like we're being run as a CGI. */
{
return getenv("REQUEST_METHOD") != NULL;
}

char *cgiScriptName()
/* Return name of script so libs can do context-sensitive stuff. */
{
return getenv("SCRIPT_NAME");
}

char *cgiServerName()
/* Return name of server */
{
return getenv("SERVER_NAME");
}

char *cgiRemoteAddr()
/* Return IP address of client (or "unknown"). */
{
static char *dunno = "unknown";
char *remoteAddr = getenv("REMOTE_ADDR");
if (remoteAddr == NULL)
    remoteAddr = dunno;
return remoteAddr;
}

char *_cgiRawInput()
/* For debugging get the unprocessed input. */
{
return inputString;
}

static void getQueryInput()
/* Get query string from environment if they've used GET method. */
{
inputString = getenv("QUERY_STRING");
if (inputString == NULL)
    errAbort("No QUERY_STRING in environment.");
inputString = cloneString(inputString);
}

static void getPostInput()
/* Get input from file if they've used POST method. */
{
char *s;
long i;
int r;

s = getenv("CONTENT_LENGTH");
if (s == NULL)
    errAbort("No CONTENT_LENGTH in environment.");
if (sscanf(s, "%lu", &inputSize) != 1)
    errAbort("CONTENT_LENGTH isn't a number.");
s = getenv("CONTENT_TYPE");
if (s != NULL && startsWith("multipart/form-data", s))
    {
    /* use MIME parse on input stream instead, can handle large uploads */
    inputString = "";  // must not be NULL so it knows it was set
    return;  
    }
inputString = needMem((size_t)inputSize+1);
for (i=0; i<inputSize; ++i)
    {
    r = getc(stdin);
    if (r == EOF)
	errAbort("Short POST input.");
    inputString[i] = r;
    }
inputString[inputSize] = 0;
}

#define memmem(hay, haySize, needle, needleSize) \
    memMatch(needle, needleSize, hay, haySize)

static void cgiParseMultipart(struct hash **retHash, struct cgiVar **retList)
/* process a multipart form */
{
char h[1024];  /* hold mime header line */
char *s = NULL, *ct = NULL;
struct dyString *dy = newDyString(256);
struct mimeBuf *mb = NULL;
struct mimePart *mp = NULL;
char **env = NULL;
struct hash *hash = newHash(6);
struct cgiVar *list = NULL, *el;
extern char **environ;


//debug
//fprintf(stderr,"GALT: top of cgiParseMultipart()\n");
//fflush(stderr);

/* find the CONTENT_ environment strings, use to make Alternate Header string for MIME */
for(env=environ; *env; env++)
    if (startsWith("CONTENT_",*env))
	{
	//debug
    	//fprintf(stderr,"%s\n",*env);  //debug
	safef(h,sizeof(h),"%s",*env);
	s = strchr(h,'_');    /* change env syntax to MIME style header, from _= to -: */
	if (!s)
	    errAbort("expecting '_' parsing env var %s for MIME alt header", *env);
	*s = '-';
	s = strchr(h,'=');
	if (!s)
	    errAbort("expecting '=' parsing env var %s for MIME alt header", *env);
	*s = ':';
	dyStringPrintf(dy,"%s\r\n",h);
	}
dyStringAppend(dy,"\r\n");  /* blank line at end means end of headers */

//debug
//fprintf(stderr,"Alternate Header Text:\n%s",dy->string);
//fflush(stderr);
mb = initMimeBuf(STDIN_FILENO);
//debug
//fprintf(stderr,"got past initMimeBuf(STDIN_FILENO)\n");
//fflush(stderr);
mp = parseMultiParts(mb, cloneString(dy->string)); /* The Alternate Header will get freed */
freeDyString(&dy);
if(!mp->multi) /* expecting multipart child parts */
    errAbort("Malformatted multipart-form.");

//debug
//fprintf(stderr,"GALT: Wow got past parse of MIME!\n");
//fflush(stderr);

ct = hashFindVal(mp->hdr,"content-type");
//debug
//fprintf(stderr,"GALT: main content-type: %s\n",ct);
//fflush(stderr);
if (!startsWith("multipart/form-data",ct))
    errAbort("main content-type expected starts with [multipart/form-data], found [%s]",ct);

for(mp=mp->multi;mp;mp=mp->next)
    {
    char *cd = NULL, *cdMain = NULL, *cdName = NULL, *cdFileName = NULL, *ct = NULL;
    cd = hashFindVal(mp->hdr,"content-disposition");
    ct = hashFindVal(mp->hdr,"content-type");
    //debug
    //fprintf(stderr,"GALT: content-disposition: %s\n",cd);
    //fprintf(stderr,"GALT: content-type: %s\n",ct);
    //fflush(stderr);
    cdMain=getMimeHeaderMainVal(cd);
    cdName=getMimeHeaderFieldVal(cd,"name");
    cdFileName=getMimeHeaderFieldVal(cd,"filename");
    //debug
    //fprintf(stderr,"cgiParseMultipart: main:[%s], name:[%s], filename:[%s]\n",cdMain,cdName,cdFileName);
    //fflush(stderr);
    if (!sameString(cdMain,"form-data"))
	errAbort("main content-type expected [form-data], found [%s]",cdMain);

    //debug
    //fprintf(stderr,"GALT: mp->size[%llu], mp->binary=[%d], mp->fileName=[%s], mp=>data:[%s]\n",
	//(unsigned long long) mp->size, mp->binary, mp->fileName, 
	//mp->binary && mp->data ? "<binary data not safe to print>" : mp->data);
    //fflush(stderr);
    
    /* filename if there is one */
    /* Internet Explorer on Windows is sending full path names, strip
     * directory name from those.  Using \ and / and : as potential
     * path separator characters, e.g.:
     *	 C:\Documents and Settings\tmp\file.txt.gz
     */
    if(cdFileName) 
	{
	char *lastPathSep = strrchr(cdFileName, (int) '\\');
	if (!lastPathSep)
		lastPathSep = strrchr(cdFileName, (int) '/');
	if (!lastPathSep)
		lastPathSep = strrchr(cdFileName, (int) ':');
	char varNameFilename[256];
	safef(varNameFilename, sizeof(varNameFilename), "%s__filename", cdName);
	AllocVar(el);
	if (lastPathSep)
	    el->val = cloneString(lastPathSep+1);
	else
	    el->val = cloneString(cdFileName);
	slAddHead(&list, el);
	hashAddSaveName(hash, varNameFilename, el, &el->name);
    	}
	
    if (mp->data) 
	{
	if (mp->binary)
	    {
	    char varNameBinary[256];
	    char addrSizeBuf[40];
	    safef(varNameBinary,sizeof(varNameBinary),"%s__binary",cdName);
	    safef(addrSizeBuf,sizeof(addrSizeBuf),"%lu %llu", 
		(unsigned long)mp->data,
		(unsigned long long)mp->size);
	    AllocVar(el);
	    el->val = cloneString(addrSizeBuf);
	    slAddHead(&list, el);
	    hashAddSaveName(hash, varNameBinary, el, &el->name);
	    }
	else  /* normal variable, not too big, does not contain zeros */
	    {
	    AllocVar(el);
	    el->val = mp->data;
	    slAddHead(&list, el);
	    hashAddSaveName(hash, cdName, el, &el->name);
	    }
	}	    
    else if (mp->fileName)
	{
	char varNameData[256];
	safef(varNameData, sizeof(varNameData), "%s__data", cdName);
	AllocVar(el);
	el->val = mp->fileName; 
	slAddHead(&list, el);
	hashAddSaveName(hash, varNameData, el, &el->name);
	//debug
    	//fprintf(stderr,"GALT special: saved varNameData:[%s], mp=>fileName:[%s]\n",el->name,el->val);
       	//fflush(stderr);
	}
    else if (mp->multi)
	{
	warn("unexpected nested MIME structures");
	}
    else
	{
	errAbort("mp-> type not data,fileName, or multi - unexpected MIME structure");
	}
    
    freez(&cdMain);
    freez(&cdName);
    freez(&cdFileName);
    }

slReverse(&list);
*retList = list;
*retHash = hash;
}




static void parseCookies(struct hash **retHash, struct cgiVar **retList)
/* parses any cookies and puts them into the given hash and list */
{
char* str;
char *namePt, *dataPt, *nextNamePt;
struct hash *hash;
struct cgiVar *list = NULL, *el;

/* don't build the hash table again */
if(haveCookiesHash == TRUE)
	return;

str = cloneString(getenv("HTTP_COOKIE"));
if(str == NULL) /* don't have a cookie */
	return;

hash = newHash(6);

namePt = str;
while (namePt != NULL)
    {
    dataPt = strchr(namePt, '=');
    if (dataPt == NULL)
	errAbort("Mangled Cookie input string %s", namePt);
    *dataPt++ = 0;
    nextNamePt = strchr(dataPt, ';');
    if (nextNamePt != NULL)
	{
         *nextNamePt++ = 0;
         nextNamePt++;
	}
    cgiDecode(dataPt,dataPt,strlen(dataPt));
    AllocVar(el);
    el->val = dataPt;
    slAddHead(&list, el);
    hashAddSaveName(hash, namePt, el, &el->name);
    namePt = nextNamePt;
    }

haveCookiesHash = TRUE;

slReverse(&list);
*retList = list;
*retHash = hash;
}

char *findCookieData(char *varName)
/* Get the string associated with varName from the cookie string. */
{
struct hashEl *hel;
char *firstResult;

/* make sure that the cookie hash table has been created */
parseCookies(&cookieHash, &cookieList);
if (cookieHash == NULL)
    return NULL;
/* Watch out for multiple cookies with the same name (hel is a list) --
 * warn if we find them. */
hel = hashLookup(cookieHash, varName);
if (hel == NULL)
    return NULL;
else
    firstResult = ((struct cgiVar *)hel->val)->val;
hel = hel->next;
while (hel != NULL)
    {
    char *val = ((struct cgiVar *)(hel->val))->val;
    if (sameString(varName, hel->name) && !sameString(firstResult, val))
	{
	/* This is too early to call warn -- it will mess up html output. */
	fprintf(stderr,
		"findCookieData: Duplicate cookie value from IP=%s: "
		"%s has both %s and %s\n",
		cgiRemoteAddr(),
		varName, firstResult, val);
	}
    hel = hel->next;
    }
return firstResult;
}

static char *cgiInputSource(char *s)
/* For NULL sources make a guess as to real source. */
{
char *qs;
if (s != NULL)
    return s;
qs = getenv("QUERY_STRING");
if (qs == NULL)
    return "POST";
if (qs[0] == 0)
    {
    char *cl = getenv("CONTENT_LENGTH");
    if (cl != NULL && atoi(cl) > 0)
	return "POST";
    }
return "QUERY";
}

static void _cgiFindInput(char *method)
/* Get raw CGI input into inputString.  Method can be "POST", "QUERY", "GET" or NULL
 * for unknown. */
{
if (inputString == NULL)
    {
    method = cgiInputSource(method);
    if (sameWord(method, "POST"))
        getPostInput();
    else if (sameWord(method, "QUERY") || sameWord(method, "GET"))
        getQueryInput();
    else
        errAbort("Unknown form method");
    }
}

static void cgiParseInputAbort(char *input, struct hash **retHash, 
	struct cgiVar **retList)
/* Parse cgi-style input into a hash table and list.  This will alter
 * the input data.  The hash table will contain references back 
 * into input, so please don't free input until you're done with 
 * the hash. Prints message aborts if there's an error.*/
{
char *namePt, *dataPt, *nextNamePt;
struct hash *hash = newHash(6);
struct cgiVar *list = NULL, *el;

namePt = input;
while (namePt != NULL && namePt[0] != 0)
    {
    dataPt = strchr(namePt, '=');
    if (dataPt == NULL)
	{
	errAbort("Mangled CGI input string %s", namePt);
	}
    *dataPt++ = 0;
    nextNamePt = strchr(dataPt, '&');
    if (nextNamePt == NULL)
	nextNamePt = strchr(dataPt, ';');	/* Accomodate DAS. */
    if (nextNamePt != NULL)
         *nextNamePt++ = 0;
    cgiDecode(namePt,namePt,strlen(namePt));	/* for unusual ct names */
    cgiDecode(dataPt,dataPt,strlen(dataPt));
    AllocVar(el);
    el->val = dataPt;
    slAddHead(&list, el);
    hashAddSaveName(hash, namePt, el, &el->name);
    namePt = nextNamePt;
    }
slReverse(&list);
*retList = list;
*retHash = hash;
}

static jmp_buf cgiParseRecover;

static void cgiParseAbort()
/* Abort cgi parsing. */
{
longjmp(cgiParseRecover, -1);
}

boolean cgiParseInput(char *input, struct hash **retHash, 
	struct cgiVar **retList)
/* Parse cgi-style input into a hash table and list.  This will alter
 * the input data.  The hash table will contain references back 
 * into input, so please don't free input until you're done with 
 * the hash. Prints message and returns FALSE if there's an error.*/
{
boolean ok = TRUE;
int status = setjmp(cgiParseRecover);
if (status == 0)    /* Always true except after long jump. */
    {
    pushAbortHandler(cgiParseAbort);
    cgiParseInputAbort(input, retHash, retList);
    }
else    /* They long jumped here because of an error. */
    {
    ok = FALSE;
    }
popAbortHandler();
return ok;
}



static void catchSignal(int sigNum)
/* handler for various terminal signals for logging purposes */
{
char *sig = NULL;
switch (sigNum)
    {
    case SIGABRT:
      sig = "SIGABRT";
      break;
    case SIGSEGV:
      sig = "SIGSEGV";
      break;
    case SIGFPE:
      sig = "SIGFPE";
      break;
    case SIGBUS:
      sig = "SIGBUS";
      break;
    }
    logCgiToStderr();
    fprintf(stderr, "Received signal %s\n", sig);
    raise(SIGKILL); 
}

void initSigHandlers()
/* set handler for various terminal signals for logging purposes */
{
if (cgiIsOnWeb())
    {
    signal(SIGABRT, catchSignal);
    signal(SIGSEGV, catchSignal);
    signal(SIGFPE, catchSignal);
    signal(SIGBUS, catchSignal);
    }
}



static void initCgiInput() 
/* Initialize CGI input stuff.  After this CGI vars are
 * stored in an internal hash/list regardless of how they
 * were passed to the program. */
{
char* s;

if (inputString != NULL)
    return;

_cgiFindInput(NULL);

/* check to see if the input is a multipart form */
s = getenv("CONTENT_TYPE");
if (s != NULL && startsWith("multipart/form-data", s))
    {
    cgiParseMultipart(&inputHash, &inputList);
    }	    
else
    {
    cgiParseInputAbort(inputString, &inputHash, &inputList);
    }

/* now parse the cookies */
parseCookies(&cookieHash, &cookieList);

/* Set enviroment variables CGIs to enable sql tracing and/or profiling */
s = cgiOptionalString("JKSQL_TRACE");
if (s != NULL)
    envUpdate("JKSQL_TRACE", s);
s = cgiOptionalString("JKSQL_PROF");
if (s != NULL)
    envUpdate("JKSQL_PROF", s);
    
}

struct cgiVar *cgiVarList() 
/* return the list of cgiVar's */
{
initCgiInput();
return inputList;
}

static char *findVarData(char *varName)
/* Get the string associated with varName from the query string. */
{
struct cgiVar *var;

initCgiInput();
if ((var = hashFindVal(inputHash, varName)) == NULL)
    return NULL;
return var->val;
}

void cgiBadVar(char *varName)
/* Complain about a variable that's not there. */
{
if (varName == NULL) varName = "";
errAbort("Sorry, didn't find input variable %s\n"
        "Probably the web page didn't mean to call this program.", 
        varName);
}

static char *mustFindVarData(char *varName)
/* Find variable and associated data or die trying. */
{
char *res = findVarData(varName);
if (res == NULL)
    cgiBadVar(varName);
return res;
}

void cgiDecode(char *in, char *out, int inLength)
/* Decode from cgi pluses-for-spaces format to normal. 
 * Out will be a little shorter than in typically, and
 * can be the same buffer. */
{
char c;
int i;
for (i=0; i<inLength;++i)
    {
    c = *in++;
    if (c == '+') 
	*out++ = ' ';
    else if (c == '%')
	{
	int code;
	if (sscanf(in, "%2x", &code) != 1) 
	    code = '?';
	in += 2;
	i += 2;
	*out++ = code;
	}
    else
	*out++ = c;
    }
*out++ = 0;
}

char *cgiEncode(char *inString)
/* Return a cgi-encoded version of inString. 
 * Alphanumerics kept as is, space translated to plus,
 * and all other characters translated to %hexVal. */
{
char c;
int outSize = 0;
char *outString, *out, *in;

if (inString == NULL)
    return(cloneString(""));

/* Count up how long it will be */
in = inString;
while ((c = *in++) != 0)
    {
    if (isalnum(c) || c == ' ' || c == '.' || c == '_')
        outSize += 1;
    else
        outSize += 3;
    }
outString = needMem(outSize+1);

/* Encode string */
in = inString;
out = outString;
while ((c = *in++) != 0)
    {
    if (isalnum(c) || c == '.' || c == '_')
        *out++ = c;
    else if (c == ' ')
        *out++ = '+';
    else
        {
        unsigned char uc = c;
        char buf[4];
        *out++ = '%';
        safef(buf, sizeof(buf), "%02X", uc);
        *out++ = buf[0];
        *out++ = buf[1];
        }
    }
*out++ = 0;
return outString;
}

char *cgiEncodeFull(char *inString)
/* Return a cgi-encoded version of inString (no + for space!). 
 * Alphanumerics/./_ kept as is and all other characters translated to 
 * %hexVal. */
{
char c;
int outSize = 0;
char *outString, *out, *in;

if (inString == NULL)
    return(cloneString(""));

/* Count up how long it will be */
in = inString;
while ((c = *in++) != 0)
    {
    if (isalnum(c) || c == '.' || c == '_')
        outSize += 1;
    else
        outSize += 3;
    }
outString = needMem(outSize+1);

/* Encode string */
in = inString;
out = outString;
while ((c = *in++) != 0)
    {
    if (isalnum(c) || c == '.' || c == '_')
        *out++ = c;
    else
        {
        unsigned char uc = c;
        char buf[4];
        *out++ = '%';
        safef(buf, sizeof(buf), "%02X", uc);
        *out++ = buf[0];
        *out++ = buf[1];
        }
    }
*out++ = 0;
return outString;
}

char *cgiOptionalString(char *varName)
/* Return value of string if it exists in cgi environment, else NULL */
{
return findVarData(varName);
}


char *cgiString(char *varName)
/* Return string value of cgi variable. */
{
return mustFindVarData(varName);
}

char *cgiUsualString(char *varName, char *usual)
/* Return value of string if it exists in cgi environment.  
 * Otherwise return 'usual' */
{
char *pt;
pt = findVarData(varName);
if (pt == NULL)
    pt = usual;
return pt;
}

struct slName *cgiStringList(char *varName)
/* Find list of cgi variables with given name.  This
 * may be empty.  Free result with slFreeList(). */
{
struct hashEl *hel;
struct slName *stringList = NULL, *string;

initCgiInput();
for (hel = hashLookup(inputHash, varName); hel != NULL; hel = hel->next)
    {
    if (sameString(hel->name, varName))
        {
	struct cgiVar *var = hel->val;
	string = newSlName(var->val);
	slAddHead(&stringList, string);
	}
    }
return stringList;
}


int cgiInt(char *varName)
/* Return int value of cgi variable. */
{
char *data;
char c;

data = mustFindVarData(varName);
data = skipLeadingSpaces(data);
c = data[0];
if (!(isdigit(c) || (c == '-' && isdigit(data[1]))))
     errAbort("Expecting number in %s, got \"%s\"\n", varName, data);
return atoi(data);
}

int cgiIntExp(char *varName)
/* Evaluate an integer expression in varName and
 * return value. */
{
return intExp(cgiString(varName));
}

int cgiOptionalInt(char *varName, int defaultVal)
/* This returns integer value of varName if it exists in cgi environment
 * and it's not just the empty string otherwise it returns defaultVal. */
{
char *s = cgiOptionalString(varName);
s = skipLeadingSpaces(s);
if (isEmpty(s))
    return defaultVal;
return cgiInt(varName);
}

double cgiDouble(char *varName)
/* Returns double value. */
{
char *data;
double x;

data = mustFindVarData(varName);
if (sscanf(data, "%lf", &x)<1) 
     errAbort("Expecting real number in %s, got \"%s\"\n", varName, data);
return x;
}

double cgiOptionalDouble(char *varName, double defaultVal)
/* Returns double value. */
{
if (!cgiVarExists(varName))
    return defaultVal;
return cgiDouble(varName);
}

boolean cgiVarExists(char *varName)
/* Returns TRUE if the variable was passed in. */
{
initCgiInput();
return hashLookup(inputHash, varName) != NULL;
}

boolean cgiBoolean(char *varName)
{
return cgiVarExists(varName);
}

int cgiOneChoice(char *varName, struct cgiChoice *choices, int choiceSize)
/* Returns value associated with string variable in choice table. */
{
char *key = cgiString(varName);
int i;
int val = -1;

for (i=0; i<choiceSize; ++i)
    {
    if (sameWord(choices[i].name, key))
        {
        val =  choices[i].value;
        return val;
        }
    }
errAbort("Unknown key %s for variable %s\n", key, varName);
return val;
}

void cgiMakeSubmitButton()
/* Make 'submit' type button. */
{
cgiMakeButton("Submit", "Submit");
}

void cgiMakeResetButton()
/* Make 'reset' type button. */
{
printf("<INPUT TYPE=RESET NAME=\"Reset\" VALUE=\" Reset \">");
}

void cgiMakeClearButton(char *form, char *field)
/* Make button to clear a text field. */
{
char javascript[1024];

safef(javascript, sizeof(javascript), 
    "document.%s.%s.value = ''; document.%s.submit();", form, field, form);
cgiMakeOnClickButton(javascript, " Clear  ");
}

void cgiMakeButtonWithMsg(char *name, char *value, char *msg)
/* Make 'submit' type button. Display msg on mouseover, if present*/
{
printf("<INPUT TYPE=SUBMIT NAME=\"%s\" VALUE=\"%s\" %s%s%s>", 
        name, value, 
        (msg ? " TITLE=\"" : ""), (msg ? msg : ""), (msg ? "\"" : "" ));
}

void cgiMakeButton(char *name, char *value)
/* Make 'submit' type button */
{
cgiMakeButtonWithMsg(name, value, NULL);
}

void cgiMakeOnClickButton(char *command, char *value)
/* Make 'push' type button with client side onClick (java)script. */
{
printf("<INPUT TYPE=\"button\" VALUE=\"%s\" onClick=\"%s\">", value, command);
}

void cgiMakeOnClickSubmitButton(char *command, char *name, char *value)
/* Make submit button with both variable name and value with client side
 * onClick (java)script. */
{
printf("<INPUT TYPE=SUBMIT NAME=\"%s\" VALUE=\"%s\" onClick=\"%s\">",
       name, value, command);
}

void cgiMakeOptionalButton(char *name, char *value, boolean disabled)
/* Make 'submit' type button that can be disabled. */
{
printf("<INPUT TYPE=SUBMIT NAME=\"%s\" VALUE=\"%s\"", name, value);
if (disabled)
    printf(" DISABLED");
printf(">"); 
}

void cgiMakeFileEntry(char *name)
/* Make file entry box/browser */
{
    printf("<INPUT TYPE=FILE NAME=\"%s\">", name);
}

void cgiSimpleTableStart()
/* start HTML table  -- no customization. Leaves room
 * for a fancier implementation */
{
printf("<TABLE>\n");
}

void cgiTableEnd()
/* end HTML table */
{
printf("</TABLE>\n");
}

void cgiSimpleTableRowStart()
/* Start table row */
{
printf("<TR>\n");
}

void cgiTableRowEnd()
/* End table row */
{
printf("</TR>\n");
}

void cgiSimpleTableFieldStart()
/* Start table field */
{
printf("<TD>");
}

void cgiTableFieldStartAlignRight()
/* Start table field and align right*/
{
printf("<TD ALIGN = RIGHT>");
}

void cgiTableFieldEnd()
/* End table field */
{
printf("</TD>\n");
}

void cgiTableField(char *text)
/* Make table field entry */
{
printf("<TD> %s </TD>\n", text);
}

void cgiTableFieldWithMsg(char *text, char *msg)
/* Make table field entry with mouseover */
{
printf("<TD %s%s%s> %s </TD>\n", 
        (msg ? " TITLE=\"" : ""), (msg ? msg : ""), (msg ? "\"" : "" ),
        text);
}

void cgiParagraph(char *text)
/* Make text paragraph */
{
printf("<P> %s\n", text);
}

void cgiMakeRadioButton(char *name, char *value, boolean checked)
/* Make radio type button.  A group of radio buttons should have the
 * same name but different values.   The default selection should be
 * sent with checked on. */
{
printf("<INPUT TYPE=RADIO NAME=\"%s\" VALUE=\"%s\" %s>", name, value,
   (checked ? "CHECKED" : ""));
}

void cgiMakeOnClickRadioButton(char *name, char *value, boolean checked,
                                        char *command)
/* Make radio type button with onClick command.
 *  A group of radio buttons should have the
 * same name but different values.   The default selection should be
 * sent with checked on. */
{
printf("<INPUT TYPE=RADIO NAME=\"%s\" VALUE=\"%s\" %s %s>",
        name, value, command, (checked ? "CHECKED" : ""));
}

char *cgiBooleanShadowPrefix()
/* Prefix for shadow variable set with boolean variables. */
{
return "boolshad.";
}

boolean cgiBooleanDefined(char *name)
/* Return TRUE if boolean variable is defined (by
 * checking for shadow. */
{
char buf[256];
safef(buf, sizeof(buf), "%s%s", cgiBooleanShadowPrefix(), name);
return cgiVarExists(buf);
}

void cgiMakeCheckBoxWithMsg(char *name, boolean checked, char *msg)
/* Make check box. Also make a shadow hidden variable so we
 * can distinguish between variable not present and
 * variable set to false. Use msg as mousever if present */
{
char buf[256];

printf("<INPUT TYPE=CHECKBOX NAME=\"%s\" VALUE=on %s%s%s %s>", name,
    (msg ? " TITLE=\"" : ""), (msg ? msg : ""), (msg ? "\"" : "" ), 
    (checked ? " CHECKED" : ""));
safef(buf, sizeof(buf), "%s%s", cgiBooleanShadowPrefix(), name);
cgiMakeHiddenVar(buf, "1");
}

void cgiMakeCheckBox(char *name, boolean checked)
/* Make check box. */
{
cgiMakeCheckBoxWithMsg(name, checked, NULL);
}

void cgiMakeCheckBoxJS(char *name, boolean checked, char *javascript)
/* Make check box with javascript. */
{
char buf[256];

printf("<INPUT TYPE=CHECKBOX NAME=\"%s\" VALUE=on %s %s>", name,
    javascript, (checked ? " CHECKED" : ""));
safef(buf, sizeof(buf), "%s%s", cgiBooleanShadowPrefix(), name);
cgiMakeHiddenVar(buf, "1");
}

void cgiMakeHiddenBoolean(char *name, boolean on)
/* Make hidden boolean variable. Also make a shadow hidden variable so we
 * can distinguish between variable not present and
 * variable set to false. */
{
char buf[256];
cgiMakeHiddenVar(name, on ? "on" : "off");
safef(buf, sizeof(buf), "%s%s", cgiBooleanShadowPrefix(), name);
cgiMakeHiddenVar(buf, "1");
}

void cgiMakeTextArea(char *varName, char *initialVal, int rowCount, int columnCount)
/* Make a text area with area rowCount X columnCount and with text: intialVal */
{
cgiMakeTextAreaDisableable(varName, initialVal, rowCount, columnCount, FALSE);
}

void cgiMakeTextAreaDisableable(char *varName, char *initialVal, int rowCount, int columnCount, boolean disabled)
/* Make a text area that can be disabled. The rea has rowCount X 
 * columnCount and with text: intialVal */
{
printf("<TEXTAREA NAME=\"%s\" ROWS=%d COLS=%d %s>%s</TEXTAREA>", varName,
       rowCount, columnCount, disabled ? "DISABLED" : "",
       (initialVal != NULL ? initialVal : ""));
}

void cgiMakeTextVar(char *varName, char *initialVal, int charSize)
/* Make a text control filled with initial value.  If charSize
 * is zero it's calculated from initialVal size. */
{
if (initialVal == NULL)
    initialVal = "";
if (charSize == 0) charSize = strlen(initialVal);
if (charSize == 0) charSize = 8;

printf("<INPUT TYPE=TEXT NAME=\"%s\" SIZE=%d VALUE=\"%s\">", varName, 
	charSize, initialVal);
}

void cgiMakeIntVar(char *varName, int initialVal, int maxDigits)
/* Make a text control filled with initial value.  */
{
if (maxDigits == 0) maxDigits = 4;

printf("<INPUT TYPE=TEXT NAME=\"%s\" SIZE=%d VALUE=%d>", varName, 
	maxDigits, initialVal);
}

void cgiMakeDoubleVar(char *varName, double initialVal, int maxDigits)
/* Make a text control filled with initial floating-point value.  */
{
if (maxDigits == 0) maxDigits = 4;

printf("<INPUT TYPE=TEXT NAME=\"%s\" SIZE=%d VALUE=%g>", varName, 
	maxDigits, initialVal);
}

void cgiMakeDropListClassWithStyle(char *name, char *menu[], 
	int menuSize, char *checked, char *class, char *style)
/* Make a drop-down list with names, text class and style. */
{
int i;
char *selString;
if (checked == NULL) checked = menu[0];
if (style)
    printf("<SELECT NAME=\"%s\" class=%s style=\"%s\">\n", name, class, style);
else
    printf("<SELECT NAME=\"%s\" class=%s>\n", name, class);
for (i=0; i<menuSize; ++i)
    {
    if (sameWord(menu[i], checked))
        selString = " SELECTED";
    else
        selString = "";
    printf("<OPTION%s>%s</OPTION>\n", selString, menu[i]);
    }
printf("</SELECT>\n");
}

void cgiMakeDropListClass(char *name, char *menu[], 
	int menuSize, char *checked, char *class)
/* Make a drop-down list with names. */
{
    cgiMakeDropListClassWithStyle(name, menu, menuSize, checked, 
                                        class, NULL);
}

void cgiMakeDropList(char *name, char *menu[], int menuSize, char *checked)
/* Make a drop-down list with names. 
 * uses style "normalText" */
{
    cgiMakeDropListClass(name, menu, menuSize, checked, "normalText");
}

void cgiMakeMultList(char *name, char *menu[], int menuSize, char *checked, int length)
/* Make a list of names with window height equalt to length,
 * which can have multiple selections. Same as drop-down list 
 * except "multiple" is added to select tag. */
{
int i;
char *selString;
if (checked == NULL) checked = menu[0];
printf("<SELECT MULTIPLE SIZE=%d ALIGN=CENTER NAME=\"%s\">\n", length, name);
for (i=0; i<menuSize; ++i)
    {
    if (sameWord(menu[i], checked))
        selString = " SELECTED";
    else
        selString = "";
    printf("<OPTION%s>%s</OPTION>\n", selString, menu[i]);
    }
printf("</SELECT>\n");
}

void cgiMakeDropListFull(char *name, char *menu[], char *values[], 
                         int menuSize, char *checked, char *extraAttribs)
/* Make a drop-down list with names and values. */
{
int i;
char *selString;
if (checked == NULL) checked = menu[0];

if (NULL != extraAttribs)
    {
    printf("<SELECT NAME=\"%s\" %s>\n", name, extraAttribs);
    }
else
    {
    printf("<SELECT NAME=\"%s\">\n", name);
    }

for (i=0; i<menuSize; ++i)
    {
    if (sameWord(values[i], checked))
        selString = " SELECTED";
    else
        selString = "";
    printf("<OPTION%s VALUE=\"%s\">%s</OPTION>\n", selString, values[i], menu[i]);
    }
printf("</SELECT>\n");
}

void cgiMakeDropListWithVals(char *name, char *menu[], char *values[], 
                         int menuSize, char *checked)
/* Make a drop-down list with names and values. In this case checked
 * corresponds to a value, not a menu. */
{
int i;
char *selString;
if (checked == NULL) checked = values[0];

printf("<SELECT NAME=\"%s\">\n", name);
for (i=0; i<menuSize; ++i)
    {
    if (sameWord(values[i], checked))
        selString = " SELECTED";
    else
        selString = "";
    printf("<OPTION%s VALUE=\"%s\">%s</OPTION>\n", selString, values[i], menu[i]);
    }
printf("</SELECT>\n");
}

void cgiMakeHiddenVar(char *varName, char *string)
/* Store string in hidden input for next time around. */
{
printf("<INPUT TYPE=HIDDEN NAME=\"%s\" VALUE=\"%s\">", varName, string);
}

void cgiContinueHiddenVar(char *varName)
/* Write CGI var back to hidden input for next time around. */
{
if (cgiVarExists(varName))
    cgiMakeHiddenVar(varName, cgiString(varName));
}

void cgiVarExclude(char *varName)
/* If varName exists, remove it. */
{
if (cgiVarExists(varName))
    {
    struct cgiVar *cv = hashRemove(inputHash, varName);
    slRemoveEl(&inputList, cv);
    }
}

void cgiVarExcludeExcept(char **varNames)
/* Exclude all variables except for those in NULL
 * terminated array varNames.  varNames may be NULL
 * in which case nothing is excluded. */
{
struct hashEl *list, *el;
struct hash *exclude = newHash(8);
char *s;

/* Build up hash of things to exclude */
if (varNames != NULL)
   {
   while ((s = *varNames++) != NULL)
       hashAdd(exclude, s, NULL);
   }

/* Step through variable list and remove them if not
 * excluded. */
initCgiInput();
list = hashElListHash(inputHash);
for (el = list; el != NULL; el = el->next)
    {
    if (!hashLookup(exclude, el->name))
        cgiVarExclude(el->name);
    }
hashElFreeList(&list);
freeHash(&exclude);
}

void cgiVarSet(char *varName, char *val)
/* Set a cgi variable to a particular value. */
{
struct cgiVar *var;
initCgiInput();
AllocVar(var);
var->val = cloneString(val);
hashAddSaveName(inputHash, varName, var, &var->name);
}

struct dyString *cgiUrlString()
/* Get URL-formatted that expresses current CGI variable state. */
{
struct dyString *dy = newDyString(0);
struct cgiVar *cv;
char *e;


for (cv = inputList; cv != NULL; cv = cv->next)
    {
    if (cv != inputList)
       dyStringAppend(dy, "&");
    e = cgiEncode(cv->val);
    dyStringPrintf(dy, "%s=", cv->name);
    dyStringAppend(dy, e);
    freez(&e);
    }
return dy;
}

void cgiContinueAllVars()
/* Write back all CGI vars as hidden input for next time around. */
{
struct cgiVar *cv;
for (cv = inputList; cv != NULL; cv = cv->next)
    cgiMakeHiddenVar(cv->name, cv->val);
}


boolean cgiFromCommandLine(int *pArgc, char *argv[], boolean preferWeb)
/* Use the command line to set up things as if we were a CGI program. 
 * User types in command line (assuming your program called cgiScript) 
 * like:
 *        cgiScript nonCgiArg1 var1=value1 var2=value2 var3=value3 nonCgiArg2
 * or like
 *        cgiScript nonCgiArg1 var1=value1&var2=value2&var3=value3 nonCgiArg2
 * or even like
 *        cgiScript nonCgiArg1 -x -y=bogus z=really
 * (The non-cgi arguments can occur anywhere.  The cgi arguments (all containing
 * the character '=' or starting with '-') are erased from argc/argv.  Normally 
 * you call this cgiSpoof(&argc, argv);
 */
{
int argc = *pArgc;
int i;
int argcLeft = argc;
char *name;
static char queryString[4096];
char *q = queryString;
boolean needAnd = TRUE;
boolean gotAny = FALSE;
boolean startDash;
boolean gotEq;
static char hostLine[512];

if (preferWeb && cgiIsOnWeb())
    return TRUE;	/* No spoofing required! */
q += safef(q, queryString + sizeof(queryString) - q,
	   "%s", "QUERY_STRING=cgiSpoof=on");
for (i=0; i<argcLeft; )
    {
    name = argv[i];
    if ((startDash = (name[0] == '-')))
       ++name;
    gotEq = (strchr(name, '=') != NULL);
    if (gotEq || startDash)
        {
        if (needAnd)
            *q++ = '&';
        q += safef(q, queryString + sizeof(queryString) - q, "%s", name);
        if (!gotEq || strchr(name, '&') == NULL)
            needAnd = TRUE;
	if (!gotEq)
	    q += safef(q, queryString + sizeof(queryString) - q, "=on");
        memcpy(&argv[i], &argv[i+1], sizeof(argv[i]) * (argcLeft-i-1));
        argcLeft -= 1;
        gotAny = TRUE;
        }
    else
        i++;
    }
if (gotAny)
    {
    *pArgc = argcLeft;
    }
putenv("REQUEST_METHOD=GET");
putenv(queryString);
safef(hostLine, sizeof(hostLine), "SERVER_NAME=%s", getenv("HOST"));
putenv(hostLine);
initCgiInput();
return gotAny;
}

boolean cgiSpoof(int *pArgc, char *argv[])
/* If run from web line set up input
 * variables from web line, otherwise
 * set up from command line. */
{
return cgiFromCommandLine(pArgc, argv, TRUE);
}

boolean cgiFromFile(char *fileName)
/* Set up a cgi environment using parameters stored in a file.
 * Takes file with arguments in the form:
 *       argument1=someVal
 *       # This is a comment
 *       argument2=someOtherVal
 *       ...
 * and puts them into the cgi environment so that the usual
 * cgiGetVar() commands can be used. Useful when a program 
 * has a lot of possible parameters.
 */
{
char **argv = NULL;
int argc = 0; 
int maxArgc = 10;
int i;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line;
boolean spoof= FALSE;
AllocArray(argv, maxArgc);
/* Remember that first arg is program name.
   Put filename there instead. */
argc = 1; 
argv[0] = cloneString(fileName);
for(;;)
    {
    /* If we are at the end we're done. */
    if(!lineFileNext(lf, &line, NULL))
	break;
    /* If it is a comment or blank line skip it. */
    if (line[0] == '#' || sameString(line, ""))
        continue;
    /* If our argv array is full expand it. */
    if((argc+1) >= maxArgc)
	{
	ExpandArray(argv, maxArgc, 2*maxArgc);
	maxArgc *= 2;
	}
    /* Fill in another argument to our psuedo arguments. */
    argv[argc++] = cloneString(line);
    }
spoof = cgiSpoof(&argc, argv);
/* Cleanup. */
lineFileClose(&lf);
for(i=0; i<argc; i++)
    freez(&argv[i]);
freez(&argv);
return spoof;
}

void logCgiToStderr()
/* Log useful CGI info to stderr */
{
char *ip = getenv("REMOTE_ADDR");
char *cgiBinary = getenv("SCRIPT_FILENAME");
char *requestUri = getenv("REQUEST_URI");
char *hgsid = cgiOptionalString("hgsid");
char *cgiFileName = NULL;
time_t nowTime = time(NULL);
struct tm *tm;
tm = localtime(&nowTime);
char *ascTime = asctime(tm);
size_t len = strlen(ascTime);
if (cgiBinary)
    cgiFileName = basename(cgiBinary);
else
    cgiFileName = "cgi-bin";
if (len > 3) ascTime[len-2] = '\0';
if (!ip)
    ip = "unknown";
if (!hgsid)
    hgsid = "unknown";
if (!requestUri)
    requestUri = "unknown";
fprintf(stderr, "[%s] [%s] [client %s] [hgsid=%.24s] [%.1024s] ", ascTime, cgiFileName, ip,
	hgsid, requestUri);
}


