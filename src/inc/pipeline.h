/* pipeline.h - create a process pipeline that can be used for reading or
 * writing  */
#ifndef GBPIPELINE_H
#define GBPIPELINE_H
#include <stdio.h>
struct pipeline;

enum pipelineFd
/* pipeline default */
    {
    pipelineInheritFd = -1, /* use current stdio file */
    pipelineDevNull = -2,   /* use /dev/null for file */
    };

struct pipeline *pipelineCreateRead(char ***cmds, char *stdinFile);
/* create a read pipeline from an array of commands.  Each command is
 * an array of arguments.  Shell expansion is not done on the arguments.
 * If stdinFile is not NULL, it will be opened as the input to the pipe.
 */

struct pipeline *pipelineCreateRead1(char **cmd, char *stdinFile);
/* like pipelineCreateRead(), only takes a single command */

struct pipeline *pipelineCreateWrite(char ***cmds, char *stdoutFile);
/* create a write pipeline from an array of commands.  Each command is
 * an array of arguments.  Shell expansion is not done on the arguments.
 * If stdoutFile is not NULL, it will be opened as the output from the pipe.
 */

struct pipeline *pipelineCreateWrite1(char **cmd, char *stdoutFile);
/* like pipelineCreateWrite(), only takes a single command */

char *pipelineDesc(struct pipeline *pl);
/* Get the desciption of a pipeline for use in error messages */

int pipelineFd(struct pipeline *pl);
/* Get the file descriptor for a pipeline */

FILE *pipelineFile(struct pipeline *pl);
/* Get a FILE object wrapped around the pipeline */

void pipelineWait(struct pipeline **plPtr);
/* Wait for processes in a pipeline to complete and free object */

#endif
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
