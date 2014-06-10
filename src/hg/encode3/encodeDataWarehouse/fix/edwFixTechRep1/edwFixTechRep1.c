/* edwFixTechRep1 - Parse edwFile.tags and if there's technical_replicate or paired_end add 
 * it to edwValidFile.. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwFixTechRep1 - Parse edwFile.tags and if there's technical_replicate or paired_end add\n"
  "it to edwValidFile.\n"
  "usage:\n"
  "   edwFixTechRep1 out.sql outFileIds\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void updateOne(struct edwFile *ef, struct cgiVar *var, char *columnName, FILE *f)
/* If var is non-null write a line to f to update columnName in edwValidFile with
 * value. */
{
if (var != NULL && !sameString(var->val, "n/a"))
    {
    fprintf(f, "update edwValidFile set %s='%s' where fileId=%u;\n", columnName, var->val, ef->id);
    }
}

void edwFixTechRep1(char *outSql, char *outFileId)
/* edwFixTechRep1 - Parse edwFile.tags and if there's technical_replicate or paired_end add 
 * it to edwValidFile.. */
{
struct sqlConnection *conn = edwConnect();
char query[512];
sqlSafef(query, sizeof(query), "select * from edwFile");
struct edwFile *ef, *efList = edwFileLoadByQuery(conn, query);
FILE *fSql = mustOpen(outSql, "w");
FILE *fIds = mustOpen(outFileId, "w");
for (ef = efList; ef != NULL; ef = ef->next)
    {
    struct cgiDictionary *dict = cgiDictionaryFromEncodedString(ef->tags);
    struct cgiVar *techRep = hashFindVal(dict->hash, "technical_replicate");
    struct cgiVar *pairedEnd = hashFindVal(dict->hash, "paired_end");
    if (techRep != NULL || pairedEnd != NULL)
        fprintf(fIds, "%u\n", ef->id);
    updateOne(ef, techRep, "technicalReplicate", fSql);
    updateOne(ef, pairedEnd, "pairedEnd", fSql);
    cgiDictionaryFree(&dict);
    }
carefulClose(&fSql);
carefulClose(&fIds);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
edwFixTechRep1(argv[1], argv[2]);
return 0;
}
