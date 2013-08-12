/* pipeline.h - create a process pipeline that can be used for reading or
 * writing.  These pipeline objects don't go through the shell, so they
 * avoid many of the obscure problems associated with system() and popen().
 *
 * Read pipelines are pipelines where a program reads output written by the
 * pipeline, and write pipelines are where the program writes data to the
 * pipeline.  The type of pipeline is specified in the set of option flags
 * passed to pipelineOpen().  The file at the other end of the pipeline is
 * specified in the otherEndFile argument of pipelineOpen(), as shown here:
 *
 * pipelineRead:
 *
 *   otherEndFile --> cmd[0] --> ... --> cmd[n] --> pipelineLf() etc.
 *
 * pipelineWrite:
 *
 *   pipeLineFile() --> cmd[0] --> ... --> cmd[n] --> otherEndFile
 *
 * Specify otherEndFile as "/dev/null" for no input or no output (or to
 * discard output).  If otherEndFile is NULL, then either stdin or stdout are
 * inherited from the current process.
 * 
 * I/O to the pipeline is done by using the result of pipelineFd(),
 * pipelineFile(), or pipelineLineFile().
 *
 * An example that reads a compressed file, sorting it numerically by the 
 * first column:
 *    
 *    static char *cmd1[] = {"gzip", "-dc", NULL};
 *    static char *cmd2[] = {"sort", "-k", "1,1n", NULL};
 *    static char **cmds[] = {cmd1, cmd2, NULL};
 *    
 *    struct pipeline *pl = pipelineOpen(cmds, pipelineRead, inFilePath, stderrFd);
 *    struct lineFile *lf = pipelineLineFile(pl);
 *    char *line;
 *    
 *    while (lineFileNext(lf, &line, NULL))
 *        {
 *        ...
 *        }
 *    pipelineWait(pl);
 *    pipelineFree(&pl);
 *
 * A similar example that generates data and writes a compressed file, sorting
 * it numerically by the first column:
 *    
 *    
 *    static char *cmd1[] = {"sort", "-k", "1,1n", NULL};
 *    static char *cmd2[] = {"gzip", "-c3", NULL};
 *    static char **cmds[] = {cmd1, cmd2, NULL};
 *    
 *    struct pipeline *pl = pipelineOpen(cmds, pipelineWrite, outFilePath, stderrFd);
 *    char *line;
 *    
 *    while ((line = makeNextRow()) != NULL)
 *        fprintf(fh, "%s\n", line);
 *    
 *    pipelineWait(pl);
 *    pipelineFree(&pl);
 *
 * To append to an output file, use pipelineWrite|pipelineAppend:
 *    
 *    struct pipeline *pl = pipelineOpen(cmds, pipelineWrite|pipelineAppend, outFilePath, stderrFd);
 */
#ifndef PIPELINE_H
#define PIPELINE_H
#include <stdio.h>
struct linefile;
struct pipeline;

enum pipelineOpts
/* pipeline options bitset */
    {
    pipelineRead       = 0x01, /* read from pipeline */
    pipelineWrite      = 0x02, /* write to pipeline */
    pipelineNoAbort    = 0x04, /* don't abort if a process exits non-zero,
                                * wait will return exit code instead.
                                * Still aborts if process signals. */
    pipelineMemInput   = 0x08, /* pipeline takes input from memory (internal) */
    pipelineAppend     = 0x10, /* Append to output file (used only with pipelineWrite) */
    pipelineSigpipe    = 0x20  /* enable sigpipe in the children and don't treat
                                  as an error in the parent */
    };

struct pipeline *pipelineOpenFd(char ***cmds, unsigned opts,
                                int otherEndFd, int stderrFd);
/* Create a pipeline from an array of commands.  Each command is an array of
 * arguments.  Shell expansion is not done on the arguments.  If pipelineRead
 * is specified, the output of the pipeline is readable from the pipeline
 * object.  If pipelineWrite is specified, the input of the pipeline is
 * writable from the pipeline object. */

struct pipeline *pipelineOpen(char ***cmds, unsigned opts,
                              char *otherEndFile, char *stderrFile);
/* Create a pipeline from an array of commands.  Each command is an array of
 * arguments.  Shell expansion is not done on the arguments.  If pipelineRead
 * is specified, the output of the pipeline is readable from the pipeline
 * object.  If pipelineWrite is specified, the input of the pipeline is
 * writable from the pipeline object.  If stderrFile is NULL, stderr is inherited,
 * otherwise it is redirected to this file.
 */

void pipelineDumpCmds(char ***cmds);
/* Dump out pipeline-formatted commands to stdout for debugging. */

struct pipeline *pipelineOpenMem(char ***cmds, unsigned opts,
                                 void *otherEndBuf, size_t otherEndBufSize,
                                 int stderrFd);
/* Create a pipeline from an array of commands, with the pipeline input/output
 * in a memory buffer.  See pipeline.h for full documentation.  Currently only
 * input to a read pipeline is supported  */

struct pipeline *pipelineOpenFd1(char **cmd, unsigned opts,
                                 int otherEndFd, int stderrFd);
/* like pipelineOpenFd(), only takes a single command */

struct pipeline *pipelineOpen1(char **cmd, unsigned opts,
                               char *otherEndFile, char *stderrFile);
/* like pipelineOpen(), only takes a single command */

struct pipeline *pipelineOpenMem1(char **cmd, unsigned opts,
                                  void *otherEndBuf, size_t otherEndBufSize,
                                  int stderrFd);
/* like pipelineOpenMem(), only takes a single command */

char *pipelineDesc(struct pipeline *pl);
/* Get the desciption of a pipeline for use in error messages */

int pipelineFd(struct pipeline *pl);
/* Get the file descriptor for a pipeline */

FILE *pipelineFile(struct pipeline *pl);
/* Get a FILE object wrapped around the pipeline.  Do not close the FILE, is
 * owned by the pipeline object.  A FILE is created on first call to this
 * function.  Subsequent calls return the same FILE.*/

struct lineFile *pipelineLineFile(struct pipeline *pl);
/* Get a lineFile object wrapped around the pipeline.  Do not close the
 * lineFile, is owned by the pipeline object.  A lineFile is created on first
 * call to this function.  Subsequent calls return the same object.*/

int pipelineWait(struct pipeline *pl);
/* Wait for processes in a pipeline to complete; normally aborts if any
 * process exists non-zero.  If pipelineNoAbort was specified, return the exit
 * code of the first process exit non-zero, or zero if none failed. */

void pipelineFree(struct pipeline **plPtr);
/* free a pipeline object */

#endif
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
