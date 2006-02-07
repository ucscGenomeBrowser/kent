/* validation of all alignment-based tables */
#include "chkAlignTbls.h"
#include "chkCommon.h"
#include "metaData.h"
#include "localmem.h"
#include "gbFileOps.h"
#include "gbRelease.h"
#include "gbVerb.h"
#include "jksql.h"
#include "hdb.h"
#include "hash.h"
#include "genePred.h"
#include "gbGenome.h"
#include "psl.h"

static char const rcsid[] = "$Id: chkAlignTbls.c,v 1.9 2006/01/22 08:09:59 markd Exp $";

/* FIXME: check native vs xeno, flag in metaData. */
/* FIXME: check OI tables */

/* FIXME: remove when bugs fixes */
#define WARN_BLAT_BUGS 1

static struct slName* gChroms = NULL;
static struct hash* gChromSizes = NULL; /* table of chromsome sizes */
static boolean gCheckPerChrom = FALSE;   /* build per-chrom tables */

static void buildChromSizes()
/* build table of chromosome sizes and list of chromosomes */
{
struct sqlConnection *conn =  hAllocConn();
struct sqlResult *sr;
char **row;
gChromSizes = hashNew(8);

sr = sqlGetResult(conn, "SELECT chrom,size FROM chromInfo");
while ((row = sqlNextRow(sr)) != NULL)
    {
    unsigned sz = gbParseUnsigned(NULL, row[1]);
    hashAddInt(gChromSizes, row[0], sz);
    slSafeAddHead(&gChroms, newSlName(row[0]));
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}

static unsigned getChromSize(char* chrom)
/* get chromosome size, or 0 if invalid */
{
struct hashEl* hel;
if (gChromSizes == NULL)
    buildChromSizes();

hel = hashLookup(gChromSizes, chrom);
if (hel == NULL)
    return 0;
else
    return (unsigned)(size_t)hel->val;
}

static void chkPsl(struct psl* psl, unsigned iRow, char* database,
                   char* table, struct metaDataTbls* metaDataTbls,
                   unsigned typeFlags)
/* Validate a PSL of a mrna/est to genome alignment against the metadata.
 * Also count the number of alignments of a mrna. */
{
unsigned chromSize = getChromSize(psl->tName);
struct metaData* md = metaDataTblsFind(metaDataTbls, psl->qName);
char pslDesc[128];
if (gbVerbose >= 3)
    gbVerbMsg(3, "chkPsl %s:%d %s %s",  table, iRow, psl->qName, psl->tName);

safef(pslDesc, sizeof(pslDesc), "psl %s.%s row %u", database, table, iRow);

/* check that we have sequence info and compare sizes sizes */
if (chromSize == 0)
    gbError("%s: tName not a valid chromosome: \"%s\"", pslDesc, psl->tName);
else
    if (chromSize != psl->tSize)
        gbError("%s: tSize %u != chromosome %s size %u",
                pslDesc, psl->tSize, psl->tName, chromSize);

if (md == NULL)
    gbError("%s: qName not in mrna table as type %s: \"%s\"",
            pslDesc, gbFmtSelect(typeFlags & GB_TYPE_MASK), psl->qName);
else if (md->inSeq)
    {
    if (!md->inGbIndex)
        gbError("%s: qName not in gbIndex as type %s: \"%s\"", pslDesc,
                gbFmtSelect(typeFlags & GB_TYPE_MASK), psl->qName);
    else
        {
        if (typeFlags != md->typeFlags)
            gbError("%s: alignment for %s type %s doesn't match expected %s",
                    pslDesc, psl->qName, gbFmtSelect(md->typeFlags),
                    gbFmtSelect(typeFlags));
        }
    if (md->seqSize != psl->qSize)
        gbError("%s: qSize %u != %s size %u",
                pslDesc, psl->qSize, psl->qName, md->seqSize);
    md->numAligns++;
    }

/* validate consistency of PSL */
if (pslCheck(pslDesc, stderr, psl))
    errorCnt++;
}

static void chkPslTable(struct gbSelect* select, struct sqlConnection* conn,
                        char* rootTable, char* chrom,
                        struct metaDataTbls* metaDataTbls,
                        unsigned typeFlags)
/* Validate a PSL of a mrna/est to genome alignment against the metadata.  If
 * not a chromosome-specific table, chrom should be null.  Chromosome-specific
 * tables are not required to exist (for testing purposes).  Also count the
 * number of alignments of a mrna. */
{
struct hTableInfo* tableInfo;
char table[64];
unsigned iRow = 0;
unsigned rowOffset;
char accWhere[64];
char query[512];
struct sqlResult *sr;
char **row;

/* need to specify an explicit chrom table, as there is an mrna table which is
 * not psl, so using mrna as a root name with a chrom that doesn't exist
 * returns the mrna instead of null */

if (chrom != NULL)
    safef(table, sizeof(table), "%s_%s", chrom, rootTable);
else
    safef(table, sizeof(table), "%s", rootTable);

gbVerbEnter(3, "chkPslTable %s", table);

tableInfo = hFindTableInfoDb(select->release->genome->database, chrom, table);
if (tableInfo == NULL)
    {
    /* If all table, require it */
    if (chrom == NULL)
        {
        if (testMode)
            fprintf(stderr, "Warning: no psl table %s.%s\n",
                    select->release->genome->database, table);
        else
            gbError("no psl table %s.%s\n", select->release->genome->database,
                    table);
        }
    }
else
    {
    rowOffset = (tableInfo->hasBin) ? 1 : 0;
    accWhere[0] = '\0';
    if (select->accPrefix != NULL)
        safef(accWhere, sizeof(accWhere), " WHERE qName LIKE '%s%%'",
              select->accPrefix);
    safef(query, sizeof(query), "SELECT * FROM %s%s", table, accWhere);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
        struct psl* psl = pslLoad(row+rowOffset);
        chkPsl(psl, iRow, select->release->genome->database, table,
               metaDataTbls, typeFlags);
        pslFree(&psl);
        iRow++;
        }
    sqlFreeResult(&sr);
    }
gbVerbLeave(3, "chkPslTable %s", table);
}

static void chkGenePred(struct genePred* gene, char *geneName, unsigned iRow,
                        char* database, char* table,
                        struct metaDataTbls* metaDataTbls, unsigned typeFlags)
/* Validate a genePred of a refSeq to genome alignment against the metadata.
 * Also count the number of alignments, and check the geneName, if available */
{
char desc[512];
unsigned chromSize = getChromSize(gene->chrom);
struct metaData* md = metaDataTblsFind(metaDataTbls, gene->name);

if (gbVerbose >= 3)
    gbVerbMsg(3, "chkGenePred %s:%d %s %s",  table, iRow, 
              gene->name, gene->chrom);
safef(desc, sizeof(desc), "gene %s.%s:%u %s %s", database, table,
      iRow, gene->name, gene->chrom);

/* basic sanity checks */
if (genePredCheck(desc, stderr, chromSize, gene))
    errorCnt++;

/* check if in mrna table */
if (md == NULL)
    gbError("%s: %s in not in mrna table", desc, gene->name);
else
    {
    if (typeFlags != md->typeFlags)
        gbError("%s: alignment of %s type %s doesn't match expected %s",
                desc, gene->name, gbFmtSelect(md->typeFlags),
                gbFmtSelect(typeFlags));
    md->numAligns++;
    }

/* check gene name */
if ((md != NULL) && (geneName != NULL))
    {
    char* rlName = (md->rlName == NULL) ? "" : md->rlName;
    if (!sameString(geneName, rlName))
        gbError("%s: %s geneName \"%s\" does not match refLink name \"%s\"",
                desc, gene->name, geneName, rlName);
    }
}

static void chkGenePredTable(struct gbSelect* select,
                             struct sqlConnection* conn,
                             char* table, boolean isRefFlat, 
                             struct metaDataTbls* metaDataTbls,
                             unsigned typeFlags)
/* Validate a genePred table.  Also count the number of genePreds for a
 * mrna.  If this is refFlat, also check the geneName.  Return numbner of
 * rows. */
{
unsigned iRow = 0;
char query[512];
struct sqlResult *sr;
char **row;
char *geneName = NULL;
int rowOff = (isRefFlat ? 1 : 0);  /* columns to skip to genePred */
if (sqlFieldIndex(conn, table, "bin") >= 0)
    rowOff++;


gbVerbEnter(3, "chkGenePredTable %s", table);

safef(query, sizeof(query), "SELECT * FROM %s", table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct genePred* gene = genePredLoad(row+rowOff);
    if (isRefFlat)
        geneName = row[0];
    chkGenePred(gene, geneName, iRow, select->release->genome->database, table,
                metaDataTbls, typeFlags);
    genePredFree(&gene);
    iRow++;
    }
sqlFreeResult(&sr);
gbVerbLeave(3, "chkGenePredTable %s", table);
}

static void chkAlignCount(struct metaData* md, struct metaDataTbls* metaDataTbls,
                          char* dbTableDesc, unsigned typeFlags)
/* check alignment counts or gene counts for an accession if it matches the
 * type, also reset the counts. */
{
if (typeFlags == md->typeFlags)
    {
    if (md->numAligns != md->gbsNumAligns)
        gbError("%s: number of alignments found in %s (%d) does not match "
                "expected (%d) from gbStatus",
                md->acc, dbTableDesc, md->numAligns, md->gbsNumAligns);
    }
md->numAligns = 0; /* reset counts */
}

static void chkAlignCounts(struct metaDataTbls* metaDataTbls,
                           char* dbTableDesc, unsigned typeFlags)
/* check alignment counts found in tables, also reset the counts
 * Type includes GB_NATIVE or GB_XENO. */
{
struct metaData* md;

gbVerbEnter(3, "chkAlignCounts %s", dbTableDesc);

/* Traverse all metadata entries, comparing number of alignments with
 * gbStatus. */
metaDataTblsFirst(metaDataTbls);
while ((md = metaDataTblsNext(metaDataTbls)) != NULL)
    chkAlignCount(md, metaDataTbls, dbTableDesc, typeFlags);
gbVerbLeave(3, "chkAlignCounts %s", dbTableDesc);
}

static void chkGenBankAlignTables(struct gbSelect* select,
                                  struct sqlConnection* conn,
                                  struct metaDataTbls* metaDataTbls)
/* Verify all of the PSL tables, including checking the count of
 * alignments for either mRNA or ESTs. */
{
char dbTableDesc[256];
struct slName* chrom;
char *chromTable = ((select->type == GB_MRNA) ? "mrna" : "est");
char *allTable = ((select->type == GB_MRNA) ? "all_mrna" : "all_est");
char *xenoTable = ((select->type == GB_MRNA) ? "xenoMrna" : "xenoEst");

/* all native */
safef(dbTableDesc, sizeof(dbTableDesc), "%s:%s",
      select->release->genome->database, allTable);
chkPslTable(select, conn, allTable, NULL, metaDataTbls,
            (GB_GENBANK|GB_NATIVE|select->type));
chkAlignCounts(metaDataTbls, dbTableDesc, (GB_GENBANK|GB_NATIVE|select->type));

/* native by chrom */
safef(dbTableDesc, sizeof(dbTableDesc), "%s:chr*_%s",
      select->release->genome->database, chromTable);
if (gCheckPerChrom)
    {
    for (chrom = gChroms; chrom != NULL; chrom = chrom->next)
        chkPslTable(select, conn, chromTable, chrom->name, metaDataTbls,
                    (GB_GENBANK|GB_NATIVE|select->type));
    chkAlignCounts(metaDataTbls, dbTableDesc,
                   (GB_GENBANK|GB_NATIVE|select->type));
    }

/* all xeno */
safef(dbTableDesc, sizeof(dbTableDesc), "%s:%s",
      select->release->genome->database, xenoTable);
chkPslTable(select, conn, xenoTable, NULL, metaDataTbls,
            (GB_GENBANK|GB_XENO|select->type));
chkAlignCounts(metaDataTbls, dbTableDesc, (GB_GENBANK|GB_XENO|select->type));
}

static void chkRefSeqAlignTables(struct gbSelect* select,
                                 struct sqlConnection* conn,
                                 struct metaDataTbls* metaDataTbls)
/* Verify all of the refsseq alignment tables, including checking the count of
 * genePred. */
{
char* database = select->release->genome->database;
char dbTableDesc[256];

if (select->orgCats & GB_NATIVE)
    {
    unsigned typeFlags = GB_REFSEQ|GB_MRNA|GB_NATIVE;
    safef(dbTableDesc, sizeof(dbTableDesc), "%s:%s", database, "refSeqAli");
    chkPslTable(select, conn, "refSeqAli", NULL, metaDataTbls, typeFlags);
    chkAlignCounts(metaDataTbls, dbTableDesc, typeFlags);

    safef(dbTableDesc, sizeof(dbTableDesc), "%s:%s", database, "refGene");
    chkGenePredTable(select, conn, "refGene", FALSE, metaDataTbls, typeFlags);
    chkAlignCounts(metaDataTbls, dbTableDesc, typeFlags);
    
    safef(dbTableDesc, sizeof(dbTableDesc), "%s:%s", database, "refFlat");
    chkGenePredTable(select, conn, "refFlat", TRUE, metaDataTbls, typeFlags);
    chkAlignCounts(metaDataTbls, dbTableDesc, typeFlags);
    }
if (select->orgCats & GB_XENO)
    {
    unsigned typeFlags = GB_REFSEQ|GB_MRNA|GB_XENO;
    safef(dbTableDesc, sizeof(dbTableDesc), "%s:%s", database, "xenoRefSeqAli");
    chkPslTable(select, conn, "xenoRefSeqAli", NULL, metaDataTbls, typeFlags);
    chkAlignCounts(metaDataTbls, dbTableDesc, typeFlags);

    safef(dbTableDesc, sizeof(dbTableDesc), "%s:%s", database, "xenoRefGene");
    chkGenePredTable(select, conn, "xenoRefGene", FALSE, metaDataTbls, typeFlags);
    chkAlignCounts(metaDataTbls, dbTableDesc, typeFlags);
    
    safef(dbTableDesc, sizeof(dbTableDesc), "%s:%s", database, "xenoRefFlat");
    chkGenePredTable(select, conn, "xenoRefFlat", TRUE, metaDataTbls, typeFlags);
    chkAlignCounts(metaDataTbls, dbTableDesc, typeFlags);
    }
}

void chkAlignTables(struct gbSelect* select, struct sqlConnection* conn,
                    struct metaDataTbls* metaDataTbls, boolean checkPerChrom)
/* Verify all of the alignment-related. */
{
gCheckPerChrom = checkPerChrom;
if (gChromSizes == NULL)
    buildChromSizes();
gbVerbEnter(1, "validating alignment tables: %s", gbSelectDesc(select));
if (select->release->srcDb == GB_GENBANK)
    chkGenBankAlignTables(select, conn, metaDataTbls);
else
    chkRefSeqAlignTables(select, conn, metaDataTbls);
gbVerbLeave(1, "validated alignment tables: %s", gbSelectDesc(select));
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

