/* pipeline.c - create a process pipeline that can be used for reading or
 * writing  */
#include "pipeline.h"
#include "common.h"
#include "dystring.h"
#include "errabort.h"
#include "portable.h"
#include "linefile.h"
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

/* FIXME: add close-on-exec startup hack */

struct plProc
/* A single process in a pipeline */
{
    struct plProc *next;   /* order list of processes */
    struct pipeline *pl;   /* pipeline we are associated with */
    char **cmd;            /* null-terminated command for this process */
    pid_t  pid;            /* pid for process, -1 if not running */
    int status;            /* status from wait */
};

struct pipeline
/* Object for a process pipeline and associated open file */
{
    struct pipeline *next;
    struct plProc *procs;      /* list of processes */
    char *procName;            /* name to use in error messages. */
    int pipeFd;                /* fd of pipe to/from process, -1 if none */
    unsigned options;          /* options */
    FILE* pipeFh;              /* optional stdio around pipe */
    char* stdioBuf;            /* optional stdio buffer */
    struct lineFile *pipeLf;   /* optional lineFile around pipe */
};

/* file buffer size */
#define FILE_BUF_SIZE 64*1024

static int pipeCreate(int *writeFd)
/* create a pipe of die, return readFd */
{
int pipeFds[2];
if (pipe(pipeFds) < 0)
    errnoAbort("can't create pipe");
*writeFd = pipeFds[1];
return pipeFds[0];
}

static void safeClose(int *fdPtr)
/* Close with error checking.  *fdPtr == -1 indicated already closed */
{
int fd = *fdPtr;
if (fd != -1)
    {
    if (close(fd) < 0)
        errnoAbort("close failed on fd %d", fd);
    *fdPtr = -1;
    }
}

static char* joinCmd(char **cmd)
/* join an cmd vector into a space separated string */
{
struct dyString *str = dyStringNew(512);
int i;
for (i = 0; cmd[i] != NULL; i++)
    {
    if (i > 0)
        dyStringAppend(str, " ");
    dyStringAppend(str, cmd[i]);
    }
return dyStringCannibalize(&str);
}

static char* joinCmds(char ***cmds)
/* join an cmds vetor into a space and pipe seperated string */
{
struct dyString *str = dyStringNew(512);
int i, j;
for (i = 0; cmds[i] != NULL; i++)
    {
    if (i > 0)
        dyStringAppend(str, " | ");
    for (j = 0; cmds[i][j] != NULL; j++)
        {
        if (j > 0)
            dyStringAppend(str, " ");
        dyStringAppend(str, cmds[i][j]);
        }
    }
return dyStringCannibalize(&str);
}

static struct plProc* plProcNew(char **cmd, struct pipeline *pl)
/* create a new plProc object for a command. */
{
int i, cmdLen = 0;
struct plProc* proc;
AllocVar(proc);
proc->pl = pl;

for (i = 0; cmd[i] != NULL; i++)
    cmdLen++;
proc->cmd = needMem((cmdLen+1)*sizeof(char*));

for (i = 0; i < cmdLen; i++)
    proc->cmd[i] = cloneString(cmd[i]);
proc->cmd[cmdLen] = NULL;
return proc;
}

static void plProcFree(struct plProc *proc)
/* free a plProc object. */
{
int i;
for (i = 0; proc->cmd[i] != NULL; i++)
    freeMem(proc->cmd[i]);
freeMem(proc->cmd);
freeMem(proc);
}

static void childAbortHandler()
/* abort handler that just exts */
{
exit(100);
}

static void plProcExecChild(struct plProc* proc, int stdinFd, int stdoutFd, int stderrFd)
/* child part of process startup */
{
int fd;
struct sigaction sigAct;

/* make sure abort handler exits */
pushWarnAbort();
pushAbortHandler(childAbortHandler);

/* treat a closed pipe as an EOF rather than getting SIGPIPE */
ZeroVar(&sigAct);
sigAct.sa_handler = SIG_IGN;
if (sigaction(SIGPIPE, &sigAct, NULL) != 0)
    errnoAbort("failed to set SIGPIPE to SIG_IGN");

/* child, first setup stdio files */
if (stdinFd != STDIN_FILENO)
    {
    if (dup2(stdinFd, STDIN_FILENO) < 0)
        errnoAbort("can't dup2 to stdin");
    }
    
if (stdoutFd != STDOUT_FILENO)
    {
    if (dup2(stdoutFd, STDOUT_FILENO) < 0)
        errnoAbort("can't dup2 to stdout");
    }
    
if (stderrFd != STDERR_FILENO)
    {
    if (dup2(stderrFd, STDERR_FILENO) < 0)
        errnoAbort("can't dup2 to stderr");
    }

/* close other file descriptors */
for (fd = STDERR_FILENO+1; fd < 64; fd++)
    close(fd);
execvp(proc->cmd[0], proc->cmd);
errnoAbort("exec failed: %s", proc->cmd[0]);
}

static void plProcExec(struct plProc* proc, int stdinFd, int stdoutFd, int stderrFd)
/* Start one process in a pipeline with the supplied stdio files.  Setting
 * stderrFh to STDERR_FILENO cause stderr to be passed through the pipeline
 * along with stdout */
{
if ((proc->pid = fork()) < 0)
    errnoAbort("can't fork");
if (proc->pid == 0)
    plProcExecChild(proc, stdinFd, stdoutFd, stderrFd);
}

static void plProcWait(struct plProc* proc)
/* wait for a process in a pipeline */
{
if (waitpid(proc->pid, &proc->status, 0) < 0)
    errnoAbort("process lost for: \"%s\" in pipeline \"%s\"", joinCmd(proc->cmd),
               proc->pl->procName);
if (WIFSIGNALED(proc->status))
    errAbort("process terminated on signal %d: \"%s\" in pipeline \"%s\"",
             WTERMSIG(proc->status), joinCmd(proc->cmd), proc->pl->procName);
assert(WIFEXITED(proc->status));

if ((WEXITSTATUS(proc->status) != 0) && !(proc->pl->options & pipelineNoAbort))
    errAbort("process exited with %d: \"%s\" in pipeline \"%s\"",
             WEXITSTATUS(proc->status), joinCmd(proc->cmd), proc->pl->procName);
proc->pid = -1;
}

static struct pipeline* pipelineNew(char ***cmds, unsigned options)
/* create a new pipeline object. Doesn't start processes */
{
struct pipeline *pl;
int iCmd;

AllocVar(pl);
pl->pipeFd = -1;
pl->options = options;
pl->procName = joinCmds(cmds);

for(iCmd = 0; cmds[iCmd] != NULL; iCmd++)
    slAddTail(&pl->procs, plProcNew(cmds[iCmd], pl));
if (pl->procs == NULL)
    errAbort("no commands in pipeline");

return pl;
}

void pipelineFree(struct pipeline **plPtr)
/* free a pipeline object */
{
struct pipeline *pl = *plPtr;
if (pl != NULL)
    {
    struct plProc *proc = pl->procs;
    while (proc != NULL)
        {
        struct plProc *delProc = proc;
        proc = proc->next;
        plProcFree(delProc);
        }
    freez(&pl->procName);
    freez(&pl->stdioBuf);
    freez(plPtr);
    }
}

static void pipelineExec(struct pipeline* pl, int stdinFd, int stdoutFd, int stderrFd)
/* Start all processes in a pipeline, stdinFd and stdoutFd are the ends of
 * the pipeline, stderrFd is applied to all processed */
{
struct plProc *proc;
int prevStdoutFd = -1;
for (proc = pl->procs; proc != NULL; proc = proc->next)
    {
    /* determine stdin/stdout to use */
    int procStdinFd, procStdoutFd;
    if (proc == pl->procs)
        procStdinFd = stdinFd; /* first process in pipeline */
    else
        procStdinFd = prevStdoutFd;
    if (proc->next == NULL)
        procStdoutFd = stdoutFd; /* last process in pipeline */
    else
        prevStdoutFd = pipeCreate(&procStdoutFd);

    plProcExec(proc, procStdinFd, procStdoutFd, stderrFd);

    /* don't leave intermediate pipes open in parent */
    if (proc != pl->procs)
        safeClose(&procStdinFd);
    if (proc->next != NULL)
        safeClose(&procStdoutFd);
    }
}

static int openOtherEndAsFD(char *otherEndFile)
/* open the other end as if it were a fd number */
{
int fd = -1;
if (startsWith("fd>",otherEndFile))
    {
    fd = atoi(otherEndFile+3);
    }
return fd;    
}

static int openOtherRead(char *otherEndFile)
/* open the other end of the pipeline for reading */
{
int fd;
if (otherEndFile != NULL)
    {
    fd = openOtherEndAsFD(otherEndFile);
    if (fd < 0)
	{
	fd = open(otherEndFile, O_RDONLY);
	if (fd < 0)
	    errnoAbort("can't open for read access: %s", otherEndFile);
	}
    }
else
    fd = STDIN_FILENO;
return fd;
}

static int openOtherWrite(char *otherEndFile)
/* open the other end of the pipeline for writing */
{
int fd;
if (otherEndFile != NULL)
    {
    fd = openOtherEndAsFD(otherEndFile);
    if (fd < 0)
	{
	fd = open(otherEndFile, O_WRONLY|O_CREAT, 0666);
    	if (fd < 0)
	    errnoAbort("can't open for write access: %s", otherEndFile);
	}
    }
else 
    fd = STDOUT_FILENO;
return fd;
}

static void pipelineStartRead(struct pipeline *pl, char *otherEndFile)
/* start a read pipeline */
{
int otherEndFd = openOtherRead(otherEndFile);
int pipeWrFd;

pl->pipeFd = pipeCreate(&pipeWrFd);
pipelineExec(pl, otherEndFd, pipeWrFd, STDERR_FILENO);
safeClose(&otherEndFd);
safeClose(&pipeWrFd);
}

static void pipelineStartWrite(struct pipeline *pl, char *otherEndFile)
/* start a write pipeline */
{
int otherEndFd = openOtherWrite(otherEndFile);
int pipeRdFd;

pipeRdFd = pipeCreate(&pl->pipeFd);
pipelineExec(pl, pipeRdFd, otherEndFd, STDERR_FILENO);
safeClose(&otherEndFd);
safeClose(&pipeRdFd);
}

struct pipeline *pipelineOpen(char ***cmds, unsigned opts,
                              char *otherEndFile)
/* Create a pipeline from an array of commands.  See pipeline.h for
 * full documentation */
{
struct pipeline *pl;

if (((opts & (pipelineRead|pipelineWrite)) == 0)
    || ((opts & (pipelineRead|pipelineWrite)) == (pipelineRead|pipelineWrite)))
    errAbort("must specify one of pipelineRead or pipelineWrite to pipelineOpen");

pl = pipelineNew(cmds, opts);
if (opts & pipelineRead)
    pipelineStartRead(pl, otherEndFile);
else
    pipelineStartWrite(pl, otherEndFile);
return pl;
}

struct pipeline *pipelineOpen1(char **cmd, unsigned opts,
                               char *otherEndFile)
/* like pipelineOpen(), only takes a single command */
{
char **cmds[2];
cmds[0] = cmd;
cmds[1] = NULL;
return pipelineOpen(cmds, opts, otherEndFile);
}

char *pipelineDesc(struct pipeline *pl)
/* Get the description of a pipeline for use in error messages */
{
return pl->procName;
}

int pipelineFd(struct pipeline *pl)
/* Get the file descriptor for a pipeline */
{
return pl->pipeFd;
}

FILE *pipelineFile(struct pipeline *pl)
/* Get a FILE object wrapped around the pipeline.  Do not close the FILE, is
 * owned by the pipeline object.  A FILE is created on first call to this
 * function.  Subsequent calls return the same FILE.*/
{
if (pl->pipeFh == NULL)
    {
    /* create FILE* on first acess */
    char *mode = (pl->options & pipelineRead) ? "r" : "w";
    if (pl->pipeLf != NULL)
        errAbort("can't call pipelineFile after having associated a lineFile with a pipeline");
    pl->pipeFh = fdopen(pl->pipeFd, mode);
    if (pl->pipeFh == NULL)
        errnoAbort("fdopen failed for: %s", pl->procName);
    pl->stdioBuf = needLargeMem(FILE_BUF_SIZE);
    setvbuf(pl->pipeFh, pl->stdioBuf,  _IOFBF, FILE_BUF_SIZE);
    }
return pl->pipeFh;
}

struct lineFile *pipelineLineFile(struct pipeline *pl)
/* Get a lineFile object wrapped around the pipeline.  Do not close the
 * lineFile, is owned by the pipeline object.  A lineFile is created on first
 * call to this function.  Subsequent calls return the same object.*/
{
if (pl->pipeLf == NULL)
    {
    /* create line on first acess */
    if (pl->pipeFh != NULL)
        errAbort("can't call pipelineLineFile after having associated a FILE with a pipeline");
    if (pl->options & pipelineWrite)
        errAbort("can't associated a lineFile with a write pipeline");
    pl->pipeLf = lineFileAttach(pipelineDesc(pl), TRUE, pl->pipeFd);
    }
return pl->pipeLf;
}

static void closePipelineFile(struct pipeline *pl)
/* close a pipeline with a FILE associated with it */
{
if (pl->options & pipelineWrite)
    {
    fflush(pl->pipeFh);
    if (ferror(pl->pipeFh))
        errAbort("write failed to pipeline: %s ", pl->procName);
    }
else if (ferror(pl->pipeFh))
    errAbort("read failed from pipeline: %s ", pl->procName);

if (fclose(pl->pipeFh) == EOF)
    errAbort("close failed on pipeline: %s ", pl->procName);
pl->pipeFh = NULL;
}

static void closePipeline(struct pipeline *pl)
/* Close the pipe file */
{
if (pl->pipeFh != NULL)
    closePipelineFile(pl);
else if (pl->pipeLf != NULL)
    lineFileClose(&pl->pipeLf);
else
    {
    if (close(pl->pipeFd) < 0)
        errAbort("close failed on pipeline: %s ", pl->procName);
    }
pl->pipeFd = -1;
}

int pipelineWait(struct pipeline *pl)
/* Wait for processes in a pipeline to complete; normally aborts if any
 * process exists non-zero.  If pipelineNoAbort was specified, return the exit
 * code of the first process exit non-zero, or zero if none failed. */
{
struct plProc *proc;
int exitCode = 0;

/* must close before waits for output pipeline */
if (pl->options & pipelineWrite)
    closePipeline(pl);

/* wait on each process in order */
for (proc = pl->procs; proc != NULL; proc = proc->next)
    {
    plProcWait(proc);
    if ((WEXITSTATUS(proc->status) != 0) && (exitCode == 0))
        exitCode = WEXITSTATUS(proc->status);
    }

/* must close after waits for input pipeline */
if (pl->options & pipelineRead)
    closePipeline(pl);

return exitCode;
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

