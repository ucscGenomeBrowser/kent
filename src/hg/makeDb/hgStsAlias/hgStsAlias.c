/* hgStsAlias - Make table of STS aliases. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "hgRelate.h"
#include "stsAlias.h"
#include "stsMarker.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgStsAlias - Make table of STS aliases\n"
  "usage:\n"
  "   hgStsAlias database file.alias\n");
}

struct hash *makeTrueHash(struct sqlConnection *conn)
/* Make up hash of TRUE names. */
{
struct hash *hash = newHash(0);
struct sqlResult *sr;
char **row;
struct stsMarker marker;

sr = sqlGetResult(conn, NOSQLINJ "select * from stsMarker");
while ((row = sqlNextRow(sr)) != NULL)
    {
    stsMarkerStaticLoad(row, &marker);
    hashAdd(hash, marker.name, NULL);
    }
sqlFreeResult(&sr);
return hash;
}

void hgStsAlias(char *database, char *inFile)
/* hgStsAlias - Make table of STS aliases. */
{
struct lineFile *lf = lineFileOpen(inFile, TRUE);
char *words[16],*parts[64];
int partCount, wordCount;
char *table = "stsAlias";
struct sqlConnection *conn = sqlConnect(database);
FILE *f = hgCreateTabFile(".", table);
struct hash *trueHash = makeTrueHash(conn);
int i;
char *alias, *trueName;
int aliasCount = 0;

while ((wordCount = lineFileChop(lf, words)) != 0)
    {
    trueName = NULL;
    if (wordCount != 2)
        {
	static boolean warned = FALSE;
	if (!warned)
	    {
	    warn("Got %d words line %d of %s, skipping", wordCount, lf->lineIx, 
		lf->fileName);
	    warn("There may be other lines like this as well");
	    warned = TRUE;
	    }
	continue;
	}
    lineFileExpectWords(lf, 2, wordCount);
    partCount = chopByChar(words[1], ';', parts, ArraySize(parts));
    if (partCount >= ArraySize(parts))
        errAbort("Too many aliases line %d of %s\n", lf->lineIx, lf->fileName);

    /* Figure out which one we actually have a name for. */
    for (i=0; i<partCount; ++i)
        {
	alias = parts[i];
	if (hashLookup(trueHash, alias))
	    {
	    trueName = alias;
	    break;
	    }
	}

    /* If we have a true name then write out alias/trueName pairs. */
    if (trueName != NULL)
	{
	for (i=0; i<partCount; ++i)
	    {
	    alias = parts[i];
	    if (alias != trueName)
		{
		++aliasCount;
	        fprintf(f, "%s\t%s\n", alias, trueName);
		}
	    }
	}
    }
lineFileClose(&lf);
printf("Found %d aliases in %s\n", aliasCount, inFile);
hgLoadTabFile(conn, ".", table, &f);
sqlDisconnect(&conn);
printf("Loaded table %s in database %s\n", table, database);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
hgStsAlias(argv[1], argv[2]);
return 0;
}
