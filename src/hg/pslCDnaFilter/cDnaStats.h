/* cDnaStats - collect statistics on alignment filtering */
#ifndef CDNASTATS_H
#define CDNASTATS_H

struct cDnaCnts
/* counts kept for alignments */
{
    unsigned queries;        /* count of queries; */
    unsigned aligns;         /* count of alignments */
    unsigned prevAligns;     /* previous aligns count, used to update queires */
};

struct cDnaStats
/* all statistics collected on cDNA filtering */
{
    struct cDnaCnts totalCnts;         /* number read */
    struct cDnaCnts keptCnts;          /* number of kept */
    struct cDnaCnts badCnts;           /* number bad that were dropped */ 
    struct cDnaCnts minQSizeCnts;      /* number below min size dropped */ 
    struct cDnaCnts weirdOverCnts;     /* number of weird overlapping PSLs */
    struct cDnaCnts weirdKeptCnts;     /* weird overlapping PSLs not dropped  */
    struct cDnaCnts overlapCnts;       /* number dropped due to overlap */
    struct cDnaCnts minIdCnts;         /* number dropped less that min id */
    struct cDnaCnts minCoverCnts;      /* number dropped less that min cover */
    struct cDnaCnts minAlnSizeCnts;    /* number dropped due minAlnSize */
    struct cDnaCnts minNonRepSizeCnts; /* number dropped due minNonRepSize */
    struct cDnaCnts maxRepMatchCnts;   /* number dropped due maxRepMatch */
    struct cDnaCnts maxAlignsCnts;     /* number dropped due to over max */
    struct cDnaCnts localBestCnts;     /* number dropped due to local near best */
    struct cDnaCnts globalBestCnts;    /* number dropped due to global near best */
    struct cDnaCnts minSpanCnts;       /* number dropped due to under minSpan */
};

void cDnaStatsUpdate(struct cDnaStats *stats);
/* update counters after processing a one cDNAs worth of alignments */

void cDnaStatsPrint(struct cDnaStats *stats, int level);
/* print stats if at verbose level or above */

#endif
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

