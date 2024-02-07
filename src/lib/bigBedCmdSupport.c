/* bigBedCmdSupport - functions to support writing bigBed related commands. */

/* Copyright (C) 2022 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "bigBedCmdSupport.h"

static void outputHeader(struct bbiFile *bbi, FILE *f, char startChar)
/* output a header from the autoSql in the file, startChar is '#' or 0 to omit  */
{
char *asText = bigBedAutoSqlText(bbi);
if (asText == NULL)
    errAbort("bigBed files does not contain an autoSql schema");
struct asObject *asObj = asParseText(asText);
char sep = startChar;
for (struct asColumn *asCol = asObj->columnList; asCol != NULL; asCol = asCol->next)
    {
    if (sep != '\0')
        fputc(sep, f);
    fputs(asCol->name, f);
    sep = '\t';
    }
fputc('\n', f);
}

void bigBedCmdOutputHeader(struct bbiFile *bbi, FILE *f)
/* output a autoSql-style header from the autoSql in the file */
{
outputHeader(bbi, f, '#');
}

void bigBedCmdOutputTsvHeader(struct bbiFile *bbi, FILE *f)
/* output a TSV-style header from the autoSql in the file */
{
outputHeader(bbi, f, '\0');
}
