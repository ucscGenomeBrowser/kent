/* gbGeneTbl - loading of genePred tables derived from alignment tables.
 * This defines per-table objects that handling loading and updating. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#ifndef gbGeneTbl_h
#define gbGeneTbl_h
struct sqlConnection;
struct gbStatus;
struct psl;
struct gbGeneTbl;  // opaque
struct gbGeneTblSet;  // opaque

/* table names */
extern char *REF_GENE_TBL;
extern char *REF_FLAT_TBL;
extern char *XENO_REF_GENE_TBL;
extern char *XENO_REF_FLAT_TBL;
extern char *MGC_GENES_TBL;
extern char *ORFEOME_GENES_TBL;

void gbGeneTblWrite(struct gbGeneTbl *ggt, struct gbStatus* status,
                    struct psl* psl, struct sqlConnection *conn);
/* write new gene to a genePred table */

void gbGeneTblRebuild(struct gbGeneTbl *ggt, struct gbStatus* status,
                      struct sqlConnection *conn);
/* rebuild a gene from an alignment that is already loaded in a table */

struct slName* gbGeneTblList(struct sqlConnection *conn);
/* Get list of genePred tables in database. */

struct gbGeneTblSet *gbGeneTblSetNew(char *tmpDir);
/* construct a new gbGeneTblSet object */

void gbGeneTblSetFree(struct gbGeneTblSet **ggtsPtr);
/* free a gbGeneTblSet object */

struct gbGeneTbl *gbGeneTblSetRefGeneGet(struct gbGeneTblSet *ggts,
			     boolean hasVersion, struct sqlConnection* conn);
/* get or create gbGeneTbl for refGene */

struct gbGeneTbl *gbGeneTblSetXenoRefGeneGet(struct gbGeneTblSet *ggts,
			     boolean hasVersion, struct sqlConnection* conn);
/* get or create gbGeneTbl for xenoRefGene */

struct gbGeneTbl *gbGeneTblSetMgcGenesGet(struct gbGeneTblSet *ggts,
			     boolean hasVersion, struct sqlConnection* conn);
/* get or create gbGeneTbl for mgcGenes */

struct gbGeneTbl *gbGeneTblSetOrfeomeGenesGet(struct gbGeneTblSet *ggts,
			     boolean hasVersion, struct sqlConnection* conn);
/* get or create a gbGeneTbl for orfeomeGenes */

void gbGeneTblSetCommit(struct gbGeneTblSet *ggts,
                        struct sqlConnection *conn);
/* commit all gbGeneTbl objects in an gbGeneTblSet */

#endif
