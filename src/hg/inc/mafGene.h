/* Copyright (C) 2012 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */


/* options */
#define MAFGENE_EXONS		(1 << 0)
#define MAFGENE_NOTRANS		(1 << 1)
#define MAFGENE_OUTBLANK	(1 << 2)
#define MAFGENE_OUTTABLE	(1 << 3)
#define MAFGENE_INCLUDEUTR	(1 << 4)
#define MAFGENE_UNIQUEAA	(1 << 5)

// struct to allow us to keep these files open and cache chrom sequence
struct mafFileCache 
{
struct twoBitFile *tbf;
struct bbiFile *bbi;
char *chrom;
struct dnaSeq *dnaSeq;
};

/* output a FASTA alignment from the given mafTable
 * for a given genePred
 */
void mafGeneOutPred(FILE *f, struct genePred *pred, char *dbName, 
    char *mafTable,  struct slName *speciesNameList, unsigned options,
    int numCols);

void mafGeneOutPredExt(FILE *f, struct genePred *pred, char *dbName, 
    char *mafTable,  struct slName *speciesNameList, unsigned options,
    int numCols, struct mafFileCache *mafFileCache);
