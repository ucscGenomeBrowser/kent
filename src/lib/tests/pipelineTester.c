/* pipelineTester - test program for pipeline object */
#include "common.h"
#include "pipeline.h"
#include "linefile.h"
#include "options.h"

static char const rcsid[] = "$Id: pipelineTester.c,v 1.2 2005/10/07 20:29:13 markd Exp $";

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
    "arguments to pipe together\n"
    "\n"
    "Options:\n"
    "  -exitCode=n - run with no-abort and expect this error code,\n"
    "   which can be zero.\n"
    "  -write - create a write pipeline\n"
    "  -pipeData=file - for a read pipeline, data read from the pipeline is copied\n"
    "   to this file for verification.  For a write pipeline, data from this\n"
    "   file is written to the pipeline.\n"
    "  -otherEnd=file - file for other end of pipeline\n"
    "  -fdApi - use the file descriptor API\n",
    msg);
}

static struct optionSpec options[] =
{
    {"exitCode", OPTION_INT},
    {"write", OPTION_BOOLEAN},
    {"pipeData", OPTION_STRING},
    {"otherEnd", OPTION_STRING},
    {"fdApi", OPTION_BOOLEAN},
    {NULL, 0},
};

/* options from command line */
boolean noAbort = FALSE;  /* don't abort, check exit code */
int expectExitCode = 0;   /* expected exit code */
boolean fdApi = FALSE; /* use the file descriptor API */
boolean isWrite = FALSE; /* make a write pipeline */
char *pipeDataFile = NULL;   /* use for input or output to the pipeline */
char *otherEndFile = NULL;   /* file for other end of pipeline */

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

char **splitCmd(char *cmdArgs)
/* split a command and arguments into null termiated list */
{
int numWords = chopByWhite(cmdArgs, NULL, 0);
char **words = needMem((numWords+1)*sizeof(char*));

chopByWhite(cmdArgs, words, numWords);
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

while (lineFileNext(pipeLf, &line, NULL))
    {
    fputs(line, dataFh);
    fputc('\n', dataFh);
    if (ferror(dataFh))
        errnoAbort("error writing data from pipeline to: %s", pipeDataFile);
    }

carefulClose(&dataFh);
}

void writeTest(struct pipeline *pl)
/* test a write pipeline */
{
FILE *pipeFh = pipelineFile(pl);
struct lineFile *dataLf = lineFileOpen(pipeDataFile, TRUE);
char *line;

while (lineFileNext(dataLf, &line, NULL))
    {
    fputs(line, pipeFh);
    fputc('\n', pipeFh);
    if (ferror(pipeFh))
        errnoAbort("error writing data to pipeline: %s", pipelineDesc(pl));
    }

lineFileClose(&dataLf);
}

void runPipeline(int nCmdsArgs, char **cmdsArgs)
/* pipeline tester */
{
unsigned options = (isWrite ? pipelineWrite : pipelineRead);
char ***cmds;
int exitCode, endOpenCnt;
int startOpenCnt = countOpenFiles();
int otherEndFd = -1;
struct pipeline *pl;

if (noAbort)
    options |= pipelineNoAbort;
cmds = splitCmds(nCmdsArgs, cmdsArgs);

if (fdApi)
    {
    otherEndFd = open(otherEndFile, (isWrite ? O_WRONLY|O_CREAT|O_TRUNC : O_RDONLY), 0777);
    if (otherEndFd < 0)
        errnoAbort("open of %s failed", otherEndFile);
    pl = pipelineOpenFd(cmds, options, otherEndFd, STDERR_FILENO);
    }
else
    pl = pipelineOpen(cmds, options, otherEndFile);

/* if no data file is specified, we just let the pipeline run without
 * interacting with it */
if (pipeDataFile != NULL)
    {
    if (isWrite)
        writeTest(pl);
    else
        readTest(pl);
    }

exitCode = pipelineWait(pl);
if (exitCode != expectExitCode)
    errAbort("expected exitCode %d, got %d", expectExitCode, exitCode);

if (fdApi)
    {
    if (close(otherEndFd) < 0)
        errnoAbort("close of otherEnd file descriptor failed");
    }

endOpenCnt = countOpenFiles();
/* it's ok to have less open, as would happen if we read from stdin */
if (endOpenCnt > startOpenCnt)
    errAbort("started with %d files open, now have %d open", startOpenCnt,
             endOpenCnt);
pipelineFree(&pl);
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
fdApi = optionExists("fdApi");
pipeDataFile = optionVal("pipeData", NULL);
otherEndFile = optionVal("otherEnd", NULL);
if (fdApi && (otherEndFile == NULL))
    errAbort("-fdApi requires -otherEndFile");
runPipeline(argc-1, argv+1);
return 0;
}
