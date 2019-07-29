/* chrisCrawl - Experiments in analyzing apache logs. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "obscure.h"

/* This program is invoked like so:
 * chrisCrawl error_log_file stdout
 *
 * and creates output like the following:
 * pid     client  timestamp       CGI     referrer        extra
 *
 * pid 167419      client 220.243.135.94:53638     Sun Jun 02 12:54:36.387058 2019 hgTracks        (null)
 *         error message: can not find any trackDb tables for rn2, check db.trackDb specification in hg.conf
 *         ../sysdeps/unix/sysv/linux/waitpid.c:__libc_waitpid
 *         osunix.c:vaDumpStack
 *         osunix.c:dumpStack
 *         hCommon.c:hDumpStackAbortHandler
 *         errAbort.c:noWarnAbort
 *         errAbort.c:vaErrAbort
 *         ...
 *
 * The gist is to just clean up the error messages somewhat and then print out a count
 * of the most common offending functions across all the stack dumps.
 *
 * This was the first C program I ever wrote so it can probably be made much more efficient
 * and faster. Currently the way it works is the program scans through the error log and
 * try to group similar log lines into the errorProcessLog struct. Then the program goes
 * through the lineList part of the struct errorProcessLog and tries to make it into
 * something more readable.
 */

char *clApacheErrorType = "cgi:error";
struct hash *functionCountHash = NULL;

struct stackDump
/* The CGI, timestamp, and stack trace from a dumped stack in an apache error log */
    {
    struct stackDump *next;
    char *timeStamp;    /* Time stamp of the start of stack trace, ex: Mon Apr 16 03:27:04.945609 2018 */
    char *duration;  /* elapsed time to return request */
    char *pid;       /* process ID */
    char *client;       /* IP Address: dotted quad of numbers, or xxx.com. */
    char *cgiName; /* Name of offending CGI */
    struct slName *methods; /* List of methods leading to trace */
    struct slName *lineList; /* */
    char *referer; /* Refering URL, may be NULL. */
    char *message;  /* Primary error message */
    };

struct errorProcessLog
/* All lines associated with a single CGI process bundled together along with a little
 * information that is common to them all. */
    {
    struct errorProcessLog *next;
    char *cgiErrorType; /* CGI error type, like "Stack dump" or "CGI_TIME" */
    char *timeStamp;    /* Time stamp like Mon Apr 16 03:27:04.945609 2018 */
    char *duration;  /* elapsed time to return request */
    char *errorType; /* type of error, like cgi:error or ssl:warn */
    char *pid;       /* process ID */
    char *client;       /* IP Address: dotted quad of numbers, or xxx.com. */
    char *cgiName; /* Name of offending CGI */
    char *referer; /* Referring URL, may be NULL. */
    struct slName *lineList;  /* List of lines bypassing some of the repeated stuff */
    };

struct partlyParsedLine
/* A partly parsed out line from an error log file. This is a temporary structure with
 * pointers into reused space.  Don't copy string values but clone them if inside of a loop
 * and putting into more permanent storage. */
    {
    struct partlyParsedLine *next;
    char *date; /* Contains date in format Day Mon DD HH:MM:SS.123456 YYYY */
    char *apacheErrorType;   /* Contains apache classification of error, often cgi::error */
    char *pid;          /* Contains pid # */
    char *client; /* Contains client dotted quad */
    char *ahWord; /* Not sure, a word that seems to start with AH and end with a colon that we eat */
    char *cgiName; /* Optional CGI name that generated the error */
    char *hgsid; /* Optional hgsid parameter of the offending error, in the format hgsid=hash */
    char *referer; /* optional referer url in the 'rest' string */
    char *rest; /* The rest of the line */
    };

void usage()
/* Explain usage and exit. */
{
errAbort(
  "chrisCrawl - Print counts of files:functions implicated in stack dumps\n"
  "usage:\n"
  "   chrisCrawl logfileName outFile\n\n"
  "options:\n"
  "-errorType    default \"cgi:error\" but can be any string from the second bracketed\n"
                 "part of an error log"
  "outFile can be stdout to print to stdout\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"errorType", OPTION_STRING},
   {NULL, 0},
};

char *betweenBrackets(char **pString)
/* Returns next part of *pString that starts with a '[' and ends with a ']'
 * and advances pString to letter past final ']'.  Returns NULL if not found.
 * The lf paremeter is just for better error reporting. */
{
char *s = *pString;
if (s == NULL) // Handle repeat calls on empty input gracefully
   return NULL;

// Find opening brace
if ((s = strchr(s, '[')) == NULL)
   return NULL;
// Skip over opening brace and save it
s += 1;
char *start = s;

// Find closing brace and replace it with a zero
s = strchr(s, ']');
if (s == NULL)
    {
    //don't warn always because somtimes messages span multiple lines
    //warn("Unclosed [ line %d of %s\n", lf->lineIx, lf->fileName);
    return NULL;
    }
*s++ = 0;

/* Update pointer and go home */
*pString = s;
return start;
}

boolean partlyParseErrorLogLine(struct lineFile *lf, char *line, struct partlyParsedLine *ppl)
/* Parse out an error log line.  Return TRUE on success */
{
ZeroVar(ppl);

/* Parse out the square bracket delimited fields */
ppl->date = betweenBrackets(&line);
ppl->apacheErrorType = betweenBrackets(&line);
ppl->pid = betweenBrackets(&line);
ppl->client = betweenBrackets(&line);

/* Now parse out the ahLine */
line = skipLeadingSpaces(line);
if (startsWith("AH", line))
    {
    ppl->ahWord = line;
    line = strchr(line, ':');
    if (line == NULL)
        {
        warn("missing colon after bracketed fields line %d of %s", lf->lineIx, lf->fileName);
        return FALSE;
        }
    *line++ = 0;
    }

// parse out optional CGIname and hgsid parameters
// skip repeated date
betweenBrackets(&line);
ppl->cgiName = betweenBrackets(&line);
ppl->hgsid = betweenBrackets(&line);

// parse out optional referer section
if (strstr(line, "referer") != NULL)
    {
    ppl->referer = cloneString(strstr(line, "referer"));
    int ix = strlen(ppl->referer);
    line[strlen(line)-ix] = '\0';
    if (!sameString(trimSpaces(line), ","))
        ppl->rest = trimSpaces(line);
    }
else
    {
    char *temp = trimSpaces(line);
    if (!sameString(temp, ","))
        ppl->rest = temp;
    }
return TRUE;
}

void sortAndPrintFunctionCounts(FILE *f, struct hash *functionHash)
{
struct hashEl *el, *elList = hashElListHash(functionHash);
slSort(&elList, hashElCmpIntValDesc);
fprintf(f, "fileName:functionName\terrorCount\n");
for (el = elList; el != NULL; el = el->next)
    {
    fprintf(f, "%s\t%d\n", el->name, hashIntVal(functionHash, el->name));
    }
}

void makeReport(FILE *f, struct stackDump *errorList)
/* pretty print stack dump info */
{
struct stackDump  *temp = NULL;
fprintf(f, "pid\tclient\ttimestamp\tCGI\treferrer\textra\n");
for (temp = errorList; temp != NULL; temp=temp->next)
    {
    fprintf(f, "\n%s\t%s\t%s\t%s\t%s\n", temp->pid, temp->client, temp->timeStamp, temp->cgiName,  temp->referer);
    fprintf(f, "\terror message: %s\n", temp->message);
    struct slName *line;
    for (line = temp->lineList; line != NULL; line = line->next)
        fprintf(f, "\t%s\n", line->name);
    }
}

struct errorProcessLog *parseIntoProcesses(struct lineFile *lf, char *cgiErrorType)
/* Group into records based on referrer ip and pid */
{
char *line;
struct errorProcessLog *eplList = NULL;
struct hash *eplHash = hashNew(0);
while (lineFileNext(lf, &line, NULL))
    {
    struct partlyParsedLine ppl;
    if (partlyParseErrorLogLine(lf, line, &ppl))
        {
        if (sameString(clApacheErrorType, ppl.apacheErrorType))
            {
            char name[128];
            safef(name, sizeof(name), "[%s][%s]", ppl.pid, ppl.client);
            struct errorProcessLog *epl = hashFindVal(eplHash, name);
            if (epl == NULL)
                {
                AllocVar(epl);
                epl->pid = cloneString(ppl.pid);
                epl->client = cloneString(ppl.client);
                epl->cgiName = cloneString(ppl.cgiName);
                epl->referer = cloneString(ppl.referer);
                epl->timeStamp = cloneString(ppl.date);
                hashAdd(eplHash, name, epl);
                slAddHead(&eplList, epl);
                }
            if (isNotEmpty(ppl.rest))
                {
                if (strstr(ppl.rest, cgiErrorType))
                    epl->cgiErrorType = cloneString(cgiErrorType);
                slNameAddTail(&epl->lineList, ppl.rest);
                }
            }
        }
    }
hashFree(&eplHash);
slReverse(&eplList);
return eplList;
}

void getMessage(struct stackDump *sd, struct errorProcessLog *epl)
/* Get error message out of apache error log line errorMsg and fill out sd appropriately*/
{
char *temp, *message = cloneString(epl->lineList->name); // message holds final result
while ((temp = betweenBrackets(&message)) != NULL)
    if (epl->cgiName != NULL)
        sd->cgiName = cloneString(epl->cgiName);
message = skipLeadingSpaces(message);
if (sd->message == NULL)
    {
    sd->message = cloneString(message);
    sd->pid = cloneString(epl->pid);
    sd->client = cloneString(epl->client);
    sd->timeStamp = cloneString(epl->timeStamp);
    sd->referer = cloneString(epl->referer);
    }
else
    sd->message = catTwoStrings(sd->message, cloneString(message));
}

void parseStackDumpLine(struct stackDump *sd, char *line)
/* Parse the stack dump part of an epl */
{
static struct dyString *p;
char *copy;
char *words[256];
int i;
ZeroVar(words);
if (p == NULL)
    p = dyStringNew(0);
else
    dyStringClear(p);
if (functionCountHash == NULL)
    functionCountHash = hashNew(0);

copy = cloneString(line);
eraseTrailingSpaces(copy);
int wc = chopByWhite(copy, NULL, 0);
if (wc > 5)
    {
    chopByWhite(copy, words, ArraySize(words));
    // get rid of line numbers because they will always change
    char *chopped[2];
    int chopCount = chopString(words[wc-1], ":", NULL, 0);
    if (chopCount > 0)
        {
        chopString(words[wc-1], ":", chopped, 2);
        words[wc-1] = chopped[0];
        }
    dyStringPrintf(p, "%s:%s", words[wc-1], words[3]);
    if (!hashLookup(functionCountHash, p->string))
        hashAddInt(functionCountHash, p->string, 1);
    else
        hashIncInt(functionCountHash, p->string);
    slNameAddHead(&sd->lineList, p->string);
    }
else
    {
    stripChar(copy, ',');
    chopByWhite(copy, words, ArraySize(words));
    for (i = 1; i < wc-1; i++)
        dyStringPrintf(p, "%s ", words[i]);
    dyStringPrintf(p, "%s", words[wc-1]);
    slNameAddHead(&sd->lineList, p->string);
    }
}

void errorProcessToStackDump(struct stackDump *sd, struct errorProcessLog *epl)
/* For a given errorProcess epl, extract any potential information and fill out sd */
{
struct slName *l;
for (l = epl->lineList; l != NULL; l = l->next)
    {
    if (startsWith("[", l->name))
        {
        getMessage(sd, epl);
        }
    else if (startsWith("#", l->name))
        {
        parseStackDumpLine(sd, l->name);
        }
    }
slReverse(&sd->lineList);
}

struct stackDump *collateProcesses(struct errorProcessLog *errorList, char *type)
/* Go through a list of all errors and pick out those that we want */
{
struct errorProcessLog *epl = NULL, *next;
struct stackDump *sd, *sdl = NULL;
for (epl = errorList; epl != NULL; epl = next)
    {
    next = epl->next;
    if (epl->cgiErrorType != NULL && sameString(type, epl->cgiErrorType))
        {
        AllocVar(sd);
        errorProcessToStackDump(sd, epl);
        slAddHead(&sdl, sd);
        }
    }
slReverse(&sdl);
return sdl;
}

void parseErrorLog(char *errLog, char *outFile)
/* parseErrorLog - Do something experimental with the error log.. */
{
FILE *f = mustOpen(outFile, "w");
struct lineFile *lf = lineFileOpen(errLog, TRUE);
struct errorProcessLog *errorList;
struct stackDump *reportList;
errorList = parseIntoProcesses(lf, "Stack");
reportList = collateProcesses(errorList, "Stack");
makeReport(f, reportList);
fprintf(f, "\n");
if (functionCountHash)
    sortAndPrintFunctionCounts(f, functionCountHash);
carefulClose(&f);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);

if (optionExists("errorType"))
    clApacheErrorType = optionVal("errorType",NULL);

if (argc != 3)
    usage();
parseErrorLog(argv[1], argv[2]);
return 0;
}
