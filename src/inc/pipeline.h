/* pipeline.h - create a process pipeline that can be used for reading or
 * writing  */
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
    };

struct pipeline *pipelineOpen(char ***cmds, unsigned opts,
                              char *otherEndFile);
/* Create a pipeline from an array of commands.  Each command is an array of
 * arguments.  Shell expansion is not done on the arguments.  If pipelineRead
 * is specified, the output of the pipeline is readable from the pipeline
 * object.  If pipelineWrite is specified, the input of the pipeline is
 * writable from the pipeline object.  If otherEndFile is not NULL, it will be
 * opened as the other end of the pipeline.  Specify "/dev/null" for no input
 * or to discard output.  If otherEndFile is NULL, then either stdin or stdout
 * are inherited from the current process.
 */

struct pipeline *pipelineOpen1(char **cmd, unsigned opts,
                               char *otherEndFile);
/* like pipelineOpen(), only takes a single command */

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
