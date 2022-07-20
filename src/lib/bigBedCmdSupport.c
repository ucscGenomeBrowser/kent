/* bigBedCmdSupport - functions to support writing bigBed related commands. */

/* Copyright (C) 2022 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "bigBedCmdSupport.h"

void bigBedCmdOutputHeader(struct bbiFile *bbi, FILE *f)
/* output a header from the autoSql in the file */
{
char *asText = bigBedAutoSqlText(bbi);
if (asText == NULL)
    errAbort("bigBed files does not contain an autoSql schema");
struct asObject *asObj = asParseText(asText);
char sep = '#';
for (struct asColumn *asCol = asObj->columnList; asCol != NULL; asCol = asCol->next)
    {
    fputc(sep, f);
    fputs(asCol->name, f);
    sep = '\t';
    }
fputc('\n', f);
}
