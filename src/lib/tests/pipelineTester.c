/* pipelineTester - test program for pipeline object */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "limits.h"
#include "common.h"
#include "pipeline.h"
#include "linefile.h"
#include "options.h"


void usage(char *msg)
/* Explain usage and exit. */
{
errAbort(
    "%s\n\n"
    "pipelineTester - test program for pipeline object\n"
    "\n"
    "Usage:\n"
    "   pipelineTester [options] cmdArgs1 [cmdArgs2 ..]\n"
    "\n"
    "Each cmdArgs are a command and whitespace separated\n"
    "arguments to pipe together.  Within a cmdArgs, single or\n"
    "double quote maybe used to quote words.\n"
    "\n"
    "Options:\n"
    "  -exitCode=n - run with no-abort and expect this error code,\n"
    "   which can be zero.\n"
    "  -write - create a write pipeline\n"
    "  -memApi - test memory buffer API\n"
    "  -pipeData=file - for a read pipeline, data read from the pipeline is copied\n"
    "   to this file for verification.  For a write pipeline, data from this\n"
    "   file is written to the pipeline.\n"
    "  -otherEnd=file - file for other end of pipeline\n"
    "  -stderr=file - file for stderr of pipeline\n"
    "  -fdApi - use the file descriptor API\n"
    "  -sigpipe - enable SIGPIPE.\n"
    "  -maxNumLines - read or write this many lines and close (for testing -sigpipe)\n",
    msg);
}

static struct optionSpec options[] =
{
    {"exitCode", OPTION_INT},
    {"write", OPTION_BOOLEAN},
    {"memApi", OPTION_BOOLEAN},
    {"pipeData", OPTION_STRING},
    {"otherEnd", OPTION_STRING},
    {"stderr", OPTION_STRING},
    {"fdApi", OPTION_BOOLEAN},
    {"sigpipe", OPTION_BOOLEAN},
    {"maxNumLines", OPTION_INT},
    {NULL, 0},
};

/* options from command line */
boolean noAbort = FALSE;  /* don't abort, check exit code */
int expectExitCode = 0;   /* expected exit code */
boolean fdApi = FALSE; /* use the file descriptor API */
boolean isWrite = FALSE; /* make a write pipeline */
boolean memApi = FALSE; /* test memory buffer API */
boolean sigpipe = FALSE; /* enable sigpipe */
int maxNumLines = INT_MAX;  /* number of lines to read or write */
char *pipeDataFile = NULL;   /* use for input or output to the pipeline */
char *otherEndFile = NULL;   /* file for other end of pipeline */
char *stderrFile = NULL;   /* file for other stderr of pipeline */

int countOpenFiles()
/* count the number of opens.  This is used to make sure no stray
 * pipes have been left open. */
{
int cnt = 0;
int fd;
struct stat statBuf;
for (fd = 0; fd < 64; fd++)
    {
    if (fstat(fd, &statBuf) == 0)
        cnt++;
    }
return cnt;
}

char *parseQuoted(char **spPtr, char *cmdArgs)
/* parse a quoted string */
{
char *start = *spPtr;
char quote = *start++;
char *end = strchr(start, quote);
if (end == NULL)
    errAbort("no closing quote in: %s", cmdArgs);
*spPtr = end+1;
return cloneStringZ(start, end-start);
}

char **splitCmd(char *cmdArgs)
/* split a command and arguments into null termiated list, handling quoting */
{
// this will get the maximum number of words, as it doesn't consider quoting
int maxNumWords = chopByWhite(cmdArgs, NULL, 0);
char **words = needMem((maxNumWords+1)*sizeof(char*));
int iWord = 0;
char *sp = skipLeadingSpaces(cmdArgs);

while (*sp != '\0')
    {
    assert(iWord < maxNumWords);
    if ((*sp == '"') || (*sp == '\''))
        words[iWord++] = parseQuoted(&sp, cmdArgs);
    else
        {
        char *end = skipToSpaces(sp);
        if (end == NULL)
            end = sp + strlen(sp);
        words[iWord++] = cloneStringZ(sp, (end - sp));
        sp = end;
        }
    sp = skipLeadingSpaces(sp);
    }
return words;
}

char ***splitCmds(int nCmdsArgs, char **cmdsArgs)
/* split all commands into a pipeline */
{
char ***cmds = needMem((nCmdsArgs+1)*sizeof(char**));
int i;
for (i = 0; i < nCmdsArgs; i++)
    cmds[i] = splitCmd(cmdsArgs[i]);
return cmds;
}

void readTest(struct pipeline *pl)
/* test a read pipeline */
{
struct lineFile *pipeLf = pipelineLineFile(pl);
FILE *dataFh = mustOpen(pipeDataFile, "w");
char *line;
int numLines = 0;

while (lineFileNext(pipeLf, &line, NULL) && (numLines < maxNumLines))
    {
    fputs(line, dataFh);
    fputc('\n', dataFh);
    if (ferror(dataFh))
        errnoAbort("error writing data from pipeline to: %s", pipeDataFile);
    numLines++;
    }

carefulClose(&dataFh);
}

void writeTest(struct pipeline *pl)
/* test a write pipeline */
{
FILE *pipeFh = pipelineFile(pl);
struct lineFile *dataLf = lineFileOpen(pipeDataFile, TRUE);
char *line;
int numLines = 0;

while (lineFileNext(dataLf, &line, NULL) && (numLines < maxNumLines))
    {
    fputs(line, pipeFh);
    fputc('\n', pipeFh);
    if (ferror(pipeFh))
        errnoAbort("error writing data to pipeline: %s", pipelineDesc(pl));
    numLines++;
    }

lineFileClose(&dataLf);
}

void *loadMemData(char *dataFile, size_t *dataSizeRet)
/* load a file into memory */
{
off_t dataSize = fileSize(dataFile);
void *buf = needLargeMem(dataSize);
FILE *fh = mustOpen(dataFile, "r");

mustRead(fh, buf, dataSize);
carefulClose(&fh);
*dataSizeRet = dataSize;
return buf;
}

void runPipelineTest(struct pipeline *pl)
/* execute and validate test once pipeline has been created */
{
/* if no data file is specified, we just let the pipeline run without
 * interacting with it */
if (pipeDataFile != NULL)
    {
    if (isWrite)
        writeTest(pl);
    else
        readTest(pl);
    }
int exitCode = pipelineWait(pl);
if (exitCode != expectExitCode)
    errAbort("expected exitCode %d, got %d", expectExitCode, exitCode);
}

void pipelineTestFd(char ***cmds, unsigned options)
/* test for file descriptor API */
{
int mode = (isWrite ? O_WRONLY|O_CREAT|O_TRUNC : O_RDONLY);
int otherEndFd = mustOpenFd(otherEndFile, mode);
int stderrFd = (stderrFile == NULL) ? STDERR_FILENO
    : mustOpenFd(stderrFile, O_WRONLY|O_CREAT|O_TRUNC);
struct pipeline *pl = pipelineOpenFd(cmds, options, otherEndFd, stderrFd);
runPipelineTest(pl);
pipelineFree(&pl);
mustCloseFd(&otherEndFd);
if (stderrFile != NULL)
    mustCloseFd(&stderrFd);
}

void pipelineTestMem(char ***cmds, unsigned options)
/* test for memory buffer API */
{
int stderrFd = (stderrFile == NULL) ? STDERR_FILENO
    : mustOpenFd(stderrFile, O_WRONLY|O_CREAT|O_TRUNC);
size_t otherEndBufSize = 0;
void *otherEndBuf = loadMemData(otherEndFile, &otherEndBufSize);
struct pipeline *pl = pipelineOpenMem(cmds, options, otherEndBuf, otherEndBufSize, stderrFd);
runPipelineTest(pl);
pipelineFree(&pl);
freeMem(otherEndBuf);
if (stderrFile != NULL)
    mustCloseFd(&stderrFd);
}

void pipelineTestFName(char ***cmds, unsigned options)
/* test for file name API */
{
struct pipeline *pl = pipelineOpen(cmds, options, otherEndFile, stderrFile);
runPipelineTest(pl);
pipelineFree(&pl);
}

void pipelineTester(int nCmdsArgs, char **cmdsArgs)
/* pipeline tester */
{
unsigned options = (isWrite ? pipelineWrite : pipelineRead);
if (noAbort)
    options |= pipelineNoAbort;
if (sigpipe)
    options |= pipelineSigpipe;
int startOpenCnt = countOpenFiles();
char ***cmds = splitCmds(nCmdsArgs, cmdsArgs);

if (fdApi)
    pipelineTestFd(cmds, options);
else if (memApi)
    pipelineTestMem(cmds, options);
else
    pipelineTestFName(cmds, options);

/* it's ok to have less open, as would happen if we read from stdin */
int endOpenCnt = countOpenFiles();
if (endOpenCnt > startOpenCnt)
    errAbort("started with %d files open, now have %d open", startOpenCnt,
             endOpenCnt);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 2)
    usage("wrong number of args");
if (optionExists("exitCode"))
    {
    noAbort = TRUE;
    expectExitCode = optionInt("exitCode", 0);
    }
isWrite = optionExists("write");
memApi = optionExists("memApi");
fdApi = optionExists("fdApi");
sigpipe = optionExists("sigpipe");
maxNumLines = optionInt("maxNumLines", INT_MAX);
if (fdApi && memApi)
    errAbort("can't specify both -fdApi and -memApi");
pipeDataFile = optionVal("pipeData", NULL);
otherEndFile = optionVal("otherEnd", NULL);
if (fdApi && (otherEndFile == NULL))
    errAbort("-fdApi requires -otherEndFile");
if (memApi && (otherEndFile == NULL))
    errAbort("-memApi requires -otherEndFile");
stderrFile = optionVal("stderr", NULL);
pipelineTester(argc-1, argv+1);
return 0;
}
