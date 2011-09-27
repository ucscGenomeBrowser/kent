/* Parsing and output of mrna metadata in ra file into seq, mrna and
 * associated unique string tables.  This module functions as a singlton
 * object, with global state that can be reset.  Refer to the
 * doc/database-update-step.html before modifying.
 */

#include "gbMetaData.h"
#include "gbGeneTbl.h"
#include "gbAlignTbl.h"
#include "gbMDParse.h"
#include "common.h"
#include "hash.h"
#include "portable.h"
#include "linefile.h"
#include "localmem.h"
#include "dystring.h"
#include "jksql.h"
#include "sqlUpdater.h"
#include "seqTbl.h"
#include "imageCloneTbl.h"
#include "dbLoadOptions.h"
#include "gbDefs.h"
#include "gbConf.h"
#include "gbVerb.h"
#include "gbIndex.h"
#include "gbEntry.h"
#include "gbRelease.h"
#include "gbProcessed.h"
#include "extFileTbl.h"
#include "gbFileOps.h"
#include "gbStatusTbl.h"
#include "gbBuildState.h"
#include "sqlDeleter.h"
#include "genbank.h"
#include "gbSql.h"
#include "gbMiscDiff.h"
#include <regex.h>

static char const rcsid[] = "$Id: gbMetaData.c,v 1.48 2009/04/21 00:14:50 markd Exp $";

/* mol enum shared by gbCdnaInfo and refSeqStatus */
#define molEnumDef \
  "mol enum('DNA', 'RNA', 'ds-RNA', 'ds-mRNA', 'ds-rRNA', 'mRNA', 'ms-DNA', 'ms-RNA',"\
  "         'rRNA', 'scRNA', 'snRNA', 'snoRNA', 'ss-DNA', 'ss-RNA', 'ss-snoRNA', 'tRNA',"\
  "         'cRNA', 'ss-cRNA', 'ds-cRNA', 'ms-rRNA') not null,\n"

static char* gbCdnaInfoCreate =
/* This keeps track of mRNA. */
"create table gbCdnaInfo ("
  "id int unsigned not null primary key,"         /* Id, same as seq ID. */
  "acc char(12) not null,"                        /* Genbank accession. */
  "version smallint unsigned not null,"           /* Genbank version. */
  "moddate date not null,"                        /* last modified date. */
  "type enum('EST','mRNA') not null,"             /* Full length or EST. */
  "direction enum('5','3','0') not null,"         /* Read direction. */
  "source int unsigned not null,"                 /* Ref in source table. */
  "organism int unsigned not null,"               /* Ref in organism table. */
  "library int unsigned not null,"                /* Ref in library table. */
  "mrnaClone int unsigned not null,"              /* Ref in clone table. */
  "sex int unsigned not null,"                    /* Ref in sex table. */
  "tissue int unsigned not null,"                 /* Ref in tissue table. */
  "development int unsigned not null,"            /* Ref in development table. */
  "cell int unsigned not null,"                   /* Ref in cell table. */
  "cds int unsigned not null,"                    /* Ref in CDS table. */
  "keyword int unsigned not null,"                /* Ref in key table. */
  "description int unsigned not null,"            /* Ref in description table. */
  "geneName int unsigned not null,"               /* Ref in geneName table. */
  "productName int unsigned not null,"            /* Ref in productName table. */
  "author int unsigned not null,"                 /* Ref in author table. */
  "gi int unsigned not null,"                     /* NCBI GI number. */
  molEnumDef
  /* Extra indices. */
  "unique(acc),"
  "index(type),"
  "index(library),"
  "index(mrnaClone),"
  "index(tissue),"
  "index(development),"
  "index(cell),"
  "index(keyword),"
  "index(description),"
  "index(geneName),"
  "index(productName),"
  "index(author))";

static char* refSeqStatusCreate = 
"CREATE TABLE refSeqStatus ("
"    mrnaAcc varchar(255) not null,"
"    status enum('Unknown', 'Reviewed', 'Validated', 'Provisional', 'Predicted', 'Inferred') not null,"
molEnumDef
"    PRIMARY KEY(mrnaAcc))";

static char* refLinkCreate = 
"CREATE TABLE refLink (\n"
"    name varchar(255) not null,        # Name displayed in UI\n"
"    product varchar(255) not null,     # Name of protein product\n"
"    mrnaAcc varchar(255) not null,     # mRNA accession\n"
"    protAcc varchar(255) not null,     # protein accession\n"
"    geneName int unsigned not null,    # pointer to geneName table\n"
"    prodName int unsigned not null,    # pointer to productName table\n"
"    locusLinkId int unsigned not null, # Locus Link ID\n"
"    omimId int unsigned not null,      # OMIM ID\n"
"    #Indices\n"
"    PRIMARY KEY(mrnaAcc),\n"
"    index(name(10)),\n"
"    index(protAcc(10)),\n"
"    index(locusLinkId),\n"
"    index(omimId),\n"
"    index(prodName),\n"
"    index(geneName)\n"
")";

/*
 * Summary table is sparse, only created for refSeqs that have Summary:
 * or COMPLETENESS: in comment.
 * Mapping of COMPLETENESS:
 *    Complete5End       = complete on the 5' end.
 *    Complete3End       = complete on the 3' end.
 *    FullLength         = full length.
 *    IncompleteBothEnds = incomplete on both ends.
 *    Incomplete5End     = incomplete on the 5' end.
 *    Incomplete3End     = incomplete on the 3' end.
 *    Partial            = not full length.
 *    Unknown            = unknown
 */
static char* refSeqSummaryCreate = 
"CREATE TABLE refSeqSummary ("
"  mrnaAcc varchar(255) not null,"
"  completeness enum('Unknown', 'Complete5End', "
"    'Complete3End', 'FullLength', 'IncompleteBothEnds',"
"    'Incomplete5End', 'Incomplete3End', 'Partial') not null,"
"  summary text not null,"
"  PRIMARY KEY(mrnaAcc))";


/* list of the names of the id to string tables */
static char *raFieldTables[] =
    {
    "author",
    "cds",
    "cell",
    "description",
    "development",
    "geneName",
    "keyword",
    "library",
    "mrnaClone",
    "organism",
    "productName",
    "sex",
    "source",
    "tissue",
    NULL
    };


/* SQL to create gbMiscDiff table */
static char* gbMiscDiffCreate = 
"CREATE TABLE gbMiscDiff (\n"
"    acc char(12) not null,\n"
"    mrnaStart int not null,\n"
"    mrnaEnd int not null,\n"
"    notes longtext,\n"
"    gene varchar(255),\n"
"    replacement varchar(255),\n"
"    index(acc)\n"
");\n";


/* SQL to create gbWarn table */
static char* gbWarnCreate = 
"CREATE TABLE gbWarn ("
"    acc char(12) not null,"
"    reason enum(\"invitroNorm\", \"athRage\", \"orestes\") not null,"
"    PRIMARY KEY(acc)"
");";


/* global configuration */
static struct dbLoadOptions* gOptions; /* options from cmdline and conf */
static char gTmpDir[PATH_LEN];      /* tmp dir for load file */
static unsigned gSrcDb = 0;         /* source database */
static char gGbdbGenBank[PATH_LEN];  /* root dir to store in database */

/* FIXME: maybe should drop gSrcDb ??? */

/* Update objects for each table */
static struct seqTbl* seqTbl = NULL;
static struct imageCloneTbl* imageCloneTbl = NULL;
static struct sqlUpdater* allUpdaters = NULL; /* list of tab files */
static struct sqlUpdater* gbCdnaInfoUpd = NULL;
static struct sqlUpdater* refSeqStatusUpd = NULL;
static struct sqlUpdater* refSeqSummaryUpd = NULL;
static struct sqlUpdater* refLinkUpd = NULL;
static struct sqlUpdater* gbMiscDiffUpd = NULL;
static struct sqlUpdater* gbWarnUpd = NULL;

/* other state objects */
static struct extFileTbl* extFiles = NULL;

static boolean haveGi = FALSE; /* does the gbCdnaInfo table have the gi
                                * column? */
static boolean haveMol = FALSE; /* does the gbCdnaInfo table have the mol
                                 * column? */
static boolean haveRsMol = FALSE; /* does the refSeqStatus table have the mol
                                   * column? */
static boolean haveMgc = FALSE; /* does this organism have MGC tables */
static boolean haveOrfeome = FALSE; /* does this organism have ORFeome tables */

static void prWarn(char *format, ...)
/* issue a warning */
{
va_list args;
va_start(args, format);
fflush(stdout);
fprintf(stderr, "Warning: ");
vfprintf(stderr, format, args);
fprintf(stderr, "\n");
va_end(args);
}

static void setGeneTblFlags(struct sqlConnection *conn, struct dbLoadOptions* options)
/* set gene table flags from options */
{
haveMgc = gbConfGetDbBoolean(options->conf, sqlGetDatabase(conn), "mgc");
haveOrfeome = gbConfGetDbBoolean(options->conf, sqlGetDatabase(conn), "orfeome");
}

void gbMetaDataInit(struct sqlConnection *conn, unsigned srcDb,
                    struct dbLoadOptions* options, char *gbdbGenBank,
                    char *tmpDir)
/* initialize for parsing metadata */
{
gOptions = options;
gSrcDb = srcDb;
gGbdbGenBank[0] = '\0';
if (gbdbGenBank != NULL)
    strcpy(gGbdbGenBank, gbdbGenBank);
strcpy(gTmpDir, tmpDir);

if (seqTbl == NULL)
    seqTbl = seqTblNew(conn, gTmpDir, (gbVerbose >= 4));
if (imageCloneTbl == NULL)
    imageCloneTbl = imageCloneTblNew(conn, gTmpDir, (gbVerbose >= 4));

if (!sqlTableExists(conn, "gbCdnaInfo"))
    {
    sqlUpdate(conn, gbCdnaInfoCreate);
    haveGi = TRUE;
    haveMol = TRUE;
    }
else
    {
    haveGi = sqlFieldIndex(conn, "gbCdnaInfo", "gi") >= 0;
    haveMol = sqlFieldIndex(conn, "gbCdnaInfo", "mol") >= 0;
    if (haveMol && !haveGi)
        errAbort("must have gi column to have mol");
    }

setGeneTblFlags(conn, options);

if (!sqlTableExists(conn, "gbMiscDiff"))
    sqlUpdate(conn, gbMiscDiffCreate);
if (!sqlTableExists(conn, "gbWarn"))
    sqlUpdate(conn, gbWarnCreate);

if (gbCdnaInfoUpd == NULL)
    gbCdnaInfoUpd = sqlUpdaterNew("gbCdnaInfo", gTmpDir, (gbVerbose >= 4), &allUpdaters);

if (gSrcDb == GB_REFSEQ)
    {
    if (!sqlTableExists(conn, "refSeqStatus"))
        {
        sqlUpdate(conn, refSeqStatusCreate);
        haveRsMol = TRUE;
        }
    else
        {
        haveRsMol = sqlFieldIndex(conn, "refSeqStatus", "mol") >= 0;
        }
    if (refSeqStatusUpd == NULL)
        refSeqStatusUpd = sqlUpdaterNew("refSeqStatus", gTmpDir, (gbVerbose >= 4),
                                        &allUpdaters);
    if (!sqlTableExists(conn, "refSeqSummary"))
        sqlUpdate(conn, refSeqSummaryCreate);
    if (refSeqSummaryUpd == NULL)
        refSeqSummaryUpd = sqlUpdaterNew("refSeqSummary", gTmpDir, (gbVerbose >= 4),
                                         &allUpdaters);
    if (!sqlTableExists(conn, "refLink"))
        sqlUpdate(conn, refLinkCreate);
    if (refLinkUpd == NULL)
        refLinkUpd = sqlUpdaterNew("refLink", gTmpDir, (gbVerbose >= 4),
                                   &allUpdaters);
    }
}

static boolean partitionMayHaveGeneTbls(struct gbSelect* select)
/* determine if its possible for the select alignment tracks to
 * have associated gene tracks. */
 {
return (select->release->srcDb == GB_REFSEQ)
    || ((select->type == GB_MRNA) && (haveMgc || haveOrfeome));
}

static boolean inGeneTbls(struct gbStatus* status)
/* determine if an entry is in one or more gene tracks */
{
return (status->srcDb == GB_REFSEQ)
    || (status->isMgcFull && haveMgc)
    || (status->isOrfeome && haveOrfeome);
}

static HGID getExtFileId(struct sqlConnection *conn, char* relPath)
/* get the extFile id for a file, prepending the gbdb root dir */
{
char path[PATH_LEN];
path[0] = '\0';
if (gGbdbGenBank[0] != '\0')
    {
    strcpy(path, gGbdbGenBank);
    strcat(path, "/");
    }
strcat(path, relPath);

if (extFiles == NULL)
    {
    gbVerbMsg(4, "loading extFile table");
    extFiles = extFileTblLoad(conn);
    }
return extFileTblGet(extFiles, conn, path);
}

static void seqUpdate(struct gbStatus* status, HGID faFileId)
/* Update the seq table for the current entry */
{
assert((status->stateChg & GB_NEW) || (status->gbSeqId != 0));
if (status->stateChg & GB_NEW)
    {
    char* seqType = (status->type == GB_MRNA) ?  SEQ_MRNA : SEQ_EST;
    char* seqSrcDb = (gSrcDb == GB_GENBANK) ? SEQ_GENBANK : SEQ_REFSEQ;
    assert(status->gbSeqId == 0);

    status->gbSeqId = seqTblAdd(seqTbl, raAcc, raVersion, seqType, seqSrcDb,
                                faFileId, raDnaSize, raFaOff, raFaSize);
    }
else if (status->stateChg & (GB_SEQ_CHG|GB_EXT_CHG))
    {
    /* Update only the columns that need to change */
    HGID fileId = (status->stateChg & (GB_SEQ_CHG|GB_EXT_CHG))
        ? faFileId : 0;
    short version =  (status->stateChg & GB_SEQ_CHG)
        ? raVersion : -1;
    seqTblMod(seqTbl, status->gbSeqId, version, fileId, raDnaSize, raFaOff,
              raFaSize);
    }
}

static void gbCdnaInfoUpdate(struct gbStatus* status, struct sqlConnection *conn)
/* Update the mrna table for the current entry */
{
if (status->stateChg & GB_NEW)
    {
    if (haveMol)
        sqlUpdaterAddRow(gbCdnaInfoUpd, "%u\t%s\t%u\t%s\t%s\t%c\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%s",
                         status->gbSeqId, raAcc, raVersion, gbFormatDate(raModDate),
                         ((status->type == GB_MRNA) ? "mRNA" : "EST"), raDir,
                         raFieldCurId("src"), raFieldCurId("org"),
                         raFieldCurId("lib"), raFieldCurId("clo"),
                         raFieldCurId("sex"), raFieldCurId("tis"),
                         raFieldCurId("dev"), raFieldCurId("cel"),
                         raFieldCurId("cds"), raFieldCurId("key"),
                         raFieldCurId("def"), raFieldCurId("gen"),
                         raFieldCurId("pro"), raFieldCurId("aut"),
                         raGi, raMol);
    else if (haveGi)
        sqlUpdaterAddRow(gbCdnaInfoUpd, "%u\t%s\t%u\t%s\t%s\t%c\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u",
                         status->gbSeqId, raAcc, raVersion, gbFormatDate(raModDate),
                         ((status->type == GB_MRNA) ? "mRNA" : "EST"), raDir,
                         raFieldCurId("src"), raFieldCurId("org"),
                         raFieldCurId("lib"), raFieldCurId("clo"),
                         raFieldCurId("sex"), raFieldCurId("tis"),
                         raFieldCurId("dev"), raFieldCurId("cel"),
                         raFieldCurId("cds"), raFieldCurId("key"),
                         raFieldCurId("def"), raFieldCurId("gen"),
                         raFieldCurId("pro"), raFieldCurId("aut"),
                         raGi);
    else
        sqlUpdaterAddRow(gbCdnaInfoUpd, "%u\t%s\t%u\t%s\t%s\t%c\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u",
                         status->gbSeqId, raAcc, raVersion, gbFormatDate(raModDate),
                         ((status->type == GB_MRNA) ? "mRNA" : "EST"), raDir,
                         raFieldCurId("src"), raFieldCurId("org"),
                         raFieldCurId("lib"), raFieldCurId("clo"),
                         raFieldCurId("sex"), raFieldCurId("tis"),
                         raFieldCurId("dev"), raFieldCurId("cel"),
                         raFieldCurId("cds"), raFieldCurId("key"),
                         raFieldCurId("def"), raFieldCurId("gen"),
                         raFieldCurId("pro"), raFieldCurId("aut"));
    }
else if (status->stateChg & GB_META_CHG)
    {
    if (haveMol)
        sqlUpdaterModRow(gbCdnaInfoUpd, 1, "version='%u', moddate='%s', direction='%c', "
                         "source=%u, organism=%u, library=%u, mrnaClone=%u, sex=%u, "
                         "tissue=%u, development=%u, cell=%u, cds=%u, keyword=%u, "
                         "description=%u, geneName=%u, productName=%u, author=%u, gi=%u, mol='%s' "
                         "WHERE id=%u",
                         raVersion, gbFormatDate(raModDate), raDir,
                         raFieldCurId("src"), raFieldCurId("org"),
                         raFieldCurId("lib"), raFieldCurId("clo"),
                         raFieldCurId("sex"), raFieldCurId("tis"),
                         raFieldCurId("dev"), raFieldCurId("cel"),
                         raFieldCurId("cds"), raFieldCurId("key"),
                         raFieldCurId("def"), raFieldCurId("gen"),
                         raFieldCurId("pro"), raFieldCurId("aut"),
                         raGi, raMol, status->gbSeqId);
    else if (haveGi)
        sqlUpdaterModRow(gbCdnaInfoUpd, 1, "version='%u', moddate='%s', direction='%c', "
                         "source=%u, organism=%u, library=%u, mrnaClone=%u, sex=%u, "
                         "tissue=%u, development=%u, cell=%u, cds=%u, keyword=%u, "
                         "description=%u, geneName=%u, productName=%u, author=%u, gi=%u "
                         "WHERE id=%u",
                         raVersion, gbFormatDate(raModDate), raDir,
                         raFieldCurId("src"), raFieldCurId("org"),
                         raFieldCurId("lib"), raFieldCurId("clo"),
                         raFieldCurId("sex"), raFieldCurId("tis"),
                         raFieldCurId("dev"), raFieldCurId("cel"),
                         raFieldCurId("cds"), raFieldCurId("key"),
                         raFieldCurId("def"), raFieldCurId("gen"),
                         raFieldCurId("pro"), raFieldCurId("aut"),
                         raGi, status->gbSeqId);
    else
        sqlUpdaterModRow(gbCdnaInfoUpd, 1, "version='%u', moddate='%s', direction='%c', "
                         "source=%u, organism=%u, library=%u, mrnaClone=%u, sex=%u, "
                         "tissue=%u, development=%u, cell=%u, cds=%u, keyword=%u, "
                         "description=%u, geneName=%u, productName=%u, author=%u "
                         "WHERE id=%u",
                         raVersion, gbFormatDate(raModDate), raDir,
                         raFieldCurId("src"), raFieldCurId("org"),
                         raFieldCurId("lib"), raFieldCurId("clo"),
                         raFieldCurId("sex"), raFieldCurId("tis"),
                         raFieldCurId("dev"), raFieldCurId("cel"),
                         raFieldCurId("cds"), raFieldCurId("key"),
                         raFieldCurId("def"), raFieldCurId("gen"),
                         raFieldCurId("pro"), raFieldCurId("aut"),
                         status->gbSeqId);
    }
}

static void imageCloneUpdate(struct gbStatus* status, struct sqlConnection *conn)
/* update image clone table */
{
/* assumes image id is never removed; most like true, would only
 * be changed on a mistake */
if (status->stateChg & (GB_NEW|GB_META_CHG))
    {
    unsigned imageId = imageCloneGBParse(raFieldCurVal("clo"));
    if (imageId != 0)
        {
        if (status->stateChg & GB_NEW)
            {
            imageCloneTblAdd(imageCloneTbl, imageId, status->acc, status->type,
                             raDir);
            }
        else if (status->stateChg & GB_META_CHG)
            {
            unsigned oldImageId = imageCloneTblGetId(conn, status->acc);
            if (oldImageId == 0)
                imageCloneTblAdd(imageCloneTbl, imageId, status->acc, status->type,
                                 raDir);
            else if (imageId != oldImageId)
                imageCloneTblMod(imageCloneTbl, imageId, status->acc, raDir);
            }
        }
    }
}

static void gbMiscDiffUpdate(struct gbStatus* status, struct sqlConnection *conn)
/* update gbMiscDiff table */
{
if (status->stateChg & (GB_NEW|GB_META_CHG|GB_REBUILD_DERIVED))
    {
    if (gbMiscDiffUpd == NULL)
        gbMiscDiffUpd = sqlUpdaterNew("gbMiscDiff", gTmpDir, (gbVerbose >= 4), &allUpdaters);
    struct gbMiscDiff *gmd;
    for (gmd = raMiscDiffs; gmd != NULL; gmd = gmd->next)
        sqlUpdaterAddRow(gbMiscDiffUpd, "%s\t%d\t%d\t%s\t%s\t%s",
                         gmd->acc, gmd->mrnaStart, gmd->mrnaEnd,
                         gbSqlStrOrNullTabVar(gmd->notes),
                         gbSqlStrOrNullTabVar(gmd->gene),
                         gbSqlStrOrNullTabVar(gmd->replacement));
    }
}

static void gbWarnUpdate(struct gbStatus* status, struct sqlConnection *conn)
/* update gbWarn table */
{
if (status->stateChg & (GB_NEW|GB_META_CHG|GB_REBUILD_DERIVED))
    {
    if (gbWarnUpd == NULL)
        gbWarnUpd = sqlUpdaterNew("gbWarn", gTmpDir, (gbVerbose >= 4), &allUpdaters);
    sqlUpdaterAddRow(gbWarnUpd, "%s\t%s", raAcc, raWarn);
    }
}

static void refSeqStatusUpdate(struct gbStatus* status)
/* Update the refSeqStatus table for the current entry */
{
if (status->stateChg & GB_NEW)
    {
    if (haveRsMol)
        sqlUpdaterAddRow(refSeqStatusUpd, "%s\t%s\t%s", raAcc, raRefSeqStatus, raMol);
    else
        sqlUpdaterAddRow(refSeqStatusUpd, "%s\t%s", raAcc, raRefSeqStatus);
    }
else if (status->stateChg & GB_META_CHG)
    {
    if (haveRsMol)
        sqlUpdaterModRow(refSeqStatusUpd, 1, "status='%s', mol='%s' WHERE mrnaAcc='%s'",
                         raRefSeqStatus, raMol, raAcc);
    else
        sqlUpdaterModRow(refSeqStatusUpd, 1, "status='%s' WHERE mrnaAcc='%s'",
                         raRefSeqStatus, raAcc);
    }
}

static void refSeqSummaryUpdate(struct sqlConnection *conn, struct gbStatus* status)
/* Update the refSeqSummary table for the current entry */
{
/* only add to table if we actually have something */
if (((raRefSeqSummary->stringSize > 0) || (raRefSeqCompleteness != NULL))
    && (status->stateChg & (GB_NEW|GB_META_CHG)))
    {
    char query[256];
    unsigned sumNew;
    char* summary = sqlEscapeString2(alloca(2*raRefSeqSummary->stringSize+1), 
                                     raRefSeqSummary->string);
    if (raRefSeqCompleteness == NULL)
        raRefSeqCompleteness = "Unknown";
    
    /* If sequence may not be new, but summary could be.  This table is
     * sparse, so must check it */
    if (status->stateChg & GB_NEW)
        sumNew = TRUE;
    else
        {
        safef(query, sizeof(query), "SELECT count(*) from refSeqSummary "
              "WHERE mrnaAcc = '%s'", raAcc);
        sumNew = (sqlQuickNum(conn, query) == 0);
        }

    if (sumNew)
        sqlUpdaterAddRow(refSeqSummaryUpd, "%s\t%s\t%s", raAcc, raRefSeqCompleteness,
                         summary);
    else
        sqlUpdaterModRow(refSeqSummaryUpd, 1, "completeness='%s', summary='%s'"
                         " WHERE mrnaAcc='%s'", raRefSeqCompleteness,
                         summary, raAcc);
    }
}

static void refLinkUpdate(struct sqlConnection *conn, struct gbStatus* status)
/* Update the refLink table for the current entry */
{
int geneId;
char *gen = emptyForNull(raFieldCurVal("gen"));
char *pro = emptyForNull(raFieldCurVal("pro"));
gen = sqlEscapeString2(alloca(2*strlen(gen)+1), gen);
pro = sqlEscapeString2(alloca(2*strlen(pro)+1), pro);

/* can either have locus id (old locus link db) or gene id, or both,
 * in which case the geneId is used */
geneId = (raGeneId != 0) ? raGeneId : raLocusLinkId;

if (status->stateChg & GB_NEW)
    sqlUpdaterAddRow(refLinkUpd, "%s\t%s\t%s\t%s\t%u\t%u\t%u\t%u",
                     gen, pro, raAcc, raProtAcc, raFieldCurId("gen"),
                     raFieldCurId("pro"), geneId, raOmimId);
else if (status->stateChg & GB_META_CHG)
    sqlUpdaterModRow(refLinkUpd, 1, "name='%s', product='%s', protAcc='%s', "
                     "geneName=%u, prodName=%u, locusLinkId=%u, "
                     "omimId=%u where mrnaAcc='%s'",
                     gen, pro, raProtAcc, raFieldCurId("gen"),
                     raFieldCurId("pro"), geneId, raOmimId, raAcc);
}

static void refSeqPepUpdate(struct sqlConnection *conn, HGID pepFaId)
/* Make or update a seq table entry for a RefSeq peptide sequence
 * referenced a refLink table. */
{
/* done for all stateChg values */
if (raProtFaOff >= 0)
    {
    HGID gbSeqId = seqTblGetId(seqTbl, conn, raProtAcc);
    if (gbSeqId == 0)
        seqTblAdd(seqTbl, raProtAcc, raProtVersion, SEQ_PEP, SEQ_REFSEQ,
                  pepFaId, raProtSize, raProtFaOff, raProtFaSize);
    else
        seqTblMod(seqTbl, gbSeqId, raProtVersion, pepFaId, raProtSize,
                  raProtFaOff, raProtFaSize);
    }
}

static boolean keepDesc(struct gbStatus* status)
/* Check if description should be stored. */
{
return dbLoadOptionsGetAttr(gOptions, status->srcDb, status->type, status->orgCat)->loadDesc;
}

static void reComp(regex_t *preg, char* pattern)
/* compile the regular expression or abort */
{
int e = regcomp(preg, pattern, REG_EXTENDED|REG_ICASE|REG_NOSUB);
if (e != 0)
    {
    char errbuf[256];
    regerror(e, preg, errbuf, sizeof(errbuf));
    errAbort("can't compile regular expression: \"%s\": %s",
             pattern, errbuf);
    }
}

static boolean reMatches(regex_t *preg, char* str)
/* determine if strange matches regular expression */
{
int e = regexec(preg, str, 0, NULL, 0);
if (e == 0)
    return TRUE;
else if (e == REG_NOMATCH)
    return FALSE;
else
    {
    char errbuf[256];
    regerror(e, preg, errbuf, sizeof(errbuf));
    errAbort("error executing regular expresion:: %s", errbuf);
    return FALSE;
    }
}

static boolean isMgcFullLength()
/* determine if the current RA entry is for an MGC */
{
/* Check for keyword and source /clone field to identify MGC genes.  Keywords
 * end in `.' and are seperated by "; ".  So look for .
 * Clone can be like: /clone="MGC:9349 IMAGE:3846611" */
static boolean first = TRUE;
static regex_t keyRe, cloRe;
if (first)
    {
    reComp(&keyRe, "(^|.* )MGC(\\.$|;.*)");
    reComp(&cloRe, "(^|.* )MGC:[0-9]+( .*|$)");
    first = FALSE;
    }
char *key = raFieldCurVal("key");
if (key == NULL)
    return FALSE;
if (!reMatches(&keyRe, key))
    return FALSE;
char *clo = raFieldCurVal("clo");
if (clo == NULL)
    return FALSE;
if (!reMatches(&cloRe, clo))
    return FALSE;
return TRUE;
}

static boolean isOrfeome()
/* determine if the current RA entry is for an ORFeome */
{
/* keyword containing "ORFeome collaboration" */
static boolean first = TRUE;
static regex_t keyRe;
if (first)
    {
    reComp(&keyRe, "(^|.* )ORFeome collaboration(\\.$|;.*)");
    first = FALSE;
    }
char *key = raFieldCurVal("key");
return (key != NULL) && reMatches(&keyRe, key);
}

static void updateMetaData(struct sqlConnection *conn, struct gbStatus* status,
                           struct gbStatusTbl* statusTbl, HGID faFileId,
                           HGID pepFaId)
/* update the database tables for the current entry based on the stateChg
 * flags */
{
assert(status->stateChg & (GB_NEW|GB_META_CHG|GB_REBUILD_DERIVED));

/* check for MGC, ORFeome */
if (status->orgCat == GB_NATIVE)
    {
    if (haveMgc)
        status->isMgcFull = isMgcFullLength();
    if (haveOrfeome)
        status->isOrfeome = isOrfeome();
    }

/* clear description if we are not keeping it */
if (!keepDesc(status))
    raFieldClear("def");

/* most database changes are only done for GB_EXT_CHG */

if (status->stateChg & (GB_NEW|GB_META_CHG))
    {
    seqUpdate(status, faFileId);  /* must be first to get status->gbSeqId */
    gbCdnaInfoUpdate(status, conn);
    imageCloneUpdate(status, conn);
    }
if (raMiscDiffs != NULL)
    gbMiscDiffUpdate(status, conn);
if (raWarn != NULL)
    gbWarnUpdate(status, conn);
if ((gSrcDb == GB_REFSEQ) && (status->stateChg & (GB_NEW|GB_META_CHG)))
    {
    refSeqStatusUpdate(status);
    refSeqSummaryUpdate(conn, status);
    refLinkUpdate(conn, status);
    refSeqPepUpdate(conn, pepFaId);
    }

/* update in-memory gbStatus entry  */
status->modDate = raModDate;

/* save CDS for use by the alignments */
if (!genbankCdsParse(raCds, &status->cds))
    {
    /* not valid CDS, only warn if RefSeq, where we expect to be better */
    if ((gSrcDb == GB_REFSEQ) && gbIsProteinCodingRefSeq(status->acc))
        prWarn("%s: malformed RefSeq CDS: %s", status->acc, raCds);
    }

/* geneName for refFlat, if not available, try locus_tag  */
char *geneName = raFieldCurVal("gen");
if (geneName == NULL)
    geneName = raFieldCurVal("lot");
if (geneName != NULL)
    status->geneName = lmCloneString(statusTbl->accHash->lm, geneName);

/* mark as done so dups in other updates don't get added to the database */
status->metaDone = TRUE;
}

static boolean processEntry(struct sqlConnection *conn,
                            struct gbStatusTbl* statusTbl,
                            HGID faFileId, HGID pepFaId)
/* Parse and process the next ra entry, check to see if it is selected
 * for update. */
{
char* acc;
struct gbStatus* status;

if ((acc = gbMDParseEntry()) == NULL)
    return FALSE;

status = gbStatusTblFind(statusTbl, acc);
if ((status != NULL) && (status->selectProc != NULL) && (!status->metaDone)
    && (status->selectProc->modDate == raModDate))
    updateMetaData(conn, status, statusTbl, faFileId, pepFaId);
return TRUE;
}

static void metaDataProcess(struct sqlConnection *conn,
                            struct gbStatusTbl* statusTbl,
                            char* raPath,
                            HGID faFileId, HGID pepFaId)
/* Parse a ra file looking for accessions to add to the database.  If the
 * ra entry matches the status->selectProc object, it will be saved for
 * loading. */
{
/* parse and save all ra records, in assending file offset order */
gbMDParseOpen(raPath, conn, gOptions, gTmpDir);
while (processEntry(conn, statusTbl, faFileId, pepFaId))
    continue; /* loop filler */
gbMDParseClose();
}

static HGID getPeptideFaId(struct sqlConnection *conn, struct gbSelect* select)
/* Get the extFile id for a refseq peptide fasta file */
{
HGID pepFaId = 0;
char pepFaPath[PATH_LEN];

if (gbProcessedGetPepFa(select, pepFaPath))
    {
    if (fileExists(pepFaPath))
        pepFaId = getExtFileId(conn, pepFaPath);
    }
return pepFaId;
}

void gbMetaDataProcess(struct sqlConnection *conn,
                       struct gbStatusTbl* statusTbl,
                       struct gbSelect* select)
/* process metadata for an update */
{
HGID faFileId, pepFaId = 0;
char raPath[PATH_LEN], faPath[PATH_LEN];

/* need fasta file id */
gbProcessedGetPath(select, "fa", faPath);
faFileId = getExtFileId(conn, faPath);

if (select->release->srcDb == GB_REFSEQ)
    pepFaId = getPeptideFaId(conn, select);

/* parse and save for loading metadata from this update */
gbProcessedGetPath(select, "ra.gz", raPath);
metaDataProcess(conn, statusTbl, raPath, faFileId, pepFaId);
}

static void updateGeneEntries(struct sqlConnection *conn,
                              struct gbGeneTblSet *ggts,
                              struct gbStatus* status)
/* update gene table entries when annotation have changed */
{
if (status->srcDb == GB_REFSEQ)
    {
    struct gbGeneTbl *geneTbl = (status->orgCat == GB_NATIVE)
        ? gbGeneTblSetRefGeneGet(ggts, conn)
        : gbGeneTblSetXenoRefGeneGet(ggts, conn);
    gbGeneTblRebuild(geneTbl, status, conn);
    }
else 
    {
    // can be both MGC and orfeome
    if (status->isMgcFull)
        gbGeneTblRebuild(gbGeneTblSetMgcGenesGet(ggts, conn), status, conn);
    if (status->isOrfeome)
        gbGeneTblRebuild(gbGeneTblSetOrfeomeGenesGet(ggts, conn), status, conn);
    }
}

void gbMetaDataUpdateChgGenes(struct sqlConnection *conn,
                              struct gbSelect *select,
                              struct gbStatusTbl* statusTbl,
                              char *tmpDir)
/* update gene tables where annotations have changed but sequence
 * has not changes and is not being reloaded */
{
if (partitionMayHaveGeneTbls(select))
    {
    struct gbGeneTblSet *ggts = gbGeneTblSetNew(tmpDir);
    struct gbStatus* status;
    for (status = statusTbl->metaChgList; status != NULL; status = status->next)
        {
        if (inGeneTbls(status) && !(status->stateChg & GB_SEQ_CHG))
            updateGeneEntries(conn, ggts, status);  // ext chg or rebuild derived
        }
    gbGeneTblSetCommit(ggts, conn);
    gbGeneTblSetFree(&ggts);
    }
}

void gbMetaDataDbLoad(struct sqlConnection *conn)
/* load the metadata changes into the database */
{
struct sqlUpdater *nextUpd;

/* this should never have been called if these tables not set up */
assert(imageCloneTbl != NULL);
assert(seqTbl != NULL);

/* seq must be first */
seqTblCommit(seqTbl, conn);
seqTblFree(&seqTbl);

/* unique string tables next, before mrna */
gbMDParseCommit(conn);

/* image ids are loaded next */
imageCloneTblCommit(imageCloneTbl, conn);

/* other metadata */
while ((nextUpd = slPopHead(&allUpdaters)) != NULL)
    {
    sqlUpdaterCommit(nextUpd, conn);
    sqlUpdaterFree(&nextUpd);
    }
gbCdnaInfoUpd = NULL;
refSeqStatusUpd = NULL;
refSeqSummaryUpd = NULL;
refLinkUpd = NULL;
gbMiscDiffUpd = NULL;
gbWarnUpd = NULL;
/* cache unique string tables in goFaster mode */
if ((gOptions->flags & DBLOAD_GO_FASTER) == 0)
    gbMDParseFree();
}

void gbMetaDataFree()
/* Free data structures */
{
gbMDParseFree();
extFileTblFree(&extFiles);
}

void gbMetaDataDeleteFromIdTables(struct sqlConnection *conn, struct dbLoadOptions* options,
                                  struct sqlDeleter* deleter)
/* delete sequence from metadata tables with ids.  These are always
 * deleted and rebuilt even for modification */
{
gOptions = options;
setGeneTblFlags(conn, options);
sqlDeleterDel(deleter, conn, IMAGE_CLONE_TBL, "acc");
sqlDeleterDel(deleter, conn, "gbMiscDiff", "acc");
sqlDeleterDel(deleter, conn, "gbWarn", "acc");
}

static void deleteFromGeneTbls(struct sqlConnection *conn, struct gbSelect* select, 
                               struct sqlDeleter* deleter)
/* delete accession from gene tables.  Used when metaData has changed, but
 * sequence has not changed. */
{
if (select->release->srcDb == GB_REFSEQ)
    {
    if (select->orgCats & GB_NATIVE)
        {
        sqlDeleterDel(deleter, conn, REF_GENE_TBL, "name");
        sqlDeleterDel(deleter, conn, REF_FLAT_TBL, "name");
        }
    if (select->orgCats & GB_XENO)
        {
        sqlDeleterDel(deleter, conn, XENO_REF_GENE_TBL, "name");
        sqlDeleterDel(deleter, conn, XENO_REF_FLAT_TBL, "name");
        }
    }
else
    {
    if (haveMgc)
        {
        sqlDeleterDel(deleter, conn, MGC_GENES_TBL, "name");
        sqlDeleterDel(deleter, conn, MGC_FULL_MRNA_TBL, "qName");
        }
    if (haveOrfeome)
        {
        sqlDeleterDel(deleter, conn, ORFEOME_GENES_TBL, "name");
        sqlDeleterDel(deleter, conn, ORFEOME_MRNA_TBL, "qName");
        }
    }
}

void gbMetaDataDeleteFromTables(struct sqlConnection *conn, struct dbLoadOptions* options,
                                unsigned srcDb, struct sqlDeleter* deleter)
/* delete sequence from metadata tables */
{
gOptions = options;
setGeneTblFlags(conn, options);
if (srcDb == GB_REFSEQ)
    {
    sqlDeleterDel(deleter, conn, "refSeqStatus", "mrnaAcc");
    sqlDeleterDel(deleter, conn, "refSeqSummary", "mrnaAcc");
    sqlDeleterDel(deleter, conn, "refLink", "mrnaAcc");
    }
sqlDeleterDel(deleter, conn, "gbMiscDiff", "acc");
sqlDeleterDel(deleter, conn, "gbWarn", "acc");
sqlDeleterDel(deleter, conn, IMAGE_CLONE_TBL, "acc");
sqlDeleterDel(deleter, conn, "gbCdnaInfo", "acc");
sqlDeleterDel(deleter, conn, "gbSeq", "acc");
/* seq must be last */
sqlDeleterDel(deleter, conn, SEQ_TBL, "acc");
}

static void refSeqPepClean(struct sqlConnection *conn)
/* Delete all refseq peptides that are in gbSeq but no longer
 * referenced by refLink.  */
{
char query[1024];
struct sqlResult* sr;
char **row;
struct sqlDeleter* deleter;

/* don't do anything if we don't have the refLink table.  This can
 * happen if refSeq was enabled after the initial load */
if (!sqlTableExists(conn, "refLink"))
    return;

deleter = sqlDeleterNew(gTmpDir, (gbVerbose >= 4));

/* Use a join to get list of acc, which proved reasonable fastly because
 * the the list is small */
safef(query, sizeof(query), "SELECT acc FROM gbSeq LEFT JOIN refLink ON (refLink.protAcc = gbSeq.acc) "
      "WHERE (acc LIKE 'NP_%%') AND (refLink.protAcc IS NULL)");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    sqlDeleterAddAcc(deleter, row[0]);
sqlFreeResult(&sr);

sqlDeleterDel(deleter, conn, SEQ_TBL, "acc");
sqlDeleterFree(&deleter);
}

void gbMetaDataDeleteOutdated(struct sqlConnection *conn,
                              struct gbSelect* select,
                              struct gbStatusTbl* statusTbl,
                              struct dbLoadOptions* options,
                              char *tmpDir)
/* Delete outdated metadata.  Also delete genePred table entries for genes
 * where metadata changed but sequence has not.  These will have the genePred
 * records reloaded.*/
{
setGeneTblFlags(conn, options);
struct sqlDeleter* deleter = sqlDeleterNew(tmpDir, (gbVerbose >= 4));
struct sqlDeleter* geneTblDeleter = NULL;
struct sqlDeleter* derivedTblDeleter = NULL;
if (partitionMayHaveGeneTbls(select))
    geneTblDeleter = sqlDeleterNew(tmpDir, (gbVerbose >= 4));
if (select->type == GB_MRNA)
    derivedTblDeleter = sqlDeleterNew(tmpDir, (gbVerbose >= 4));
struct gbStatus* status;
gSrcDb = select->release->srcDb;
gOptions = options;
strcpy(gTmpDir, tmpDir);

/* Delete any meta modified from id tables */
for (status = statusTbl->seqChgList; status != NULL; status = status->next)
    {
    if (status->stateChg & GB_META_CHG)
        sqlDeleterAddAcc(deleter, status->acc);
    }
for (status = statusTbl->metaChgList; status != NULL; status = status->next)
    {
    assert(!(status->stateChg&GB_SEQ_CHG));
    if (status->stateChg&GB_META_CHG)
        sqlDeleterAddAcc(deleter, status->acc);
    else if (status->stateChg&GB_REBUILD_DERIVED)
        sqlDeleterAddAcc(derivedTblDeleter, status->acc);
    if (geneTblDeleter != NULL)
        {
        // need to just try, since we can set the status->isMgcFull
        // flag until we are reading the ra.
        sqlDeleterAddAcc(geneTblDeleter, status->acc);
        }
    }
for (status = statusTbl->deleteList; status != NULL; status = status->next)
    sqlDeleterAddAcc(deleter, status->acc);
for (status = statusTbl->orphanList; status != NULL; status = status->next)
    sqlDeleterAddAcc(deleter, status->acc);
gbMetaDataDeleteFromIdTables(conn, options, deleter);
sqlDeleterFree(&deleter);

/* remove deleted and orphans from metadata. */
deleter = sqlDeleterNew(tmpDir, (gbVerbose >= 4));
for (status = statusTbl->deleteList; status != NULL; status = status->next)
    sqlDeleterAddAcc(deleter, status->acc);
for (status = statusTbl->orphanList; status != NULL; status = status->next)
    sqlDeleterAddAcc(deleter, status->acc);

// must do gene tbls before other tables
if (geneTblDeleter != NULL)
    deleteFromGeneTbls(conn, select, geneTblDeleter);
gbMetaDataDeleteFromTables(conn, options, select->release->srcDb, deleter);

sqlDeleterFree(&deleter);
sqlDeleterFree(&geneTblDeleter);
sqlDeleterFree(&derivedTblDeleter);

/* If we are cleaning up the ext table, we need to get rid of any
 * refseq peptides in gbSeq that are no longer referenced.  We don't
 * do it other times as these are not reachable directly.
 */
if ((select->release->srcDb == GB_REFSEQ)
    && (gOptions->flags & DBLOAD_EXT_FILE_UPDATE))
    refSeqPepClean(conn);
} 

void gbMetaDataRemove(struct sqlConnection *conn,
                      struct dbLoadOptions* options,
                      struct gbSelect* select,
                      struct sqlDeleter* deleter)
/* remove metaData from all entries in the select categories.
 * Used when reloading. */
{
gOptions = options;
setGeneTblFlags(conn, options);
if (partitionMayHaveGeneTbls(select))
    deleteFromGeneTbls(conn, select, deleter);
gbMetaDataDeleteFromTables(conn, options, select->release->srcDb, deleter);
}

struct slName* gbMetaDataListTables(struct sqlConnection *conn)
/* Get a list of metadata tables that exist in the database */
{
static char* TABLE_NAMES[] = {
    "gbSeq", "gbExtFile", "gbCdnaInfo", "refSeqStatus", "refSeqSummary", "refLink",
    "imageClone", "gbMiscDiff", "gbWarn", NULL
};
struct slName* tables = NULL;
int i;

for (i = 0; TABLE_NAMES[i] != NULL; i++)
    gbAddTableIfExists(conn, TABLE_NAMES[i], &tables);
for (i = 0; raFieldTables[i] != NULL; ++i)
    gbAddTableIfExists(conn, raFieldTables[i], &tables);
return tables;
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

