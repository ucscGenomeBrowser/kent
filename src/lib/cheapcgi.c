/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* Routines for getting variables passed in from web page
 * forms via CGI. */

#include "common.h"
#include "hash.h"
#include "cheapcgi.h"

struct cgiVar
/* Info on one cgi variable. */
    {
    struct cgiVar *next;	/* Next in list. */
    char *name;			/* Name - allocated in hash. */
    char *val;  		/* Value - also not allocated here. */
    boolean saved;		/* True if saved. */
    };

/* These three variables hold the parsed version of cgi variables. */
static char *inputString = NULL;
static struct hash *inputHash = NULL;
static struct cgiVar *inputList = NULL;

boolean cgiIsOnWeb()
/* Return TRUE if looks like we're being run as a CGI. */
{
return getenv("REQUEST_METHOD") != NULL;
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
}

static void getPostInput()
/* Get input from file if they've used POST method. */
{
long inputSize;
char *s;
long i;
int r;

s = getenv("CONTENT_LENGTH");
if (s == NULL)
    errAbort("No CONTENT_LENGTH in environment.");
if (sscanf(s, "%lu", &inputSize) != 1)
    errAbort("CONTENT_LENGTH isn't a number.");
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

static char *cgiInputSource(char *s)
/* For NULL sources make a guess as to real source. */
{
char *qs;
if (s != NULL)
    return s;
qs = getenv("QUERY_STRING");
if (qs == NULL || qs[0] == 0)
    return "POST";
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


void *cgiParseInput(char *input, struct hash **retHash, struct cgiVar **retList)
/* Parse cgi-style input into a hash table and list.  This will alter
 * the input data.  The hash table will contain references back 
 * into input, so please don't free input until you're done with 
 * the hash. */
{
char *namePt, *dataPt, *nextNamePt;
struct hash *hash = newHash(6);
struct hashEl *hel;
struct cgiVar *list = NULL, *el;

namePt = input;
while (namePt != NULL)
    {
    dataPt = strchr(namePt, '=');
    if (dataPt == NULL)
	errAbort("Mangled CGI input string %s", namePt);
    *dataPt++ = 0;
    nextNamePt = strchr(dataPt, '&');
    if (nextNamePt != NULL)
         *nextNamePt++ = 0;
    cgiDecode(dataPt,dataPt,strlen(dataPt));
    AllocVar(el);
    hel = hashAdd(hash, namePt, el);
    el->name = hel->name;
    el->val = dataPt;
    slAddHead(&list, el);
    namePt = nextNamePt;
    }
slReverse(&list);
*retList = list;
*retHash = hash;
}

static void initCgiInput()
/* Initialize CGI input stuff. */
{
_cgiFindInput(NULL);
cgiParseInput(inputString, &inputHash, &inputList);
}

static char *findVarData(char *varName)
/* Get the string associated with varName from the query string. */
{
struct cgiVar *var;

if (inputString == NULL)
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

/* Count up how long it will be */
in = inString;
while ((c = *in++) != 0)
    {
    if (isalnum(c) || c == ' ' || c == '.')
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
    if (isalnum(c) || c == '.')
        *out++ = c;
    else if (c == ' ')
        *out++ = '+';
    else
        {
        unsigned char uc = c;
        char buf[4];
        *out++ = '%';
        sprintf(buf, "%02X", uc);
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

int cgiOptionalInt(char *varName, int defaultVal)
/* This returns integer value of varName if it exists in cgi environment
 * and it's not just the empty string otherwise it returns defaultVal. */
{
char *s;
if (!cgiVarExists(varName))
    return defaultVal;
return cgiInt(varName);
}

double cgiDouble(char *varName)
{
char *data;
double x;

data = mustFindVarData(varName);
if (sscanf(data, "%lf", &x)<1) 
     errAbort("Expecting real number in %s, got \"%s\"\n", varName, data);
return x;
}

boolean cgiVarExists(char *varName)
/* Returns TRUE if the variable was passed in. */
{
if (inputString == NULL)
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
    if (!differentWord(choices[i].name, key))
        {
        val =  choices[i].value;
        return val;
        }
    }
errAbort("Unknown key %s for variable %s\n", key, varName);
return val;
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

boolean cgiSpoof(int *pArgc, char *argv[])
/* Use the command line to set up things as if we were a CGI program. 
 * User types in command line (assuming your program called cgiScript) 
 * like:
 *        cgiScript nonCgiArg1 var1=value1 var2=value2 var3=value3 nonCgiArg2
 * or like
 *        cgiScript nonCgiArg1 var1=value1&var2=value2&var3=value3 nonCgiArg2
 * (The non-cgi arguments can occur anywhere.  The cgi arguments (all containing
 * the character '=') are erased from argc/argv.  Normally you call this
 *        cgiSpoof(&argc, argv);
 */
{
int argc = *pArgc;
int i;
int argcLeft = argc;
char *name;
static char queryString[2046];
char *q = queryString;
boolean needAnd = FALSE;
boolean gotAny = FALSE;

q += sprintf(q, "%s", "QUERY_STRING=");
for (i=0; i<argcLeft; )
    {
    name = argv[i];
    if (strchr(name,'=') != NULL)
        {
        if (needAnd)
            *q++ = '&';
        q += sprintf(q, "%s", name);
        if (strchr(name, '&') == NULL)
            needAnd = TRUE;
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
initCgiInput();
return gotAny;
}
