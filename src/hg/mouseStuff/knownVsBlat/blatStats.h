
struct stat
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
    struct stat upstream100;	/* 100 bases upstream of txn start. */
    struct stat upstream200;	/* 200 bases upstream of txn start. */
    struct stat upstream400;	/* 400 bases upstream of txn start. */
    struct stat upstream800;	/* 800 bases upstream of txn start. */
    struct stat mrnaTotal;	/* CDS + UTR's. */
    struct stat utr5;		/* 5' UTR. */
    struct stat firstCds;	/* Coding part of first coding exon. */
    struct stat firstIntron;	/* First intron. */
    struct stat middleCds;	/* Middle coding exons. */
    struct stat onlyCds;	/* Case where only one CDS exon. */
    struct stat middleIntron;	/* Middle introns. */
    struct stat onlyIntron;	/* Case where only single intron. */
    struct stat endCds;		/* Coding part of last coding exon. */
    struct stat endIntron;	/* Last intron. */
    struct stat splice5;	/* First 10 bases of intron. */
    struct stat splice3;	/* Last 10 bases of intron. */
    struct stat utr3;		/* 3' UTR. */
    struct stat downstream200;	/* 200 bases past end of UTR. */
    };

void reportStat(FILE *f, char *name, struct stat *stat, boolean reportPercentId);
/* Print out one set of stats. */

void reportStats(FILE *f, struct blatStats *stats, char *name, boolean reportPercentId);
/* Print out stats. */

void addStat(struct stat *a, struct stat *acc);
/* Add stat from a into acc. */

struct blatStats *addStats(struct blatStats *a, struct blatStats *acc);
/* Add stats in a to acc. */

struct blatStats *sumStatsList(struct blatStats *list);
/* Return sum of all stats. */

float divAsPercent(double a, double b);
/* Return a/b * 100. */
