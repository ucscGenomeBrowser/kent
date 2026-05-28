/* mailViaPipe - a safer and sharable sendmail utility using
 * more secure pipeline module.
 * Special note: 
 * Currently, RR never return exit or error status to the browser 
 * after sendmail, so the implementation here does not handle any 
 * error condition at all, and will return 0 regardless. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
 
#include "pipeline.h"
#include "common.h"
#include "mailViaPipe.h"

int mailViaPipe(char *toAddress, char *theSubject, char *theBody, char *fromAddress)
/* Send mail via pipeline to sendmail.  Abort if a problem. */
{
char *cmd1[] = {"/usr/sbin/sendmail", "-t", "-oi", NULL};
struct pipeline *dataPipe = pipelineOpen1(cmd1, pipelineWrite | pipelineNoAbort,
                                          "/dev/null", NULL, 0);
FILE *out = pipelineFile(dataPipe);
fprintf(out, "To: %s\n", toAddress);
fprintf(out, "From: %s\n", fromAddress);
fprintf(out, "Subject: %s\n", theSubject);
fprintf(out, "\n");
fprintf(out, "%s\n", theBody);
fflush(out);
return pipelineClose(&dataPipe);
}


int mailViaPipeBounce(char *toAddress, char *theSubject, char *theBody, char *fromAddress)
/* Send mail via pipeline to sendmail, use -f bounce address to sendmail. */
{
char bounceEmail[2048];
safef(bounceEmail, sizeof(bounceEmail), "%s%s%s@%s%s.e%s", "hcl", "aws", "on", "ucs", "c", "du");
char *cmd1[] = {"/usr/sbin/sendmail", "-t", "-f", bounceEmail, "-oi", NULL};
struct pipeline *dataPipe = pipelineOpen1(cmd1, pipelineWrite | pipelineNoAbort,
                                          "/dev/null", NULL, 0);
FILE *out = pipelineFile(dataPipe);
fprintf(out, "To: %s\n", toAddress);
fprintf(out, "From: %s\n", fromAddress);
fprintf(out, "Subject: %s\n", theSubject);
fprintf(out, "\n");
fprintf(out, "%s\n", theBody);
fflush(out);
return pipelineClose(&dataPipe);
}
