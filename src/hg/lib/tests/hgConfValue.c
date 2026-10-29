/* hgConfValue - print hg.conf settings the way the browser reads them.
 *
 * A script that wants a setting out of hg.conf has to follow includes to find it, and an
 * include may be nested, and its path is relative to the file that names it, not to where
 * the script runs.  makeHgCentralRegress.sh followed one level, relative to the current
 * directory, and failed on a ~/.hg.conf whose central settings were three includes down.
 * This asks hgConfig.c instead, so a script gets the answer a CGI would get.
 *
 * Reads HGDB_CONF, then ~/.hg.conf, as every other program does.  Prints one line per name,
 * empty when the setting is absent.  refs #38391 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hgConfig.h"

static void usage()
/* Explain usage and exit. */
{
errAbort("hgConfValue - print hg.conf settings, following includes as the browser does\n"
         "usage:\n"
         "   hgConfValue name [name ...]\n"
         "Reads HGDB_CONF, or ~/.hg.conf when that is not set.  Prints one line per name,\n"
         "empty when the setting is absent.");
}

int main(int argc, char *argv[])
{
if (argc < 2)
    usage();
int i;
for (i = 1; i < argc; i++)
    printf("%s\n", emptyForNull(cfgOption(argv[i])));
return 0;
}
