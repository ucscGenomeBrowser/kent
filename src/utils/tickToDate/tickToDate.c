/* tickToDate - Convert seconds since 1970 to time and date. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "tickToDate - Convert seconds since 1970 to time and date\n"
  "usage:\n"
  "   tickToDate ticks\n"
  "Use 'now' for current ticks and date\n"
  );
}

void tickToDate(char *tickString)
/* tickToDate - Convert seconds since 1970 to time and date. */
{
time_t ticks;
if (sameString(tickString, "now"))
    ticks = time(NULL);
else
    ticks = atol(tickString);
printf("%s", ctime(&ticks));
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
tickToDate(argv[1]);
return 0;
}
