#include "common.h"
#include "hCommon.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "jksql.h"
#include "knownInfo.h"
#include "hdb.h"
#include "hgConfig.h"
#include "hgFind.h"
#include "hash.h"

static boolean fitFields(struct hash *hash, char *chrom, char *start, char *end,
        char retChrom[32], char retStart[32], char retEnd[32])
/* Return TRUE if chrom/start/end are in hash.
 * If so copy them to retChrom, retStart, retEnd.
 * Helper routine for findChromStartEndFields below. */
{
if (hashLookup(hash, chrom) && hashLookup(hash, start) && hashLookup(hash, end))
    {
    strcpy(retChrom, chrom);
    strcpy(retStart, start);
    strcpy(retEnd, end);
    return TRUE;
    }
else
    return FALSE;
}

boolean hFindChromStartEndFields(char *table,
        char retChrom[32], char retStart[32], char retEnd[32])
/* Given a table return the fields for selecting chromosome, start, and end.
 *  If no such fields returns FALSE. */
{
char query[256];
struct sqlResult *sr;
char **row;
struct hash *hash = newHash(5);
struct sqlConnection *conn = hAllocConn();
boolean isPos = TRUE;

/* Read table description into hash. */
sprintf(query, "describe %s", table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    hashAdd(hash, row[0], NULL);
sqlFreeResult(&sr);

/* Look for bed-style names. */
if (fitFields(hash, "chrom", "chromStart", "chromEnd", retChrom, retStart,
retEnd))
    ;
/* Look for psl-style names. */
else if (fitFields(hash, "tName", "tStart", "tEnd", retChrom, retStart, retEnd))
    ;
/* Look for gene prediction names. */
else if (fitFields(hash, "chrom", "txStart", "txEnd", retChrom, retStart,
retEnd))
    ;
/* Look for repeatMasker names. */
else if (fitFields(hash, "genoName", "genoStart", "genoEnd", retChrom, retStart,
retEnd))
    ;
else
    isPos = FALSE;
freeHash(&hash);
hFreeConn(&conn);
return isPos;
}

int main() {
	char line[256];
	char chr[256];
	char start[256];
	char end[256];

	hDefaultConnect();
	hSetDb("hg7");

	while(fgets(line, 255, stdin)) {
		if(hFindChromStartEndFields(line, chr, start, end)) {
			printf("UPDATE browserTable SET isPlaced = 1, startName = '%s', endName = '%s' WHERE tableName = '%s'\n", start, end, line);
		}
	}

	return 0;
}

