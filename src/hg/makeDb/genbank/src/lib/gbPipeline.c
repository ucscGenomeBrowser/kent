/* Create process pipeline as input or output files */

/* Copyright (C) 2004 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "gbPipeline.h"
#include "common.h"
#include "dystring.h"
#include "errAbort.h"
#include "portable.h"
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>

/* FIXME: switch to using pipeline.h */

struct gbProc
/* A single process in a pipeline */
{
    struct gbProc *next;   /* order list of processes */
    char **cmd;            /* null-terminated command for this process */
    pid_t  pid;            /* pid for process */
};

struct gbPipeline
/* Object for a process pipeline and associated open file */
{
    struct gbPipeline *next;
    struct gbProc *procs;      /* list of processes */
    char *procName;            /* name to use in error messages. */
    int pipeFd;                /* fd of pipe to/from process, -1 if none */
    unsigned options;          /* options */
    FILE* pipeFh;              /* optional stdio around pipe */
    char* stdioBuf;            /* optional stdio buffer */
};

/* file buffer size */
#define FILE_BUF_SIZE 64*1024

/* table of all currently open processes */
static struct gbPipeline* gActivePipelines = NULL;

static void safeClose(int *fdPtr)
/* do close with error checking; doesn't check for NULL on purpose */
{
int fd = *fdPtr;
if (close(fd) < 0)
    errnoAbort("close failed on fd %d", fd);
*fdPtr = -1;
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

static struct gbProc* gbProcNew(char **cmd)
/* create a new gbProc object for a command. */
{
int i, cmdLen = 0;
struct gbProc* proc;
AllocVar(proc);

for (i = 0; cmd[i] != NULL; i++)
    cmdLen++;
proc->cmd = needMem((cmdLen+1)*sizeof(char*));

for (i = 0; i < cmdLen; i++)
    proc->cmd[i] = cloneString(cmd[i]);
proc->cmd[cmdLen] = NULL;
return proc;
}

static void gbProcFree(struct gbProc *proc)
/* free a gbProc object. */
{
int i;
for (i = 0; proc->cmd[i] != NULL; i++)
    free(proc->cmd[i]);
free(proc->cmd);
free(proc);
}

static struct gbPipeline* gbPipelineNew(char ***cmds, unsigned options)
/* create a new gbPipeline object */
{
struct gbPipeline *pipeline;
AllocVar(pipeline);
pipeline->pipeFd = -1;
pipeline->options = options;
slAddHead(&gActivePipelines, pipeline);
pipeline->procName = joinCmds(cmds);

return pipeline;
}

static void gbPipelineFree(struct gbPipeline **pipelinePtr)
/* free a gbPipeline object, removing from open list */
{
struct gbPipeline *pipeline = *pipelinePtr;
if (pipeline != NULL)
    {
    struct gbProc *proc = pipeline->procs;
    while (proc != NULL)
        {
        struct gbProc *delProc = proc;
        proc = proc->next;
        gbProcFree(delProc);
        }
    slRemoveEl(&gActivePipelines, pipeline);
    freez(&pipeline->procName);
    freez(&pipeline->stdioBuf);
    freez(pipelinePtr);
    }
}

static int execProc(struct gbProc* proc, int stdinFd, int stdoutFd)
/* Start one process in a pipeline, stdinFd must be specified.  If stdoutFd is
 * >= 0 it is used as stdout, otherwise a pipe is created and the other end
 * will be returned.  Returns -1 if a output pipe was not created.
 */
{
int stdoutPipe[2], fd;

if (stdoutFd < 0)
    {
    if (pipe(stdoutPipe) < 0)
        errnoAbort("can't create pipe");
    }
if ((proc->pid = fork()) < 0)
    errnoAbort("can't fork");
if (proc->pid == 0)
    {
    /* child */
    if (dup2(stdinFd, STDIN_FILENO) < 0)
        errnoAbort("can't dup2 pipe");
    if (stdoutFd < 0)
        {
        safeClose(&stdoutPipe[0]);
        stdoutFd = stdoutPipe[1];
        }
    if (dup2(stdoutFd, STDOUT_FILENO) < 0)
        errnoAbort("can't dup2 pipe");
    for (fd = STDERR_FILENO+1; fd < 64; fd++)
        close(fd);
    execvp(proc->cmd[0], proc->cmd);
    errnoAbort("exec failed: %s", proc->cmd[0]);
    }
else
    {
    /* parent */
    if (stdoutFd < 0)
        safeClose(&stdoutPipe[1]);
    }
/* return output pipe if we created it */
return (stdoutFd >= 0) ? -1 : stdoutPipe[0];
}

static void waitProc(struct gbPipeline *pipeline, struct gbProc* proc)
/* wait for a process in a pipeline */
{
int status;
if (waitpid(proc->pid, &status, 0) < 0)
    errnoAbort("process lost for: %s in %s", joinCmd(proc->cmd),
               pipeline->procName);
if (WIFSIGNALED(status))
    errAbort("process terminated on signal %d: %s in %s",
             WTERMSIG(status), joinCmd(proc->cmd), pipeline->procName);
if (WEXITSTATUS(status) != 0)
    errAbort("process exited with %d: %s in %s",
             WEXITSTATUS(status), joinCmd(proc->cmd), pipeline->procName);
proc->pid = -1;
}

static void execPipeline(struct gbPipeline* pipeline, int stdinFd, int stdoutFd)
/* Start all processes in a pipeline, stdinFd and stdoutFd are the ends of
 * the pipe.
 */
{
struct gbProc *proc;
int prevStdoutFd = -1, newStdoutFd;
for (proc = pipeline->procs; proc != NULL; proc = proc->next)
    {
    int useStdinFd = (prevStdoutFd < 0) ? stdinFd : prevStdoutFd;
    int useStdoutFd = (proc->next == NULL) ? stdoutFd : -1;
    /* note: fd is returned only if was created by execProc */
    newStdoutFd = execProc(proc, useStdinFd, useStdoutFd);
    if (prevStdoutFd >= 0)
        safeClose(&prevStdoutFd);
    prevStdoutFd = newStdoutFd;
    }
}

static struct gbProc* pipelineParse(char ***cmds)
/* parse a pipeline into a list of gbProc objects.  Doesn't start processes */
{
struct gbProc *procs = NULL;
int iCmd;

for(iCmd = 0; cmds[iCmd] != NULL; iCmd++)
    {
    struct gbProc *proc = gbProcNew(cmds[iCmd]);
    slAddTail(&procs, proc);
    }
if (procs == NULL)
    errAbort("no commands in pipeline");
return procs;
}

struct gbPipeline *gbPipelineCreate(char ***cmds, unsigned options,
                                    char *inFile, char *outFile)
/* Start a pipeline, either for input or output.  If inFile or outFile are
 * NULL, they are opened as the stdin/out of process.  If flags is O_APPEND,
 * it will open for append access.  Each row of cmd is a process in the
 * pipeline. */
{
struct gbPipeline *pipeline;
int pipeFds[2];
unsigned flags = 0;
char *mode = NULL;
if (options & PIPELINE_READ)
    {
    flags = O_RDONLY;
    mode = "r";
    }
else if (options & PIPELINE_WRITE)
    {
    flags = (O_WRONLY|O_CREAT);
    mode = "w";
    }
else if (options& PIPELINE_APPEND)
    {
    flags = (O_APPEND|O_WRONLY|O_CREAT);
    mode = "a";
    }
else
    errAbort("invalid gbPipelineCreate options");

pipeline = gbPipelineNew(cmds, options);
pipeline->procs = pipelineParse(cmds);

if (pipe(pipeFds) < 0)
    errnoAbort("can't create pipe");
if (options & PIPELINE_READ)
    {
    char *inFileName = (inFile != NULL) ? inFile : "/dev/null";
    int stdinFd;
    if (outFile != NULL)
        errAbort("can't specify outFile with read access");
    stdinFd = open(inFileName, O_RDONLY);
    if (stdinFd < 0)
        errnoAbort("can't open for read access: %s", inFileName);
    execPipeline(pipeline, stdinFd, pipeFds[1]);
    pipeline->pipeFd = pipeFds[0];
    safeClose(&stdinFd);
    safeClose(&pipeFds[1]);
    }
else
    {
    char *outFileName = (outFile != NULL) ? outFile : "/dev/null";
    int stdoutFd;
    if (inFile != NULL)
        errAbort("can't specify inFile with write/append access");
    stdoutFd = open(outFileName, flags,
                    S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
    if (stdoutFd < 0)
        errnoAbort("can't open for write access: %s", outFileName);
    execPipeline(pipeline, pipeFds[0], stdoutFd);
    pipeline->pipeFd = pipeFds[1];
    safeClose(&stdoutFd);
    safeClose(&pipeFds[0]);
    }

if (options & PIPELINE_FDOPEN)
    {
    pipeline->pipeFh = fdopen(pipeline->pipeFd, mode);
    if (pipeline->pipeFh == NULL)
        errnoAbort("fdopen failed for: %s", pipeline->procName);
    pipeline->stdioBuf = needLargeMem(FILE_BUF_SIZE);
    setvbuf(pipeline->pipeFh, pipeline->stdioBuf,  _IOFBF, FILE_BUF_SIZE);
    }

return pipeline;
}

char *gbPipelineDesc(struct gbPipeline *pipeline)
/* Get the desciption of a pipeline for use in error messages */
{
return pipeline->procName;
}

int gbPipelineFd(struct gbPipeline *pipeline)
/* Get the file descriptor for a pipeline */
{
return pipeline->pipeFd;
}

FILE *gbPipelineFile(struct gbPipeline *pipeline)
/* Get the FILE for a pipeline */
{
assert(pipeline->pipeFh != NULL);
return pipeline->pipeFh;
}

struct gbPipeline *gbPipelineFind(int fd)
/* Find the pipeline object for a file descriptor */
{
struct gbPipeline *pipeline;
for (pipeline = gActivePipelines; pipeline != NULL; pipeline = pipeline->next)
    {
    if (pipeline->pipeFd == fd)
        return pipeline;
    }
return NULL;
}

static void closePipe(struct gbPipeline *pipeline)
/* Close the pipe file */
{
if (pipeline->pipeFh != NULL)
    {
    if (pipeline->options & PIPELINE_WRITE)
        {
        fflush(pipeline->pipeFh);
        if (ferror(pipeline->pipeFh))
            errAbort("write failed to pipeline: %s ", pipeline->procName);
        }
    if (fclose(pipeline->pipeFh) == EOF)
        errAbort("close failed on pipeline: %s ", pipeline->procName);
    pipeline->pipeFh = NULL;
    }
else
    {
    if (close(pipeline->pipeFd) < 0)
        errAbort("close failed on pipeline: %s ", pipeline->procName);
    }
}

void gbPipelineWait(struct gbPipeline **pipelinePtr)
/* Wait for processes in a pipeline to complete and free object */
{
struct gbPipeline *pipeline = *pipelinePtr;
if (pipeline != NULL)
    {
    struct gbProc *proc;
    /* must close before wait for output pipeline */
    if (pipeline->options & PIPELINE_WRITE)
        closePipe(pipeline);

    for (proc = pipeline->procs; proc != NULL; proc = proc->next)
        waitProc(pipeline, proc);

    /* must close after wait for input pipeline */
    if (pipeline->options & PIPELINE_READ)
        closePipe(pipeline);
    gbPipelineFree(pipelinePtr);
    }
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

