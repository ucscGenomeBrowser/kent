/*    longRange --- functions to provide UI for long range interaction
 *         graph.   Functions to retrieve long range information from
 *         bedTabix format.
 */

/* Copyright (C) 2016 The Regents of the University of California 
 *  * See README in this or parent directory for licensing information. */

#include "longRange.h"

static char *longTabixAutoSqlString =
"table longTabix\n"
"\"Long Range Tabix file\"\n"
"   (\n"
"   string chrom;      \"Reference sequence chromosome or scaffold\"\n"
"   uint   chromStart; \"Start position in chromosome\"\n"
"   uint   chromEnd;   \"End position in chromosome\"\n"
"   string interactingRegion;       \"(e.g. chrX:123-456,3.14, where chrX:123-456 is the coordinate of the mate, and 3.14 is the score of the interaction)\"\n"
"   uint   id;      \"Unique Id\"\n"
"   char[1] strand;    \"+ or -\"\n"
"   )\n"
;

struct asObject *longTabixAsObj()
// Return asObject describing fields of longTabix file
{
return asParseText(longTabixAutoSqlString);
}

void longRangeCfgUi(struct cart *cart, struct trackDb *tdb, char *name, char *title, boolean boxed)
/* Complete track controls for long range interaction. */
{
char buffer[1024];
safef(buffer, sizeof buffer, "%s.%s", tdb->track, LONG_HEIGHT );
unsigned height = sqlUnsigned(cartUsualString(cart, buffer, LONG_DEFHEIGHT));
printf("<BR><b>Track height:&nbsp;</b>");
cgiMakeIntVar(buffer, height, 3);
safef(buffer, sizeof buffer, "%s.%s", tdb->track, LONG_MINSCORE);
double minScore = sqlDouble(cartUsualString(cart, buffer, LONG_DEFMINSCORE));
printf("<BR><BR><b>Minimum score:&nbsp;</b>");
cgiMakeDoubleVar(buffer, minScore, 0);
}

static char *getOther(struct bed *bed, unsigned *s, unsigned *e, double *score)
/* parse the name field of longTabix to get the other location */
{
char *otherChrom = cloneString(bed->name);
char *ptr = strchr(otherChrom, ':');
if (ptr == NULL)
    errAbort("bad longTabix bed name %s\n", bed->name);
*ptr++ = 0;
*s = atoi(ptr);
ptr = strchr(ptr, '-');
if (ptr == NULL)
    errAbort("bad longTabix bed name %s\n", bed->name);
ptr++;
*e = atoi(ptr);
ptr = strchr(ptr, ',');
if (ptr == NULL)
    errAbort("bad longTabix bed name %s\n", bed->name);
ptr++;
*score = sqlDouble(ptr);

return otherChrom;
}

struct longRange *parseLongTabix(struct bed *beds, unsigned *maxWidth, double minScore)
/* Parse longTabix format into longRange structures */
{
struct longRange *longRangeList = NULL;
for(; beds; beds=beds->next)
    {
    double score;
    unsigned otherS;
    unsigned otherE;
    char *otherChrom = getOther(beds, &otherS, &otherE, &score);
    if (score < minScore)
        continue;

    struct longRange *longRange;
    AllocVar(longRange);
    slAddHead(&longRangeList, longRange);

    unsigned otherCenter = (otherS + otherE)/2;
    unsigned otherWidth = otherE - otherS;
    unsigned thisWidth = beds->chromEnd - beds->chromStart;
    unsigned center = (beds->chromEnd + beds->chromStart) / 2;

    // don't have oriented feet at the moment
    longRange->sOrient = longRange->eOrient = 0;
    longRange->id = beds->score;
    longRange->score = score;
    longRange->name = beds->name;
    
    if (otherCenter < center)
        {
        longRange->s = otherCenter;
        longRange->sw = otherWidth;
        longRange->sChrom = otherChrom;
        longRange->e = center;
        longRange->ew = thisWidth;
        longRange->eChrom = beds->chrom;
        }
    else
        {
        longRange->s = center;
        longRange->sw = thisWidth;
        longRange->sChrom = beds->chrom;
        longRange->e = otherCenter;
        longRange->ew = otherWidth;
        longRange->eChrom = otherChrom;
        }
    unsigned longRangeWidth = longRange->e - longRange->s;
    if (longRangeWidth > *maxWidth)
        *maxWidth = longRangeWidth;
    }
return longRangeList;
}
