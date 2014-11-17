/* mailViaPipe - a safer and sharable sendmail utility using
 * more secure pipeline module.
 * Special note: 
 * Currently, RR never return exit or error status to the browser 
 * after sendmail, so the implementation here does not handle any 
 * error condition at all, and will return 0 regardless. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
 
#include "pipeline.h"
#include "common.h"
#include "mailViaPipe.h"

int mailViaPipe(char *toAddress, char *theSubject, char *theBody, char *fromAddress)
/* Send mail via pipeline to sendmail.  Abort if a problem. */
{
char *cmd1[] = {"/usr/sbin/sendmail", "-t", "-oi", NULL};
struct pipeline *dataPipe = pipelineOpen1(cmd1, pipelineWrite | pipelineNoAbort,
"/dev/null", NULL);
FILE *out = pipelineFile(dataPipe);
fprintf(out, "To: %s\n", toAddress);
fprintf(out, "From: %s\n", fromAddress);
fprintf(out, "Subject: %s\n", theSubject);
fprintf(out, "\n");
fprintf(out, "%s\n", theBody);
fflush(out);
return pipelineClose(&dataPipe);
}

