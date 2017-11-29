/*    longRange --- functions to provide UI for long range interaction
 *         graph.   Functions to retrieve long range information from
 *         bedTabix format.
 */

/* Copyright (C) 2016 The Regents of the University of California 
 *  * See README in this or parent directory for licensing information. */

#include "longRange.h"

void longRangeCfgUi(struct cart *cart, struct trackDb *tdb, char *name, char *title, boolean boxed)
/* Complete track controls for long range interaction. */
{
char buffer[1024];
int min, max, deflt, current;
cartTdbFetchMinMaxPixels(cart, tdb, LONG_MINHEIGHT, LONG_MAXHEIGHT, atoi(LONG_DEFHEIGHT),
                                &min, &max, &deflt, &current);
safef(buffer, sizeof buffer, "%s.%s", tdb->track, LONG_HEIGHT);
printf("<br><b>Track height:&nbsp;</b>");
cgiMakeIntVar(buffer, current, 3);
printf("&nbsp;<span>pixels&nbsp;(range: %d to %d, default: %d)<span>", 
        min, max, deflt);

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
*maxWidth = 1;
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
    
    if (sameString(beds->chrom, otherChrom) && (otherCenter < center))
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
    if (sameString(longRange->eChrom,longRange->sChrom) && ( longRangeWidth > *maxWidth))
        *maxWidth = longRangeWidth;
    }
return longRangeList;
}
