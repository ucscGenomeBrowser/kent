/* genbank.c - Various functions for dealing with genbank data */
#include "common.h"
#include "genbank.h"

static char *JOIN_PREFIX = "join(";
static char *COMPLEMENT_PREFIX = "complement(";

static boolean convertCoord(char *numStr, unsigned *coord)
/* convert an CDS cooordinate, return false if invalid */
{
char *endPtr;
*coord = strtoul(numStr, &endPtr, 10);
return ((*numStr != '\0') && (*endPtr == '\0'));
}

static boolean parseStartCds(char *startBuf, unsigned *cdsStart)
/* parse a starting CDS coordinate */
{
if (startBuf[0] == '<')
    startBuf++;
if (!convertCoord(startBuf, cdsStart))
    return FALSE;
cdsStart--;  /* convert to zero-based */
return TRUE;
}

static boolean parseEndCds(char *endBuf, unsigned *cdsEnd)
/* parse an ending CDS coordinate */
{
if (endBuf[0] == '>')
    endBuf++;
return convertCoord(endBuf, cdsEnd);
}

static boolean parseRange(char *cdsBuf, unsigned *cdsStart, unsigned *cdsEnd)
/* parse a cds range in the for 221..617 */
{
char *p1;

/* find .. */
p1 = strchr(cdsBuf, '.'); 
if ((p1 == NULL) || (p1[1] != '.'))
    return FALSE; /* no .. */

*p1 = '\0';
p1 += 2;
if (!parseStartCds(cdsBuf, cdsStart))
    return FALSE;
return parseEndCds(p1, cdsEnd);
}

static boolean parseJoin(char *cdsBuf, unsigned *cdsStart, unsigned *cdsEnd)
/* parse a join cds, taking the first and last coordinates in range */
{
int len = strlen(cdsBuf);
char *startPtr, *endPtr, *p;

/* Pull out last number in join */
if (cdsBuf[len-1] != ')')
    return FALSE;  /* no close paren */
cdsBuf[len-1] = '\0';
endPtr = strrchr(cdsBuf, '.');
if (endPtr == NULL)
    return FALSE;  /* no preceeding dot */
endPtr++;

/* Pull out first number */
startPtr = cdsBuf+strlen(JOIN_PREFIX);
p = strchr(startPtr, '.');
if (p == NULL)
    return FALSE; /* no dot after number */
*p = '\0';

if (!parseStartCds(startPtr, cdsStart))
    return FALSE;  /* invalid start */
return parseEndCds(endPtr, cdsEnd);
}

static boolean parseComplement(char *cdsBuf, unsigned *cdsStart,
                               unsigned *cdsEnd)
/* parse a complement cds, perhaps recursively parsing a join */
{
int len = strlen(cdsBuf);
char *p1 = cdsBuf+strlen(COMPLEMENT_PREFIX);

if (cdsBuf[len-1] != ')')
    return FALSE;  /* no closing paren */
cdsBuf[len-1] = '\0';

if (startsWith(JOIN_PREFIX, p1))
    return parseJoin(p1, cdsStart, cdsEnd);
else
    return parseRange(p1, cdsStart, cdsEnd);
}

boolean genbankParseCds(char *cdsStr, unsigned *cdsStart, unsigned *cdsEnd)
/* Parse a genbank CDS, returning TRUE if it can be successfuly parsed,
 * FALSE if there are problems.  If a join() is specified, the first and last
 * coordinates are used for the CDS.  Incomplete CDS specifications will still
 * return the start or end.
 */
{
char *cdsBuf = alloca(strlen(cdsStr)+1);
strcpy(cdsBuf, cdsStr); /* copy that can be modified */

if (startsWith(JOIN_PREFIX, cdsBuf))
    return parseJoin(cdsBuf, cdsStart, cdsEnd);
else if (startsWith(COMPLEMENT_PREFIX, cdsBuf))
    return parseComplement(cdsBuf, cdsStart, cdsEnd);
else
    return parseRange(cdsBuf, cdsStart, cdsEnd);
}

