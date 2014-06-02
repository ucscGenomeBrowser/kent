/* edwFixRevoked - Mark as deprecated files that are revoked in ENCODE2. */

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
  "edwFixRevoked - Mark as deprecated files that are revoked in ENCODE2\n"
  "usage:\n"
  "   edwFixRevoked database file\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void edwFixRevoked(char *database, char *inFile)
/* edwFixRevoked - Mark as deprecated files that are revoked in ENCODE2. */
/* inFile is in format:
 *    metaVariable objStatus revoked [- reason]
 *    metaObject name */
{
struct sqlConnection *conn = edwConnect();
struct lineFile *lf = lineFileOpen(inFile, TRUE);
char *line;
char *defaultReason = "Revoked in ENCODE2";
char *reason = defaultReason;
while (lineFileNextReal(lf, &line))
    {
    if (startsWithWord("metaVariable", line))
        {
	char *pattern = "metaVariable objStatus revoked";
	if (startsWithWord(pattern, line))
	    {
	    reason = skipLeadingSpaces(line + strlen(pattern));
	    if (isEmpty(reason))
	        reason = defaultReason;
	    else
	        {
		if (reason[0] == '-')
		   reason = skipLeadingSpaces(reason + 1);
		reason = cloneString(reason);
		}
	    }
	else
	    errAbort("??? %s\n", line);
	}
    else if (startsWithWord("metaObject", line))
        {
	char *row[3];
	int wordCount = chopLine(line, row);
	if (wordCount != 2)
	    errAbort("Strange metaobject line %d of %s\n", lf->lineIx, lf->fileName);
	char *prefix = row[1];
	if (!startsWith("wgEncode", prefix))
	    errAbort("Strange object line %d of %s\n", lf->lineIx, lf->fileName);
	char query[512];
	sqlSafef(query, sizeof(query), 
	    "select * from edwFile where submitFileName like '%s/%%/%s%%'", database, prefix);
	struct edwFile *ef, *efList = edwFileLoadByQuery(conn, query);
	printf("# %s %s\n", prefix, reason);
	for (ef = efList; ef != NULL; ef = ef->next)
	    {
	    long long id = ef->id;
	    printf("update edwFile set deprecated='%s' where id=%lld;\n", reason, id);
	    }
	}
    else
        errAbort("Unrecognized first word in %s\n", line);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
edwFixRevoked(argv[1], argv[2]);
return 0;
}
