/* Copyright (C) 2003 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */


struct oneStat
/* One type of stat. */
    {
    int features;	/* Number of this type of feature. */
    int hits;		/* Number of hits to this type of features. */
    int basesTotal;	/* Total bases in all features. */
    int basesPainted;	/* Total bases hit. */
    double cumIdRatio;	/* Identity ratio times basesTotal. */
    };

struct blatStats 
/* Blat summary statistics. */
    {
    struct blatStats	*next;
    struct oneStat upstream100;	/* 100 bases upstream of txn start. */
    struct oneStat upstream200;	/* 200 bases upstream of txn start. */
    struct oneStat upstream400;	/* 400 bases upstream of txn start. */
    struct oneStat upstream800;	/* 800 bases upstream of txn start. */
    struct oneStat mrnaTotal;	/* CDS + UTR's. */
    struct oneStat utr5;		/* 5' UTR. */
    struct oneStat firstCds;	/* Coding part of first coding exon. */
    struct oneStat firstIntron;	/* First intron. */
    struct oneStat middleCds;	/* Middle coding exons. */
    struct oneStat onlyCds;	/* Case where only one CDS exon. */
    struct oneStat middleIntron;	/* Middle introns. */
    struct oneStat onlyIntron;	/* Case where only single intron. */
    struct oneStat endCds;		/* Coding part of last coding exon. */
    struct oneStat endIntron;	/* Last intron. */
    struct oneStat splice5;	/* First 10 bases of intron. */
    struct oneStat splice3;	/* Last 10 bases of intron. */
    struct oneStat utr3;		/* 3' UTR. */
    struct oneStat downstream200;	/* 200 bases past end of UTR. */
    };

void reportStat(FILE *f, char *name, struct oneStat *stat, boolean reportPercentId);
/* Print out one set of stats. */

void reportStats(FILE *f, struct blatStats *stats, char *name, boolean reportPercentId);
/* Print out stats. */

void addStat(struct oneStat *a, struct oneStat *acc);
/* Add stat from a into acc. */

void addStats(struct blatStats *a, struct blatStats *acc);
/* Add stats in a to acc. */

struct blatStats *sumStatsList(struct blatStats *list);
/* Return sum of all stats. */

float divAsPercent(double a, double b);
/* Return a/b * 100. */
