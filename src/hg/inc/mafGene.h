
/* output a FASTA alignment from the given mafTable
 * for a given genePred
 */
void mafGeneOutPred(FILE *f, struct genePred *pred, char *dbName, 
    char *mafTable,  struct slName *speciesNameList, 
    boolean inExons, boolean noTrans);
