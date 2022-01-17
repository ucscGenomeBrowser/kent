/* Create process pipeline as input or output files */

/* Copyright (C) 2003 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#ifndef GBPIPELINE_H
#define GBPIPELINE_H
#include <stdio.h>

/* Option flag set */
#define PIPELINE_READ     0x01 /* read from pipeline */
#define PIPELINE_WRITE    0x02 /* write to pipeline */
#define PIPELINE_APPEND   0x04 /* write to pipeline, appends to output file */
#define PIPELINE_FDOPEN   0x10 /* wrap a FILE* around pipe */

struct gbPipeline *gbPipelineCreate(char ***cmds, unsigned options,
                                    char *inFile, char *outFile);
/* Start a pipeline, either for input or output.  If inFile or outFile are
 * NULL, they are opened as the stdin/out of process.  If flags is O_APPEND,
 * it will open for append access.  Each row of cmd is a process in the
 * pipeline. */

char *gbPipelineDesc(struct gbPipeline *pipeline);
/* Get the desciption of a pipeline for use in error messages */

int gbPipelineFd(struct gbPipeline *pipeline);
/* Get the file descriptor for a pipeline */

FILE *gbPipelineFile(struct gbPipeline *pipeline);
/* Get the FILE for a pipeline */

struct gbPipeline *gbPipelineFind(int fd);
/* Find the pipeline object for a file descriptor */

void gbPipelineWait(struct gbPipeline **pipelinePtr);
/* Wait for processes in a pipeline to complete and free object */

#endif
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
