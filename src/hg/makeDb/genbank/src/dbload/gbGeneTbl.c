/* gbGeneTbl - loading of genePred tables derived from alignment tables.
 * This defines per-table objects that handling loading and updating. */
#include "common.h"
#include "gbGeneTbl.h"
#include "gbAlignTbl.h"
#include "errabort.h"
#include "hash.h"
#include "portable.h"
#include "linefile.h"
#include "jksql.h"
#include "psl.h"
#include "pslReader.h"
#include "hdb.h"
#include "genePred.h"
#include "gbIndex.h"
#include "gbRelease.h"
#include "gbEntry.h"
#include "dbLoadOptions.h"
#include "gbAligned.h"
#include "gbGenome.h"
#include "gbStatusTbl.h"
#include "gbProcessed.h"
#include "gbFileOps.h"
#include "gbVerb.h"
#include "sqlUpdater.h"
#include "gbBuildState.h"
#include "gbSql.h"
#include "sqlDeleter.h"

/* table names */
char *REF_GENE_TBL = "refGene";
char *REF_FLAT_TBL = "refFlat";
char *XENO_REF_GENE_TBL = "xenoRefGene";
char *XENO_REF_FLAT_TBL = "xenoRefFlat";
char *MGC_GENES_TBL = "mgcGenes";
char *ORFEOME_GENES_TBL = "orfeomeGenes";

struct gbGeneTbl
/* object associated with a genePred table being updated or loaded */
{
    char *tbl;                  // table name
    char *flatTbl;              // flat table name, or NULL if no flat table
    char *alnTbl;               // alignment table for rebuilding
    boolean hasBin;             // table has or will have bin column
    boolean hasExtCols;         // table has or will have exonFrames, etc columns
    struct sqlUpdater* upd;     // updater for table
    struct sqlUpdater* flatUpd; // updater for flat table
    char tmpDir[PATH_LEN];      // tmp directory
};

static struct gbGeneTbl *gbGeneTblNew(struct sqlConnection* conn,
                                      char *tbl, char *flatTbl,
                                      char *alnTbl, char *tmpDir)
/* construct a new gbGeneTbl object */
{
struct gbGeneTbl *ggt;
AllocVar(ggt);
ggt->tbl = cloneString(tbl);
ggt->flatTbl = cloneString(flatTbl);
ggt->alnTbl = cloneString(alnTbl);

if (!sqlTableExists(conn, ggt->tbl))
    {
    ggt->hasBin = TRUE;
    ggt->hasExtCols = TRUE;
    }
else
    {
    ggt->hasBin = (sqlFieldIndex(conn, ggt->tbl, "bin") >= 0);
    ggt->hasExtCols = (sqlFieldIndex(conn, ggt->tbl, "exonFrames") >= 0);
    }
safecpy(ggt->tmpDir, sizeof(ggt->tmpDir), tmpDir);
return ggt;
}

static void gbGeneTblFree(struct gbGeneTbl **ggtPtr)
/* free a gbGeneTbl object */
{
struct gbGeneTbl *ggt = *ggtPtr;
if (ggt != NULL)
    {
    freeMem(ggt->tbl);
    freeMem(ggt->flatTbl);
    sqlUpdaterFree(&ggt->upd);
    sqlUpdaterFree(&ggt->flatUpd);
    *ggtPtr = NULL;
    }
}

static void gbGeneTblCommit(struct gbGeneTbl *ggt, struct sqlConnection* conn)
/* commit gbGeneTbl */
{
if (ggt->upd != NULL)
    sqlUpdaterCommit(ggt->upd, conn);
if (ggt->flatUpd != NULL)
    sqlUpdaterCommit(ggt->flatUpd, conn);
}

static void createGeneTbl(struct gbGeneTbl *ggt, struct sqlConnection* conn)
/* create a genePred table */
{
char *sql = genePredGetCreateSql(ggt->tbl,
                                 (ggt->hasExtCols ? genePredAllFlds : 0),
                                 (ggt->hasBin ? genePredWithBin : 0),
                                 hGetMinIndexLength(sqlGetDatabase(conn)));
sqlRemakeTable(conn, ggt->tbl, sql);
freeMem(sql);
}

static void createFlatTbl(struct gbGeneTbl *ggt, struct sqlConnection* conn)
/* create a genePred flat table */
{
/* edit generated SQL to add geneName column and index */
char *tmpDef = genePredGetCreateSql(ggt->flatTbl, 0, 0, hGetMinIndexLength(sqlGetDatabase(conn)));
char *part2 = strchr(tmpDef, '(');
*(part2++) = '\0';
char *p = strrchr(part2, ')');
*p = '\0';
char editDef[1024];
safef(editDef, sizeof(editDef),
      "%s(geneName varchar(255) not null, %s, INDEX(geneName(10)))",
      tmpDef, part2);
freeMem(tmpDef);
sqlRemakeTable(conn, ggt->flatTbl, editDef);
}

static FILE *gbGeneTblGetTabFh(struct gbGeneTbl *ggt,
                               struct sqlConnection* conn)
/* get tab file for genePred table, create updater and table if needed */
{
if (ggt->upd == NULL)
    {
    if (!sqlTableExists(conn, ggt->tbl))
        createGeneTbl(ggt, conn);
    ggt->upd = sqlUpdaterNew(ggt->tbl, ggt->tmpDir, (gbVerbose >= 4), NULL);
    }
return sqlUpdaterGetFh(ggt->upd, 1);
}

static FILE *gbGeneTblGetFlatTabFh(struct gbGeneTbl *ggt,
                                   struct sqlConnection* conn)
/* get tab file for genePred flat table, create updater and table if needed */
{
if (ggt->flatUpd == NULL)
    {
    if (!sqlTableExists(conn, ggt->flatTbl))
        createFlatTbl(ggt, conn);
    ggt->flatUpd = sqlUpdaterNew(ggt->flatTbl, ggt->tmpDir, (gbVerbose >= 4), NULL);
    }
return sqlUpdaterGetFh(ggt->flatUpd, 1);
}

static void gbGeneTblWriteGene(struct gbGeneTbl *ggt, struct gbStatus* status,
                               struct psl* psl, struct sqlConnection *conn)
/* write genePred row */
{
struct genePred* gp
    = genePredFromPsl3(psl, &status->cds, 
                       (ggt->hasExtCols ? genePredAllFlds : 0), 0,
                       genePredStdInsertMergeSize, genePredStdInsertMergeSize);
FILE *fh = gbGeneTblGetTabFh(ggt, conn);
if (ggt->hasExtCols)
    {
    /* add gene name */
    freeMem(gp->name2);
    gp->name2 = cloneString(status->geneName);
    }
if (ggt->hasBin)
    fprintf(fh, "%u\t", hFindBin(gp->txStart, gp->txEnd));
genePredTabOut(gp, fh);
genePredFree(&gp);
}

static void gbGeneTblWriteGeneFlat(struct gbGeneTbl *ggt, struct gbStatus* status,
                                   struct psl* psl, struct sqlConnection *conn)
/* write genePred flat row */
{
struct genePred* gp
    = genePredFromPsl3(psl, &status->cds, 0, 0,
                       genePredStdInsertMergeSize, genePredStdInsertMergeSize);
FILE *fh = gbGeneTblGetFlatTabFh(ggt, conn);
fprintf(fh, "%s\t", ((status->geneName == NULL) ? "" : status->geneName));
genePredTabOut(gp, fh);
genePredFree(&gp);
}

void gbGeneTblWrite(struct gbGeneTbl *ggt, struct gbStatus* status,
                    struct psl* psl, struct sqlConnection *conn)
/* write new gene to a genePred table */
{
char *vdot = gbDropVer(psl->qName);
gbGeneTblWriteGene(ggt, status, psl, conn);
if (ggt->flatTbl != NULL)
    gbGeneTblWriteGeneFlat(ggt, status, psl, conn);
gbRestoreVer(psl->qName, vdot);
}

void gbGeneTblRebuild(struct gbGeneTbl *ggt, struct gbStatus* status,
                      struct sqlConnection *conn)
/* rebuild a gene from an alignment that is already loaded in a table */
{
char where[128];
safef(where, sizeof(where), "qName = \"%s\"", status->acc);
struct psl *psls = pslReaderLoadQuery(conn, ggt->alnTbl, where);
struct psl *psl;
for (psl = psls; psl != NULL; psl = psl->next)
    gbGeneTblWrite(ggt, status, psl, conn);
pslFreeList(&psls);
}

struct slName* gbGeneTblList(struct sqlConnection *conn)
/* Get list of genePred tables in database. */
{
struct slName* tables = NULL;
gbAddTableIfExists(conn, REF_GENE_TBL, &tables);
gbAddTableIfExists(conn, REF_FLAT_TBL, &tables);
gbAddTableIfExists(conn, XENO_REF_GENE_TBL, &tables);
gbAddTableIfExists(conn, XENO_REF_FLAT_TBL, &tables);
gbAddTableIfExists(conn, MGC_GENES_TBL, &tables);
gbAddTableIfExists(conn, ORFEOME_GENES_TBL, &tables);
return tables;
}

struct gbGeneTblSet
/* set of gbGeneTbl objects for all possible tables, created in a lazy
 * manner. */
{
    char tmpDir[PATH_LEN];

    // per-table objects
    struct gbGeneTbl *refGene;
    struct gbGeneTbl *xenoRefGene;
    struct gbGeneTbl *mgcGenes;
    struct gbGeneTbl *orfeomeGenes;
};

struct gbGeneTblSet *gbGeneTblSetNew(char *tmpDir)
/* construct a new gbGeneTblSet object */
{
struct gbGeneTblSet *ggts;
AllocVar(ggts);
safecpy(ggts->tmpDir, sizeof(ggts->tmpDir), tmpDir);
return ggts;
}

void gbGeneTblSetFree(struct gbGeneTblSet **ggtsPtr)
/* free a gbGeneTblSet object */
{
struct gbGeneTblSet *ggts = *ggtsPtr;
if (ggts != NULL)
    {
    gbGeneTblFree(&ggts->refGene);
    gbGeneTblFree(&ggts->xenoRefGene);
    gbGeneTblFree(&ggts->mgcGenes);
    gbGeneTblFree(&ggts->orfeomeGenes);
    freeMem(ggts);
    *ggtsPtr = NULL;
    }
}

struct gbGeneTbl *gbGeneTblSetRefGeneGet(struct gbGeneTblSet *ggts,
                                         struct sqlConnection* conn)
/* get or create gbGeneTbl for refGene */
{
if (ggts->refGene == NULL)
    ggts->refGene = gbGeneTblNew(conn, REF_GENE_TBL, REF_FLAT_TBL, REFSEQ_ALI_TBL, ggts->tmpDir);
return ggts->refGene;
}

struct gbGeneTbl *gbGeneTblSetXenoRefGeneGet(struct gbGeneTblSet *ggts,
                                             struct sqlConnection* conn)
/* get or create gbGeneTbl for xenoRefGene */
{
if (ggts->xenoRefGene == NULL)
    ggts->xenoRefGene = gbGeneTblNew(conn, XENO_REF_GENE_TBL, XENO_REF_FLAT_TBL, XENO_REFSEQ_ALI_TBL, ggts->tmpDir);
return ggts->xenoRefGene;
}

struct gbGeneTbl *gbGeneTblSetMgcGenesGet(struct gbGeneTblSet *ggts,
                                          struct sqlConnection* conn)
/* get or create gbGeneTbl for mgcGenes */
{
if (ggts->mgcGenes == NULL)
    ggts->mgcGenes = gbGeneTblNew(conn, MGC_GENES_TBL, NULL, MGC_FULL_MRNA_TBL, ggts->tmpDir);
return ggts->mgcGenes;
}

struct gbGeneTbl *gbGeneTblSetOrfeomeGenesGet(struct gbGeneTblSet *ggts,
                                              struct sqlConnection* conn)
/* get or create a gbGeneTbl for orfeomeGenes */
{
if (ggts->orfeomeGenes == NULL)
    ggts->orfeomeGenes = gbGeneTblNew(conn, ORFEOME_GENES_TBL, NULL, MGC_FULL_MRNA_TBL, ggts->tmpDir);
return ggts->orfeomeGenes;
}

void gbGeneTblSetCommit(struct gbGeneTblSet *ggts,
                        struct sqlConnection *conn)
/* commit all gbGeneTbl objects in an gbGeneTblSet */
{
if (ggts->refGene != NULL)
    gbGeneTblCommit(ggts->refGene, conn);
if (ggts->xenoRefGene != NULL)
    gbGeneTblCommit(ggts->xenoRefGene, conn);
if (ggts->mgcGenes != NULL)
    gbGeneTblCommit(ggts->mgcGenes, conn);
if (ggts->orfeomeGenes != NULL)
    gbGeneTblCommit(ggts->orfeomeGenes, conn);
}
