/* gbAlignTbl - loading of PSL alignment tables. This defines per-table
 * objects that handling loading and updating. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#ifndef gbAlignTbl_h
#define gbAlignTbl_h
struct sqlConnection;
struct psl;
struct estOrientInfo;
struct gbStatus;
struct gbAlignTbl;  // opaque
struct gbAlignTblSet;  // opaque


/* table names and per-chrom suffixes */
extern char *ALL_MRNA_TBL;
extern char *MRNA_TBL_SUF;
extern char *MRNA_ORIENTINFO_TBL;
extern char *ALL_EST_TBL;
extern char *EST_TBL_SUF;
extern char *INTRON_EST_TBL;
extern char *INTRON_EST_TBL_SUF;
extern char *EST_ORIENTINFO_TBL;
extern char *XENO_MRNA_TBL;
extern char *XENO_EST_TBL;
extern char *REFSEQ_ALI_TBL;
extern char *XENO_REFSEQ_ALI_TBL;
extern char *MGC_FULL_MRNA_TBL;
extern char *ORFEOME_MRNA_TBL;

struct slName* gbAlignTblList(struct sqlConnection *conn);
/* Get list of psl tables in database. */

void gbAlignTblWrite(struct gbAlignTbl *gat, struct psl* psl,
                     struct sqlConnection *conn);
/* write new align to PSL tables */

void gbAlignTblWriteOi(struct gbAlignTbl *gat, struct estOrientInfo* oi,
                       struct sqlConnection *conn);
/* write new align orientInfo */

struct gbAlignTblSet *gbAlignTblSetNew(boolean perChrom, boolean hasVersion, char *tmpDir);
/* construct a new gbAlignTblSet object */

void gbAlignTblSetFree(struct gbAlignTblSet **gatsPtr);
/* free a gbAlignTblSet object */

struct gbAlignTbl *gbAlignTblSetGet(struct gbAlignTblSet *gats,
                                    struct gbStatus *status);
/* get an entry from the table based on srcDb|type|orgCat,
 * creating if it doesn't exist */

struct gbAlignTbl *gbAlignTblSetGetIntronEst(struct gbAlignTblSet *gats);
/* get gbAlignTbl for the intronEst table */

struct gbAlignTbl *gbAlignTblSetGetMgc(struct gbAlignTblSet *gats);
/* get gbAlignTbl for the MGC table */

struct gbAlignTbl *gbAlignTblSetOrfeomeGet(struct gbAlignTblSet *gats);
/* construct a new gbAlignTbl for the orfeome table */

void gbAlignTblSetCommit(struct gbAlignTblSet *gats,
                         struct sqlConnection *conn);
/* commit all gbAlignTbl objects in an gbAlignTblSet */

#endif
