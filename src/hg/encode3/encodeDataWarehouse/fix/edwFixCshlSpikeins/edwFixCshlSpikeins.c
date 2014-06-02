/* edwFixCshlSpikeins - Fix ENCODE2 spike in BAMs from wgEncodeCshl. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwFixCshlSpikeins - Fix ENCODE2 spike in BAMs from wgEncodeCshl\n"
  "usage:\n"
  "   edwFixCshlSpikeins out.sql out.csh out.tab\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void edwFixCshlSpikeins(char *outSql, char *outCsh, char *outTab)
/* edwFixCshlSpikeins - Fix ENCODE2 spike in BAMs from wgEncodeCshl. */
{
struct sqlConnection *conn = edwConnect();
char query[512];
FILE *sqlF = mustOpen(outSql, "w");
FILE *cshF = mustOpen(outCsh, "w");
FILE *tabF = mustOpen(outTab, "w");

fprintf(sqlF, "# Updating .spikeins.CL.bam.gz names to just .bam\n");
sqlSafef(query, sizeof(query), 
    "select * from edwFile where submitFileName like '%%/wgEncodeCshlLongRnaSeq%%.spikeins.CL.bam.gz'");
struct edwFile *ef, *efList = edwFileLoadByQuery(conn, query);
for (ef = efList; ef != NULL; ef = ef->next)
    {
    char *oldName = ef->submitFileName;
    char *newName = cloneString(oldName);
    char *firstDot = strchr(newName, '.');
    *firstDot = 0;
    fprintf(sqlF, "update edwFile set submitFileName='%s.bam' where id=%u;\n", newName, ef->id);
    fprintf(tabF, "%s\t%s.bam\n", oldName, newName);
    }
fprintf(sqlF, "\n");

fprintf(sqlF, "# Updating formats and names of .spikeins to be bam.\n");
fprintf(cshF, "#!/bin/tcsh -efx\n");
sqlSafef(query, sizeof(query), 
    "select * from edwFile where submitFileName like '%%/wgEncodeCshlLongRnaSeq%%Spikeins%%.spikeins'");
efList = edwFileLoadByQuery(conn, query);
for (ef = efList; ef != NULL; ef = ef->next)
    {
    char *oldName = ef->submitFileName;
    char *newName = cloneString(oldName);
    char *firstDot = strchr(newName, '.');
    *firstDot = 0;
    fprintf(sqlF, "update edwFile set submitFileName='%s.bam' where id=%u;\n", newName, ef->id);
    fprintf(cshF, "edwChangeFormat bam %u\n", ef->id);
    fprintf(tabF, "%s\t%s.bam\n", oldName, newName);
    }
fprintf(sqlF, "\n");
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
edwFixCshlSpikeins(argv[1], argv[2], argv[3]);
return 0;
}

