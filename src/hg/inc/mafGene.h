
/* options */
#define MAFGENE_EXONS		(1 << 0)
#define MAFGENE_NOTRANS		(1 << 1)
#define MAFGENE_OUTBLANK	(1 << 2)
#define MAFGENE_OUTTABLE	(1 << 3)

/* output a FASTA alignment from the given mafTable
 * for a given genePred
 */
void mafGeneOutPred(FILE *f, struct genePred *pred, char *dbName, 
    char *mafTable,  struct slName *speciesNameList, unsigned options,
    int numCols);
