/* pipeline.c - create a process pipeline that can be used for reading or
 * writing  */
#include "pipeline.h"
#include "common.h"
#include "dystring.h"
#include "errabort.h"
#include "portable.h"
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>

/* FIXME: add close-on-exec startup hack */

enum pipelineFlags
/* internal option bitset */
    {
    pipelineRead    = 0x01, /* read from pipeline */
    pipelineWrite   = 0x02, /* write to pipeline */
    pipelineAppend  = 0x04, /* write to pipeline, appends to output file */
    };

struct plProc
/* A single process in a pipeline */
{
    struct plProc *next;   /* order list of processes */
    struct pipeline *pl;   /* pipeline we are associated with */
    char **cmd;            /* null-terminated command for this process */
    pid_t  pid;            /* pid for process */
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
};

/* file buffer size */
#define FILE_BUF_SIZE 64*1024

static int devNullOpen(int flags)
/* open /dev/null with the specified flags */
{
int fd = open("/dev/null", flags);
if (fd < 0)
    errnoAbort("can't open /dev/null");
return fd;
}

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
    free(proc->cmd[i]);
free(proc->cmd);
free(proc);
}

static void plProcExec(struct plProc* proc, int stdinFd, int stdoutFd, int stderrFd)
/* Start one process in a pipeline with the supplied stdio files.  Setting
 * stderrFh to STDERR_FILENO cause stderr to be passed through the pipeline
 * along with stdout */
{
int fd;
if ((proc->pid = fork()) < 0)
    errnoAbort("can't fork");
if (proc->pid == 0)
    {
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
}

static void plProcWait(struct plProc* proc)
/* wait for a process in a pipeline */
{
int status;
if (waitpid(proc->pid, &status, 0) < 0)
    errnoAbort("process lost for: %s in %s", joinCmd(proc->cmd),
               proc->pl->procName);
if (WIFSIGNALED(status))
    errAbort("process terminated on signal %d: %s in %s",
             WTERMSIG(status), joinCmd(proc->cmd), proc->pl->procName);
if (WEXITSTATUS(status) != 0)
    errAbort("process exited with %d: %s in %s",
             WEXITSTATUS(status), joinCmd(proc->cmd), proc->pl->procName);
proc->pid = -1;
}

static struct pipeline* pipelineNew(char ***cmds, unsigned options)
/* create a new pipeline object */
{
struct pipeline *pl;
AllocVar(pl);
pl->pipeFd = -1;
pl->options = options;
pl->procName = joinCmds(cmds);
return pl;
}

static void pipelineFree(struct pipeline **plPtr)
/* free a pipeline object, removing from open list */
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

static void pipelineBuild(struct pipeline *pl, char ***cmds)
/* Build the list of plProc objects from an array of commands. Doesn't start
 * processes */
{
int iCmd;

for(iCmd = 0; cmds[iCmd] != NULL; iCmd++)
    {
    struct plProc *proc = plProcNew(cmds[iCmd], pl);
    slAddTail(&pl->procs, proc);
    }
if (pl->procs == NULL)
    errAbort("no commands in pipeline");
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
    int procStdinFd = (prevStdoutFd < 0) ? stdinFd : prevStdoutFd;
    int procStdoutFd;
    if (proc->next == NULL)
        procStdoutFd = stdoutFd;
    else
        prevStdoutFd = pipeCreate(&procStdoutFd);
    plProcExec(proc, procStdinFd, procStdoutFd, stderrFd);
    }
}

static void checkOpts(unsigned opts, char *stdFile)
/* check that options and stdin/out file make sense together */
{
if ((opts & pipelineInheritFd) && (opts & pipelineDevNull))
    errAbort("can't specify both pipelineInheritFd and pipelineDevNull when creating a pipeline");
if (stdFile != NULL)
    {
    if ((opts & pipelineInheritFd) || (opts & pipelineDevNull))
        errAbort("can't specify pipelineInheritFd or pipelineDevNull with a file path when creating a pipeline");
    }
else
    {
    if (!((opts & pipelineInheritFd) || (opts & pipelineDevNull)))
        errAbort("must specify pipelineInheritFd or pipelineDevNull or a file path when creating a pipeline");
    }
}

struct pipeline *pipelineCreateRead(char ***cmds, unsigned opts,
                                    char *stdinFile)
/* create a read pipeline from an array of commands.  Each command is
 * an array of arguments.  Shell expansion is not done on the arguments.
 * If stdinFile is not NULL, it will be opened as the input to the pipe,
 * otherwise pipelineOpts is used to determine the input file.
 */
{
struct pipeline *pl = pipelineNew(cmds, pipelineRead);
int stdinFd = -1, pipeWrFd;
checkOpts(opts, stdinFile);
if (stdinFile != NULL)
    {
    stdinFd = open(stdinFile, O_RDONLY);
    if (stdinFd < 0)
        errnoAbort("can't open for read access: %s", stdinFile);
    }
else if (opts & pipelineInheritFd)
    stdinFd = STDIN_FILENO;
else if (opts & pipelineDevNull)
    stdinFd = devNullOpen(O_RDONLY);

pl->pipeFd = pipeCreate(&pipeWrFd);
pipelineBuild(pl, cmds);
pipelineExec(pl, stdinFd, pipeWrFd, STDERR_FILENO);
safeClose(&stdinFd);
safeClose(&pipeWrFd);
return pl;
}

struct pipeline *pipelineCreateRead1(char **cmd, unsigned opts,
                                     char *stdinFile)
/* like pipelineCreateRead(), only takes a single command */
{
char **cmds[2];
cmds[0] = cmd;
cmds[1] = NULL;
return pipelineCreateRead(cmds, opts, stdinFile);
}

struct pipeline *pipelineCreateWrite(char ***cmds, unsigned opts,
                                     char *stdoutFile)
/* create a write pipeline from an array of commands.  Each command is
 * an array of arguments.  Shell expansion is not done on the arguments.
 * If stdoutFile is not NULL, it will be opened as the output from the pipe.
 * otherwise pipelineOpts is used to determine the output file.
 */
{
struct pipeline *pl = pipelineNew(cmds, pipelineWrite);
int stdoutFd = -1, pipeRdFd;
checkOpts(opts, stdoutFile);
if (stdoutFile != NULL)
    {
    stdoutFd = open(stdoutFile, O_WRONLY|O_CREAT);
    if (stdoutFd < 0)
        errnoAbort("can't open for write access: %s", stdoutFile);
    }
else if (opts & pipelineInheritFd)
    stdoutFd = STDOUT_FILENO;
else if (opts & pipelineDevNull)
    stdoutFd = devNullOpen(O_WRONLY);

pipeRdFd = pipeCreate(&pl->pipeFd);

pipelineBuild(pl, cmds);
pipelineExec(pl, pipeRdFd, stdoutFd, STDERR_FILENO);
safeClose(&stdoutFd);
safeClose(&pipeRdFd);
return pl;
}

struct pipeline *pipelineCreateWrite1(char **cmd, unsigned opts,
                                      char *stdoutFile)
/* like pipelineCreateWrite(), only takes a single command */
{
char **cmds[2];
cmds[0] = cmd;
cmds[1] = NULL;
return pipelineCreateWrite(cmds, opts, stdoutFile);
}

char *pipelineDesc(struct pipeline *pl)
/* Get the desciption of a pipeline for use in error messages */
{
return pl->procName;
}

int pipelineFd(struct pipeline *pl)
/* Get the file descriptor for a pipeline */
{
return pl->pipeFd;
}

FILE *pipelineFile(struct pipeline *pl)
/* Get the FILE for a pipeline */
{
if (pl->pipeFh == NULL)
    {
    /* create FILE* fon first acess */
    char *mode = (pl->options & pipelineRead) ? "r" : "w";
    pl->pipeFh = fdopen(pl->pipeFd, mode);
    if (pl->pipeFh == NULL)
        errnoAbort("fdopen failed for: %s", pl->procName);
    pl->stdioBuf = needLargeMem(FILE_BUF_SIZE);
    setvbuf(pl->pipeFh, pl->stdioBuf,  _IOFBF, FILE_BUF_SIZE);
    }
return pl->pipeFh;
}

static void closePipeline(struct pipeline *pl)
/* Close the pipe file */
{
if (pl->pipeFh != NULL)
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
else
    {
    if (close(pl->pipeFd) < 0)
        errAbort("close failed on pipeline: %s ", pl->procName);
    }
pl->pipeFd = -1;
}

void pipelineWait(struct pipeline **plPtr)
/* Wait for processes in a pl to complete and free object */
{
struct pipeline *pl = *plPtr;
if (pl != NULL)
    {
    struct plProc *proc;
    /* must close before waits for output pipeline */
    if (pl->options & pipelineWrite)
        closePipeline(pl);

    /* wait on each process in order */
    for (proc = pl->procs; proc != NULL; proc = proc->next)
        plProcWait(proc);

    /* must close after waits for input pipeline */
    if (pl->options & pipelineRead)
        closePipeline(pl);
    pipelineFree(plPtr);
    }
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

