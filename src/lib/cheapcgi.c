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
#include "portable.h"


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

#define memmem(hay, haySize, needle, needleSize) \
    memMatch(needle, needleSize, hay, haySize)

static void cgiParseMultipart(char *input, struct hash **retHash, struct cgiVar **retList)
/* process a multipart form */
{
char* s;
char* boundary;
char *namePt, *nameEndPt, *dataPt, *dataEndPt, *filenamePt = 0, *filenameEndPt;
struct hash *hash = newHash(6);
struct cgiVar *list = NULL, *el;

char* inputEnd = input + inputSize;

/* find the boundary string */
s = getenv("CONTENT_TYPE");
s = strstr(s, "boundary=");
if(s == NULL) 
    errAbort("Malformatted multipart-from.");

/* skip the "boundary=" */
s += 9;
/* allocate enough room plus "--" and '\0' */
boundary = needMem(strlen(s) + 2 + 1);
/* setup the string */
boundary[0] = '-';
boundary[1] = '-';
strcpy(boundary + 2, s);
		
namePt = input;
while(namePt != 0) 
    {
    filenamePt = 0;
    /* find the boundary */
    namePt = memmem(namePt, inputEnd - namePt, boundary, strlen(boundary));
    if(namePt != 0) 
	{
	/* skip passed the boundary */
	namePt += strlen(boundary);	

	/* if we have -, we are done */
	if(*namePt == '-')
	    break;

	/* find the name */
	namePt = memmem(namePt, inputEnd - namePt, "name=\"", strlen("name=\""));
	if(namePt == 0)
	    errAbort("Mangled CGI input (0) string %s", input);
	namePt += strlen("name=\"");

	/* find the end of the name */
	nameEndPt = memmem(namePt, inputEnd - namePt, "\"", strlen("\""));
	if (nameEndPt == 0)
	    errAbort("Mangled CGI input (1) string %s", input);
	*nameEndPt = 0;

	/* find the data */
	dataPt = memmem(nameEndPt + 1, inputEnd - nameEndPt - 1, "\n\r\n", strlen("\n\r\n"));
	if (dataPt == 0)
	    errAbort("Mangled CGI input (2) string %s", input);
	dataPt += 3;

	/* find the end of the data */
	dataEndPt = memmem(dataPt, inputEnd - dataPt, boundary, strlen(boundary));
	if (dataEndPt == 0)
	    errAbort("Mangled CGI input (3) string %s", input);
	dataEndPt -= 2;

	*(dataEndPt) = '\0';

	/* find the filename if there is one */
	if(*(nameEndPt + 1) == ';' && *(nameEndPt + 2) == ' ' && *(nameEndPt + 3) == 'f') {
	    char varNameFilename[256];
	    struct cgiVar *filenameEl;

	    filenamePt = nameEndPt + 13;
	    filenameEndPt = memmem(filenamePt, inputEnd - filenamePt, "\"", strlen("\""));
	    if(filenameEndPt == 0)
		errAbort("Mangled CGI input (4) string %s", input);
	    *filenameEndPt = '\0';

	    snprintf(varNameFilename, 256, "%s__filename", namePt);

	    AllocVar(filenameEl);
	    filenameEl->val = filenamePt;
	    slAddHead(&list, filenameEl);
	    hashAddSaveName(hash, varNameFilename, filenameEl, &filenameEl->name);
	}

	if(filenamePt != 0 && doUseTempFile) {
	    struct tempName uploadedFile;
	    FILE* f;
	    char varNameFilename[256];
	    struct cgiVar *filenameEl;

	    makeTempName(&uploadedFile, "hgSs", ".cgi");

	    /* write the temp filename */
	    f = mustOpen(uploadedFile.forCgi, "w");
	    fwrite(dataPt, sizeof(char), dataEndPt - dataPt, f);
	    carefulClose(&f);

	    snprintf(varNameFilename, 256, "%s__data", namePt);

	    AllocVar(filenameEl);
	    filenameEl->val = cloneString(uploadedFile.forCgi);
	    slAddHead(&list, filenameEl);
	    hashAddSaveName(hash, varNameFilename, filenameEl, &filenameEl->name);
	} else {
	    AllocVar(el);
	    el->val = dataPt;
	    slAddHead(&list, el);
	    hashAddSaveName(hash, namePt, el, &el->name);
	}

	namePt = dataEndPt;
	}
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

str = getenv("HTTP_COOKIE");
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
struct cgiVar *var;

/* make sure that the cookie hash table has been created */
parseCookies(&cookieHash, &cookieList);
if (cookieHash == NULL)
    return NULL;
if ((var = hashFindVal(cookieHash, varName)) == NULL)
    return NULL;
return var->val;
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

static void cgiParseInput(char *input, struct hash **retHash, struct cgiVar **retList)
/* Parse cgi-style input into a hash table and list.  This will alter
 * the input data.  The hash table will contain references back 
 * into input, so please don't free input until you're done with 
 * the hash. */
{
char *namePt, *dataPt, *nextNamePt;
struct hash *hash = newHash(6);
struct cgiVar *list = NULL, *el;

namePt = input;
while (namePt != NULL && namePt[0] != 0)
    {
    dataPt = strchr(namePt, '=');
    if (dataPt == NULL)
	errAbort("Mangled CGI input string %s", namePt);
    *dataPt++ = 0;
    nextNamePt = strchr(dataPt, '&');
    if (nextNamePt == NULL)
	nextNamePt = strchr(dataPt, ';');	/* Accomodate DAS. */
    if (nextNamePt != NULL)
         *nextNamePt++ = 0;
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
    cgiParseMultipart(inputString, &inputHash, &inputList);
else
    cgiParseInput(inputString, &inputHash, &inputList);

/* now parse the cookies */
parseCookies(&cookieHash, &cookieList);
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

char *cgiUsualString(char *varName, char *usual)
/* Return value of string if it exists in cgi environment.  
 * Otherwiser return 'usual' */
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
char *s;
if (!cgiVarExists(varName))
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
char *s;
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
    if (!differentWord(choices[i].name, key))
        {
        val =  choices[i].value;
        return val;
        }
    }
errAbort("Unknown key %s for variable %s\n", key, varName);
return val;
}

void cgiMakeButton(char *name, char *value)
/* Make 'submit' type button. */
{
printf("<INPUT TYPE=SUBMIT NAME=\"%s\" VALUE=\"%s\">", name, value);
}

void cgiMakeRadioButton(char *name, char *value, boolean checked)
/* Make radio type button.  A group of radio buttons should have the
 * same name but different values.   The default selection should be
 * sent with checked on. */
{
printf("<INPUT TYPE=RADIO NAME=\"%s\" VALUE=\"%s\" %s>", name, value,
   (checked ? "CHECKED" : ""));
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
sprintf(buf, "%s%s", cgiBooleanShadowPrefix(), name);
return cgiVarExists(buf);
}

void cgiMakeCheckBox(char *name, boolean checked)
/* Make check box. Also make a shadow hidden variable so we
 * can distinguish between variable not present and
 * variable set to false. */
{
char buf[256];

printf("<INPUT TYPE=CHECKBOX NAME=\"%s\" VALUE=on%s>", name,
    (checked ? " CHECKED" : "") );
sprintf(buf, "%s%s", cgiBooleanShadowPrefix(), name);
cgiMakeHiddenVar(buf, "1");
}

void cgiMakeTextArea(char *varName, char *initialVal, int rowCount, int columnCount)
/* Make a text area with area rowCount X columnCount and with text: intialVal */
{
printf("<TEXTAREA NAME=\"%s\" ROWS=%d COLS=%d>%s</TEXTAREA>", varName,
       rowCount, columnCount, (initialVal != NULL ? initialVal : ""));
}

void cgiMakeTextVar(char *varName, char *initialVal, int charSize)
/* Make a text control filled with initial value.  If charSize
 * is zero it's calculated from initialVal size. */
{
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

void cgiMakeDropList(char *name, char *menu[], int menuSize, char *checked)
/* Make a drop-down list with names. */
{
int i;
char *selString;
if (checked == NULL) checked = menu[0];
printf("<SELECT ALIGN=CENTER NAME=\"%s\">\n", name);
for (i=0; i<menuSize; ++i)
    {
    if (!differentWord(menu[i], checked))
        selString = " SELECTED";
    else
        selString = "";
    printf("<OPTION%s>%s</OPTION>\n", selString, menu[i]);
    }
printf("</SELECT>\n");
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
    if (!differentWord(menu[i], checked))
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
    if (!differentWord(menu[i], checked))
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
    dyStringPrintf(dy, "%s=%s", cv->name, e);
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

if (preferWeb && cgiIsOnWeb())
    return TRUE;	/* No spoofing required! */
q += sprintf(q, "%s", "QUERY_STRING=cgiSpoof=on");
for (i=0; i<argcLeft; )
    {
    name = argv[i];
    if (startDash = (name[0] == '-'))
       ++name;
    gotEq = (strchr(name, '=') != NULL);
    if (gotEq || startDash)
        {
        if (needAnd)
            *q++ = '&';
        q += sprintf(q, "%s", name);
        if (!gotEq || strchr(name, '&') == NULL)
            needAnd = TRUE;
	if (!gotEq)
	    q += sprintf(q, "=on");
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

boolean cgiSpoof(int *pArgc, char *argv[])
/* If run from web line set up input
 * variables from web line, otherwise
 * set up from command line. */
{
return cgiFromCommandLine(pArgc, argv, TRUE);
}

