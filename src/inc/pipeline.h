/* pipeline.h - create a process pipeline that can be used for reading or
 * writing  */
#ifndef GBPIPELINE_H
#define GBPIPELINE_H
#include <stdio.h>
struct pipeline;

enum pipelineOpts
/* pipeline options bitset */
    {
    pipelineInheritFd = 0x01, /* use current stdin as input of read pipeline
                               * or stdout as output of write pipeline.  */
    pipelineDevNull =   0x02, /* use /dev/null input or output file */
    };

struct pipeline *pipelineCreateRead(char ***cmds, unsigned opts,
                                    char *stdinFile);
/* create a read pipeline from an array of commands.  Each command is
 * an array of arguments.  Shell expansion is not done on the arguments.
 * If stdinFile is not NULL, it will be opened as the input to the pipe,
 * otherwise pipelineOpts is used to determine the input file.
 */

struct pipeline *pipelineCreateRead1(char **cmd, unsigned opts,
                                     char *stdinFile);
/* like pipelineCreateRead(), only takes a single command */

struct pipeline *pipelineCreateWrite(char ***cmds, unsigned opts,
                                     char *stdoutFile);
/* create a write pipeline from an array of commands.  Each command is
 * an array of arguments.  Shell expansion is not done on the arguments.
 * If stdoutFile is not NULL, it will be opened as the output from the pipe.
 * otherwise pipelineOpts is used to determine the output file.
 */

struct pipeline *pipelineCreateWrite1(char **cmd, unsigned opts,
                                      char *stdoutFile);
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
