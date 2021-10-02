/* ensFace - stuff to manage interface to Ensembl web site. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "dystring.h"
#include "ensFace.h"
#include "hCommon.h"
#include "hdb.h"


struct stringPair
/* A pair of strings. */
   {
   char *a;	/* One string. */
   char *b;	/* The other string */
   };


char *ensOrgNameFromScientificName(char *scientificName)
/* Convert from ucsc to Ensembl organism name.
 * This is scientific name, with underscore replacing spaces
 * Caller must free returned string */
{
    char *p;
    char *res;
    if (scientificName == NULL) 
        return NULL;
    if (sameWord(scientificName, "Takifugu rubripes"))
        {
        /* special case for fugu, whose scientific name
         * has been changed to Takifugu, but not at ensembl */
        return "Fugu_rubripes";
        }
    if (sameWord(scientificName, "Pongo pygmaeus abelii"))
        {
        /* special case for Orangutan, different form of the same
         * scientific name */
        return "Pongo_pygmaeus";
        }
    if (sameWord(scientificName, "Canis lupus familiaris"))
        {
        /* special case for Dog, different form of the same
         * scientific name */
        return "Canis_familiaris";
        }
    if (sameWord(scientificName, "Gorilla gorilla gorilla"))
        {
        /* special case for Dog, different form of the same
         * scientific name */
        return "Gorilla_gorilla";
        }
    if (sameWord(scientificName, "Spermophilus tridecemlineatus"))
        {
        /* special case for squirrel, whose scientific name has been
         * changed from Spermophilusv to Ictidomys at ensembl */
        return "Ictidomys_tridecemlineatus";
        }

    /* replace spaces with underscores, assume max two spaces
     * (species and sub-species).  */
    res = cloneString(scientificName);
    if ((p = index(res, ' ')) != NULL)
        *p = '_';
    if ((p = rindex(res, ' ')) != NULL)
        *p = '_';
    return res;
}

static char *ucscToEnsembl(char *database, char *chrom)
/* if table UCSC_TO_ENSEMBL exists in the given database, return the
   Ensembl name for this chrom */
{
static char ensemblName[256];
struct sqlConnection *conn = hAllocConn(database);
ensemblName[0] = 0;
if (sqlTableExists(conn, UCSC_TO_ENSEMBL))
    {
    char query[256];
    sqlSafef(query, ArraySize(query), "select ensembl from %s where ucsc='%s'",
	UCSC_TO_ENSEMBL, chrom);
    (void) sqlQuickQuery(conn,query,ensemblName,ArraySize(ensemblName));
    }
return ensemblName;
}

static int liftToEnsembl(char *database, char *chrom)
/* if table ENSEMBL_LIFT exists in the given database, return the
   offset for this chrom, else return zero */
{
int offset = 0;
struct sqlConnection *conn = hAllocConn(database);

if (sqlTableExists(conn, ENSEMBL_LIFT))
    {
    char query[256];
    sqlSafef(query, ArraySize(query), "select offset from %s where chrom='%s'",
	ENSEMBL_LIFT, chrom);
    offset = sqlQuickNum(conn,query); // returns 0 for failed query
    }
return offset;
}

struct dyString *ensContigViewUrl(
char *database, char *ensOrg, char *chrom, int chromSize,
                            int winStart, int winEnd, char *archive)
/* Return a URL that will take you to ensembl's contig view. */
/* Not using chromSize.  archive is possibly a date reference */
{
struct dyString *dy = dyStringNew(0);
char *chrName;
char *ensemblName = ucscToEnsembl(database, chrom);
int ensemblLift = 0;
int start = winStart;
int end = winEnd;

if (isNotEmpty(ensemblName))
    {
    chrName = ensemblName;
    ensemblLift = liftToEnsembl(database, ensemblName);
    start += ensemblLift;
    end += ensemblLift;
    }
else if (startsWith("scaffold", chrom))
    chrName = chrom;
else
    chrName = skipChr(chrom);

if (archive)
    if (sameWord(archive,"ncbi36"))
	{
    dyStringPrintf(dy, 
	   "http://%s.ensembl.org/%s/contigview?chr=%s&start=%d&end=%d",
		    archive, ensOrg, chrName, start, end);
	}
    else
	{
    dyStringPrintf(dy, 
	   "http://%s.archive.ensembl.org/%s/contigview?chr=%s&start=%d&end=%d",
		    archive, ensOrg, chrName, start, end);
	}
else
    if (sameWord(database, "hg19"))
        // grch37 now has a special status within Ensembl, with a separate server that gets special updates
        dyStringPrintf(dy, 
               "http://grch37.ensembl.org/Homo_sapiens/contigview?chr=%s&start=%d&end=%d", chrName, start, end);
    else
        dyStringPrintf(dy, 
               "http://www.ensembl.org/%s/contigview?chr=%s&start=%d&end=%d", ensOrg, chrName, start, end);
return dy;
}
