/* Object to load the metadata from database and verify */
#include "common.h"
#include "chkCommon.h"
#include "gbDefs.h"
#include "chkMetaDataTbls.h"
#include "chkSeqTbl.h"
#include "metaData.h"
#include "gbFileOps.h"
#include "gbEntry.h"
#include "gbUpdate.h"
#include "gbAligned.h"
#include "gbProcessed.h"
#include "gbRelease.h"
#include "gbGenome.h"
#include "gbVerb.h"
#include "localmem.h"
#include "jksql.h"
#include "hdb.h"


static char* validRefSeqStatus[] = {
    "Unknown", "Reviewed", "Validated", "Provisional", "Predicted", "Inferred", NULL
};

static void loadGbCdnaInfoRow(struct metaDataTbls* metaDataTbls,
                              struct sqlConnection* conn, char** row)
/* load one row from the gbCdnaInfo table */
{
struct metaData* md;
int len, numNonZero, iRow = 0;
char *acc, *dir;
boolean gotError, isOk;

/* columns: acc,id,moddate,version,moddate,type */
acc = row[iRow++];
verbose(3, "got %s in gbCdnaInfo table\n", acc);
md = metaDataTblsGet(metaDataTbls, acc);
if (md->inGbCdnaInfo)
    {
    gbError("%s: acc occurs multiple times in the mrna table", acc);
    return;
    }
md->inGbCdnaInfo = TRUE;
md->gbCdnaInfoId = strToUnsigned(row[iRow++], acc, "gbCdnaInfo.id", NULL);
len = strlen(acc);
md->gbCdnaInfoVersion = strToUnsigned(row[iRow++], "gbCdnaInfo.version", acc, &gotError);
if (!gotError && (md->gbCdnaInfoVersion <= 0))
     gbError("%s: gbCdnaInfo.version invalid: \"%d\"", acc, md->gbCdnaInfoVersion);
isOk = TRUE;
md->gbCdnaInfoModdate = gbParseChkDate(row[iRow++], &isOk);
if (!isOk)
    gbError("%s: invalid gbCdnaInfo.moddate value: \"%s\"", acc, row[iRow-1]);
md->gbCdnaInfoType = gbParseType(row[iRow++]);
md->typeFlags |= md->gbCdnaInfoType;

dir = row[iRow++];
if ((strlen(dir) > 1) || (strchr("053", *dir) == NULL))
    gbError("%s: invalid gbCdnaInfo.direction value: \"%s\"", acc, dir);

/* Make sure that at least a few of the id fields have data  */
numNonZero = 0;
while (iRow < 20)
    {
    int id = strToUnsigned(row[iRow++], md->acc, "gbCdnaInfo.?", NULL);
    if (id > 0)
        numNonZero++;
    /* remember if we have a description */
    if (iRow-1 == 16)
        md->haveDesc = (id != 0);
    }
if (numNonZero == 0)
    gbError("%s: none of gbCdnaInfo string ids have non-zero values", dir);
else if (numNonZero < 3)
    gbError("%s: accession %s: only %d of gbCdnaInfo string ids have non-zero values",
            acc,dir, numNonZero);
}

static void loadGbCdnaInfoData(struct metaDataTbls* metaDataTbls,
                               struct gbSelect* select, struct sqlConnection* conn)
/* load the gbCdnaInfo table */
{
char accWhere[64];
char query[512];
struct sqlResult* result;
char** row;

gbVerbMsg(2, "load gbCdnaInfo table data");
accWhere[0] = '\0';
if (select->accPrefix != NULL)
    safef(accWhere, sizeof(accWhere), " AND (acc LIKE '%s%%')",
          select->accPrefix);
safef(query, sizeof(query), 
      "SELECT acc,id,version,moddate,type,direction,"
      /*        0  1       2       3    4         5 */
      "source,organism,library,mrnaClone,sex,tissue,development,cell,cds,"
      /*    6        7       8         9  10     11          12   13  14 */
      "keyword,description,geneName,productName,author "
      /*    15          16       17          18     19 */
      "FROM gbCdnaInfo WHERE (type='%s')%s",
      ((select->type == GB_MRNA) ? "mRNA" : "EST"), accWhere);
/* mrna doesn't have a srcDb, so we guess from acc */
result = sqlGetResult(conn, query);
while ((row = sqlNextRow(result)) != NULL)
    {
    if (gbGuessSrcDb(row[0]) == select->release->srcDb)
        loadGbCdnaInfoRow(metaDataTbls, conn, row);
    }
sqlFreeResult(&result);
}

static void loadRefSeqStatusRow(struct metaDataTbls* metaDataTbls,
                                struct sqlConnection* conn,
                                char** row)
/* load a row of the refSeqStatus table */
{
int i;
char* rssStatus;
struct metaData* md;

/* columns: mrnaAcc,status */
md = metaDataTblsGet(metaDataTbls, row[0]);
if (md->inRefSeqStatus)
    gbError("%s: occurs multiple times in the refSeqStatus table", md->acc);
md->inRefSeqStatus = TRUE;
rssStatus = row[1];
for (i = 0; (validRefSeqStatus[i] != NULL) 
         && !sameString(rssStatus, validRefSeqStatus[i]); i++)
    continue;
if (validRefSeqStatus[i] == NULL)
    gbError("%s: invalid refSeqStatus.status", md->acc);
}

static void loadRefSeqStatus(struct metaDataTbls* metaDataTbls,
                            struct sqlConnection* conn)
/* load the refSeqStatus table */
{
struct sqlResult* result;
char** row;

gbVerbMsg(2, "load refSeqStatus table data");

result = sqlGetResult(conn, "SELECT mrnaAcc,status FROM refSeqStatus");
while ((row = sqlNextRow(result)) != NULL)
    loadRefSeqStatusRow(metaDataTbls, conn, row);
sqlFreeResult(&result);
}

static void loadRefLinkRow(struct metaDataTbls* metaDataTbls,
                           struct sqlConnection* conn, char** row)
/* load a row of the refLink table */
{
/* columns: mrnaAcc,name,product,protAcc,geneName,prodName,locusLinkId,omimId */
struct metaData* md;
int iRow = 0;
char *acc = row[iRow++];
char *product;

if (!(startsWith("NM_", acc) || startsWith("NR_", acc)))
    {
    gbError("%s: non-NM_/NR_ mrnaAcc in refLink", acc);
    return;
    }

md = metaDataTblsGet(metaDataTbls, acc);
if (md->inRefLink)
    gbError("%s: occurs multiple times in the refLink table", md->acc);
md->inRefLink = TRUE;
safef(md->rlName, sizeof(md->rlName), "%s", row[iRow++]);
product = row[iRow++];
safef(md->rlProtAcc, sizeof(md->rlProtAcc), "%s", row[iRow++]);

/* check if ids are valid (zero is allowed, so just parse) */
strToUnsigned(row[iRow++], md->acc, "refLink.geneName", NULL);
strToUnsigned(row[iRow++], md->acc, "refLink.prodName", NULL);
strToUnsigned(row[iRow++], md->acc, "refLink.locusLinkId", NULL);
strToUnsigned(row[iRow++], md->acc, "refLink.omimId", NULL);

if (gbIsProteinCodingRefSeq(md->acc))
    {
    if (strlen(md->rlProtAcc) == 0)
        gbError("%s: empty protein acc in refLink", acc);
    else
        {
        metaDataTblsAddProtAcc(metaDataTbls, md);
        md->hasProt = TRUE;
        }
    }
}

static void loadRefLink(struct metaDataTbls* metaDataTbls,
                        struct sqlConnection* conn)
/* load the refLink table */
{
struct sqlResult* result;
char** row;

gbVerbMsg(2, "load relLink table data");

result = sqlGetResult(conn, "SELECT mrnaAcc,name,product,protAcc,geneName,"
                      "prodName,locusLinkId,omimId from refLink");
while ((row = sqlNextRow(result)) != NULL)
    loadRefLinkRow(metaDataTbls, conn, row);
sqlFreeResult(&result);
}

static void loadGbStatusRow(struct metaDataTbls* metaDataTbls,
                            struct sqlConnection* conn, char** row,
                            unsigned descOrgCats)
/* load a row of the gbStatus table */
{
struct metaData* md;
int iRow = 0;
boolean isOk;
HGID seqId;

/* columns: acc,version,modDate,type,srcDb,gbSeq,numAligns */

md = metaDataTblsGet(metaDataTbls, row[iRow++]);
if (md->inGbStatus)
    gbError("%s: occurs multiple times in the gbStatus table", md->acc);
md->inGbStatus = TRUE;
md->gbsVersion = strToUnsigned(row[iRow++], md->acc, "gbStatus.version", NULL);

isOk = TRUE;
md->gbsModDate = gbParseChkDate(row[iRow++], &isOk);
if (!isOk)
    gbError("%s: invalid gbStatus.moddate value: \"%s\"", md->acc, row[iRow-1]);

md->gbsType = gbParseType(row[iRow++]);
md->gbsSrcDb = gbParseSrcDb(row[iRow++]);
md->gbsOrgCat = gbParseOrgCat(row[iRow++]);
seqId = strToUnsigned(row[iRow++], md->acc, "gbStatus.gbSeq", NULL);
md->gbsNumAligns = strToUnsigned(row[iRow++], md->acc, "gbStatus.numAligns",
                                 NULL);

md->typeFlags |= md->gbsType;

if (md->inGbCdnaInfo)
    {
    if (seqId != md->gbCdnaInfoId)
        gbError("%s: gbStatus.gbSeq (%d) not same gbCdnaInfo.id (%d)", md->acc, seqId,
                md->gbCdnaInfoId);
    if (md->gbsType != md->gbCdnaInfoType)
        gbError("%s: gbStatus.type (%s) not same as gbCdnaInfo.type (%s)", md->acc,
                gbFmtSelect(md->gbsType), gbFmtSelect(md->gbCdnaInfoType));
    if (md->gbsSrcDb != (md->typeFlags & GB_SRC_DB_MASK))
        gbError("%s: gbStatus.srcDb (%s) not same gbCdnaInfo.srcDb (%s)", md->acc,
                gbFmtSelect(md->gbsSrcDb), gbFmtSelect(md->typeFlags));
    if (md->gbsVersion != md->gbCdnaInfoVersion)
        gbError("%s: gbStatus.version (%d) not same gbCdnaInfo.version (%d)", md->acc,
                md->gbsVersion, md->gbCdnaInfoVersion);
    if ((md->gbsModDate != md->gbCdnaInfoModdate))
        gbError("%s: gbStatus.modDate (%s) not same gbCdnaInfo.moddate (%s)", md->acc,
                gbFormatDate(md->gbsModDate), gbFormatDate(md->gbCdnaInfoModdate));
    /* verify either have or don't have a description */
    if (descOrgCats & md->gbsOrgCat)
        {
        if (!md->haveDesc)
            gbError("%s: should have gbCdnaInfo.description: %s", md->acc,
                    gbFmtSelect(md->gbsType|md->gbsOrgCat|md->gbsSrcDb));
        }
    else
        {
        if (md->haveDesc)
            gbError("%s: should not have gbCdnaInfo.description: %s", md->acc,
                    gbFmtSelect(md->gbsType|md->gbsOrgCat|md->gbsSrcDb));
        }
    }
}

static void loadGbStatus(struct metaDataTbls* metaDataTbls,
                         struct gbSelect* select, 
                         unsigned descOrgCats,
                         struct sqlConnection* conn)
/* load the gbStatus table */
{
char accWhere[64];
char query[512];
struct sqlResult* result;
char** row;

gbVerbMsg(2, "load gbStatus table data");
accWhere[0] = '\0';
if (select->accPrefix != NULL)
    safef(accWhere, sizeof(accWhere), " AND (acc LIKE '%s%%')",
          select->accPrefix);
safef(query, sizeof(query), 
      "SELECT acc,version,modDate,type,srcDb,orgCat,gbSeq,numAligns "
      "FROM gbStatus WHERE (type='%s') AND (srcDb='%s')%s",
      ((select->type == GB_MRNA) ? "mRNA" : "EST"),
      ((select->release->srcDb == GB_GENBANK) ? "GenBank" : "RefSeq"),
      accWhere);

result = sqlGetResult(conn, query);
while ((row = sqlNextRow(result)) != NULL)
    loadGbStatusRow(metaDataTbls, conn, row, descOrgCats);
sqlFreeResult(&result);
}

static void chkGbStatusGbEntry(struct gbSelect* select, struct gbEntry* entry,
                               struct metaData* md)
/* check entry fields against status fields */
{
/* processed entry should be the one matching the aligned update */
struct gbAligned* aligned = gbEntryFindAlignedVer(entry,
                                                  md->gbsVersion);
if (aligned == NULL)
    gbError("%s.%d: no genbank gbIndex aligned object for gbStatus",
            md->acc, md->gbsVersion);
else
    {
    /* search for a processed entry matching this data and version */
    struct gbProcessed* processed = entry->processed;
    while ((processed != NULL) &&
           !((processed->modDate == md->gbsModDate)
             && (processed->version == md->gbsVersion)))
        processed = processed->next;
    if (processed == NULL)
        gbError("%s: no gbIndex processed entry for version %d, moddate %s, update %s",
                md->acc, md->gbsVersion, gbFormatDate(md->gbsModDate),
                aligned->update->name);
    if (aligned->numAligns != md->gbsNumAligns)
        gbError("%s.%d: genbank index number of alignments (%d) does not match gbStatus (%d)",
                md->acc, md->gbsVersion, aligned->numAligns, md->gbsNumAligns);
    }
}

void chkMetaDataGbEntry(struct metaDataTbls* metaDataTbls,
                        struct gbSelect* select, struct gbEntry* entry)
/* Check metadata against a gbEntry object */
{
struct metaData* md = metaDataTblsGet(metaDataTbls, entry->acc);
md->inGbIndex = TRUE;
md->inGbAlign = (entry->aligned != NULL);
md->isNative = (entry->orgCat == GB_NATIVE);  /* FIXME: dup field */
md->typeFlags |= ((entry->orgCat == GB_NATIVE) ? GB_NATIVE : GB_XENO);
md->excluded = ((entry->orgCat & select->orgCats) == 0);

if (md->inGbStatus)
    {
    if (md->excluded)
        gbError("%s: excluded (%s) entry should not be in gbStatus table",
                gbOrgCatName(entry->orgCat), entry->acc);
    else
        chkGbStatusGbEntry(select, entry, md);
    }
}

static char* getTablesDesc(struct metaData* md)
/* get string indicating which tables an acc was found in */
{
static char tbls[256];
tbls[0] = '\0';
if (md->inGbCdnaInfo)
    strcat(tbls, ",mrna");
if (md->inSeq)
    strcat(tbls, ",seq");
if (md->inRefSeqStatus)
    strcat(tbls, ",refSeqStatus");
if (md->inRefLink)
    strcat(tbls, ",refLink");
if (md->inGbStatus)
    strcat(tbls, ",gbStatus");
if (md->inGbIndex)
    strcat(tbls, ",gbIndex");
return tbls+1;  /* skip first comma */
}

static void checkXRefRefSeq(struct metaData* md)
/* check sanity of collected additional metadata for RefSeq */
{
if (!md->inRefSeqStatus)
    gbError("%s: not in refSeqStatus table, and is RefSeq acc", md->acc);
if (!md->inRefLink)
    gbError("%s: not in refLink table, and is RefSeq acc ", md->acc);
if (gbIsProteinCodingRefSeq(md->acc))
    {
    if (!md->hasProt)
        gbError("%s: no peptide for RefSeq", md->acc);
    else 
        {
        if (!md->protInSeq)
            gbError("%s: RefSeq peptide %s not in gbSeq table", md->acc, md->rlProtAcc);
        else if (!md->protInExtFile)
            gbError("%s: RefSeq peptide %s not in gbExtFile table", md->acc, md->rlProtAcc);
        }
    }
}

static void checkXRef(struct metaData* md)
/* check sanity of collected metadata */
{
if (!md->inGbAlign)
    return;  /* can't check anything else */
if (!md->inGbCdnaInfo)
    gbError("%s: not in gbCdnaInfo table, referenced in %s", md->acc,
            getTablesDesc(md));
if (!md->inSeq)
    gbError("%s: not in seq table, referenced in %s", md->acc,
            getTablesDesc(md));
if (!md->inExtFile)
    gbError("%s: not in gbExtFile table, referenced in %s", md->acc,
            getTablesDesc(md));

if (md->typeFlags & GB_REFSEQ)
    {
    checkXRefRefSeq(md);
    }
else
    {
    if (md->inRefSeqStatus)
        gbError("%s: in refSeqStatus table, and not RefSeq acc", md->acc);
    if (md->inRefLink)
        gbError("%s: in refLink table, and not RefSeq acc", md->acc);
    }
if (!md->inGbStatus)
    gbError("%s: not in gbStatus table, referenced in %s", md->acc,
            getTablesDesc(md));
if (!md->inGbIndex)
    gbError("%s: not in gbIndex, referenced in %s", md->acc,
            getTablesDesc(md));
}

struct metaDataTbls* chkMetaDataTbls(struct gbSelect* select,
                                     struct sqlConnection* conn,
                                     boolean checkExtSeqRecs,
                                     unsigned descOrgCats,
                                     char* gbdbMapToCurrent)
/* load the metadata tables do basic validatation.  descOrgCats are
 * orgCats that should have descriptions. */
{
struct metaDataTbls* metaDataTbls;

gbVerbEnter(1, "load and check metadata tables: %s", gbSelectDesc(select));

metaDataTbls = metaDataTblsNew();

/* order is important here to allow checking between tables */
loadGbCdnaInfoData(metaDataTbls, select, conn);
if (select->release->srcDb == GB_REFSEQ)
    {
    /* must load before seq data due to protein checks */
    loadRefSeqStatus(metaDataTbls, conn);
    loadRefLink(metaDataTbls, conn);
    }
loadSeqData(metaDataTbls, select, conn, checkExtSeqRecs, gbdbMapToCurrent);
loadGbStatus(metaDataTbls, select, descOrgCats, conn);

gbVerbLeave(1, "load and check metadata tables: %s", gbSelectDesc(select));
return metaDataTbls;
}

void chkMetaDataXRef(struct metaDataTbls* metaDataTbls)
/* Verify that data that is referenced in some tables is in all expected
 * tables.  Called after processing all indices */
{
struct metaData* md;

gbVerbEnter(1, "cross check metadata");

metaDataTblsFirst(metaDataTbls);
while ((md = metaDataTblsNext(metaDataTbls)) != NULL)
    checkXRef(md);
gbVerbLeave(1, "cross check metadata");
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */


