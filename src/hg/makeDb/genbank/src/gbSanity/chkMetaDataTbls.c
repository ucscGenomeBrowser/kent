/* Object to load the metadata from database and verify */
#include "common.h"
#include "gbDefs.h"
#include "chkMetaDataTbls.h"
#include "metaData.h"
#include "gbFileOps.h"
#include "gbEntry.h"
#include "gbUpdate.h"
#include "gbAligned.h"
#include "gbProcessed.h"
#include "gbRelease.h"
#include "gbGenome.h"
#include "gbVerb.h"
#include "extFileTbl.h"
#include "localmem.h"
#include "jksql.h"
#include "hdb.h"
#include "fa.h"

static char const rcsid[] = "$Id: chkMetaDataTbls.c,v 1.3 2003/06/28 04:02:21 markd Exp $";


static char* validRefSeqStatus[] = {
    "Unknown", "Reviewed", "Validated", "Provisional", "Predicted", NULL
};

/* hash tables used to avoid repeating error messages */
static struct hash* missingExtFileIds;  /* missing ext file ids that
                                         * are not in the table */
static struct hash* missingExtFiles;  /* Missing or wrong size files in
                                       * extFile table. */

boolean alreadyReported(struct hash* reportedHash, char* key)
/* check is a key associated with an error is in the specified reported
 * table */
{
return (reportedHash != NULL) && (hashLookup(reportedHash, key) != NULL);
}

void flagReported(struct hash* reportedHash, char* key)
/* flag an error as having been reported */
{
hashAdd(reportedHash, key, NULL);
}

static unsigned strToUnsigned(char* str, char* acc, char* useMsg,
                              boolean* gotError)
/* Parse a string into an unsigned. */
{
char* stop;
unsigned num = 0;
num = strtoul(str, &stop, 10);
if ((*stop != '\0') || (stop == str))
    {
    gbError("%s: invalid unsigned \"%s\": %s ", acc, str, useMsg);
    if (gotError != NULL)
        *gotError = TRUE;
    }
else
    if (gotError != NULL)
        *gotError = FALSE;
return num;
}

static off_t strToOffset(char* str, char* acc, char* useMsg)
/* Parse a string into an offset_t. */
{
char* stop;
off_t num = 0;
num = strtoull(str, &stop, 10);
if ((*stop != '\0') || (stop == str))
    gbError("%s: invalid offset \"%s\": %s ", acc, str, useMsg);
return num;
}

static void loadMrnaRow(struct metaDataTbls* metaDataTbls,
                        struct sqlConnection* conn, char** row)
/* load one row from the mrna table */
{
struct metaData* md;
int len, numNonZero, iRow = 0;
char *acc, *dir;
boolean gotError, isOk;

/* columns: acc,id,moddate,version,moddate,type */
acc = row[iRow++];
md = metaDataTblsGet(metaDataTbls, acc);
if (md->inMrna)
    {
    gbError("%s: acc occurs multiple times in the mrna table", acc);
    return;
    }
md->inMrna = TRUE;
md->mrnaId = strToUnsigned(row[iRow++], acc, "mrna.id", NULL);
len = strlen(acc);
md->mrnaVersion = strToUnsigned(row[iRow++], "mrna.version", acc, &gotError);
if (!gotError && (md->mrnaVersion <= 0))
     gbError("%s: mrna.version invalid: \"%d\"", acc, md->mrnaVersion);
isOk = TRUE;
md->mrnaModdate = gbParseChkDate(row[iRow++], &isOk);
if (!isOk)
    gbError("%s: invalid mrna.moddate value: \"%s\"", acc, row[iRow-1]);
md->mrnaType = gbParseType(row[iRow++]);
md->typeFlags |= md->mrnaType;

dir = row[iRow++];
if ((strlen(dir) > 1) || (strchr("053", *dir) == NULL))
    gbError("%s: invalid mrna.direction value: \"%s\"", acc, dir);

/* Make sure that at least a few of the id fields have data  */
numNonZero = 0;
while (iRow < 20)
    {
    int id = strToUnsigned(row[iRow++], md->acc, "mrna.?", NULL);
    if (id > 0)
        numNonZero++;
    /* remember if we have a description */
    if (iRow-1 == 16)
        md->haveDesc = (id != 0);
    }
if (numNonZero == 0)
    gbError("%s: none of mrna string ids have non-zero values", dir);
else if (numNonZero < 4)
    gbError("%s: only %d of mrna string ids have non-zero values",
            dir, numNonZero);
}

static void loadMrnaData(struct metaDataTbls* metaDataTbls,
                         struct gbSelect* select, struct sqlConnection* conn)
/* load the mrna table */
{
char accWhere[64];
char query[512];
struct sqlResult* result;
char** row;

gbVerbMsg(2, "load mrna table data");
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
      "FROM mrna WHERE (type='%s')%s",
      ((select->type == GB_MRNA) ? "mRNA" : "EST"), accWhere);
/* mrna doesn't have a srcDb, so we guess from acc */
result = sqlGetResult(conn, query);
while ((row = sqlNextRow(result)) != NULL)
    {
    if (gbGuessSrcDb(row[0]) == select->release->srcDb)
        loadMrnaRow(metaDataTbls, conn, row);
    }
sqlFreeResult(&result);
}

static struct extFile* getExtFile(struct metaData* md,
                                  struct extFileTbl* extFileTbl,
                                  HGID seqExtFile)
/* get the extFile by id, reporting if not in table */
{
struct extFile* extFile = extFileTblFindById(extFileTbl, seqExtFile);

/* check if in ext file table */
if (extFile == NULL)
    {
    /* only reported the first time */
    char seqExtStr[32];
    safef(seqExtStr, sizeof(seqExtStr), "%d", seqExtFile);
    if (!alreadyReported(missingExtFileIds, seqExtStr))
        {
        gbError("%s: gbSeq.gbExtFile (%d) not in gbExtFile table", md->acc,
                seqExtFile);
        flagReported(missingExtFileIds, seqExtStr);
        }
    }
return extFile;
}

static void checkExtRecord(struct metaData* md, char* extPath,
                           off_t seqFile_offset, off_t seqFile_size)
/* Check the external file record for a sequence (slow). Assumes
 * that bounds have been sanity check for a file. */
{
/* read range into buffer */
FILE *fh = mustOpen(extPath, "r");
char *faBuf;
char accVer[GB_ACC_BUFSZ];
struct dnaSeq *dnaSeq;
if (fseeko(fh, seqFile_offset, SEEK_SET) < 0)
    {
    gbError("%s: can't seek %s", md->acc, extPath);
    carefulClose(&fh);
    }
faBuf = needMem(seqFile_size+1);
mustRead(fh, faBuf, seqFile_size);
faBuf[seqFile_size] = '\0';
carefulClose(&fh);

/* verify contents */
if (faBuf[0] != '>')
    {
    gbError("%s: gbExtFile offset doesn't start a fasta record: %s",
            md->acc, extPath);
    free(faBuf);
    return;
    }
dnaSeq = faFromMemText(faBuf);
safef(accVer, sizeof(accVer), "%s.%d", md->acc, md->mrnaVersion);

if (!sameString(dnaSeq->name, accVer))
    gbError("%s: name in fasta header \"%s\" doesn't matche expected \"%s\": %s",
            md->acc, dnaSeq->name, accVer, extPath);
if (dnaSeq->size != md->seqSize)
    gbError("%s: size of fasta sequence (%d) doesn't matche expected (%d): %s",
            md->acc, dnaSeq->size, md->seqSize, extPath);
freeDnaSeq(&dnaSeq);
}

static void verifySeqExtFile(struct metaData* md,
                             struct extFileTbl* extFileTbl,
                             boolean checkExtSeqRecs,
                             char* gbdbMapToCurrent,
                             HGID seqExtFile, off_t seqFile_offset,
                             off_t seqFile_size)
/* verify a seq table entry with it's extFile entry.   */
{
int mapLen = (gbdbMapToCurrent != NULL) ? strlen(gbdbMapToCurrent) : 0;
char* extPath;
boolean badSeq = FALSE;
struct extFile* extFile = getExtFile(md, extFileTbl, seqExtFile);
if (extFile == NULL)
    return; /* all that can be checked */
md->inExtFile = TRUE;

/* now, sanity check seq with extFile */
if ((seqFile_offset+seqFile_size) > extFile->size)
    {
    gbError("%s: gbSeq.file_offset+gbSeq.file_size > gbExtFile.size", md->acc);
    badSeq = TRUE;
    }

/* map path to local directory if requested */
extPath = extFile->path;
if ((gbdbMapToCurrent != NULL) && startsWith(gbdbMapToCurrent, extPath)
    && (extPath[mapLen] == '/'))
    extPath += mapLen+1;

/* check readability and size of file. if found unreadable before, don't repeat
 * message or rest of tests. */
if (alreadyReported(missingExtFiles, extPath))
    return;  /* already reported, nothing more to do */

if (access(extPath, R_OK) < 0)
    {
    gbError("%s: extFile does not exist or is not readable: %s",
            md->acc, extPath);
    flagReported(missingExtFiles, extPath);
    badSeq = TRUE;
    }
else if (fileSize(extPath) != extFile->size)
    {
    gbError("%s: disk file size (%lld) does not match ext.size (%lld): %s",
            md->acc, fileSize(extPath), extFile->size, extPath);
    flagReported(missingExtFiles, extPath);
    badSeq = TRUE;
    }

if (!badSeq && checkExtSeqRecs)
    checkExtRecord(md, extPath, seqFile_offset, seqFile_size);
}

static void loadSeqRow(struct metaDataTbls* metaDataTbls,
                       struct extFileTbl* extFileTbl,
                       boolean checkExtSeqRecs,
                       char* gbdbMapToCurrent,
                       struct sqlConnection* conn, char** row)
/* load one row from the seq table */
{
char *acc;
HGID id;
unsigned type, srcDb;
HGID seqExtFile;
off_t seqFile_offset, seqFile_size;
int iRow = 0;
boolean isOk;
struct metaData* md;

/* columns: acc,id,size,gbExtFile,file_offset,file_size,type,srcDb */
acc = row[iRow++];
md = metaDataTblsGet(metaDataTbls, acc);
if (md->inSeq)
    gbError("%s: acc occurs multiple times in the seq table", acc);
md->inSeq = TRUE;
id = strToUnsigned(row[iRow++], md->acc, "gbSeq.id", NULL);
md->seqSize = strToUnsigned(row[iRow++], md->acc, "gbSeq.size", NULL);

isOk = TRUE;

seqExtFile = strToUnsigned(row[iRow++], md->acc, "gbSeq.gbExtFile", NULL);
seqFile_offset = strToOffset(row[iRow++], md->acc, "gbSeq.file_offset");
seqFile_size = strToOffset(row[iRow++], md->acc, "gbSeq.file_size");
type = gbParseType(row[iRow++]);
srcDb = gbParseSrcDb(row[iRow++]);

if (md->inMrna)
    {
    if (id != md->mrnaId)
        gbError("%s: gbSeq.id (%d) not same mrna.id (%d)", acc, id, md->mrnaId);
    if (type != md->mrnaType)
        gbError("%s: gbSeq.type (%s) not same as mrna.type (%s)", acc,
                gbFmtSelect(type), gbFmtSelect(md->mrnaType));
    if ((srcDb & md->typeFlags) == 0)
        gbError("%s: gbSeq.srcDb (%s) not same mrna.srcDb (%s)", acc,
                gbFmtSelect(srcDb), gbFmtSelect(md->typeFlags));
    if (md->seqSize >= seqFile_size)
        gbError("%s: gbSeq.size >= gbSeq.file_size", md->acc);
    }

verifySeqExtFile(md, extFileTbl, checkExtSeqRecs, gbdbMapToCurrent, seqExtFile,
                 seqFile_offset, seqFile_size);
}

static void loadSeqData(struct metaDataTbls* metaDataTbls,
                        struct gbSelect* select, struct sqlConnection* conn,
                        boolean checkExtSeqRecs, char* gbdbMapToCurrent)
/* load seq table data, mrna table should be loaded. */
{
char accWhere[64];
char query[512];
char **row;
struct sqlResult* result;
struct extFileTbl* extFileTbl;

gbVerbMsg(2,  "load gbExtFile table data");
extFileTbl = extFileTblLoad(conn);

gbVerbMsg(2, "load gbSeq table data");
accWhere[0] = '\0';
if (select->accPrefix != NULL)
    safef(accWhere, sizeof(accWhere), " AND (acc LIKE '%s%%')",
          select->accPrefix);
safef(query, sizeof(query), 
      "SELECT acc,id,size,gbExtFile,file_offset,file_size,type,srcDb "
      "FROM gbSeq WHERE (type='%s') AND (srcDb='%s')%s",
      ((select->type == GB_MRNA) ? "mRNA" : "EST"),
      ((select->release->srcDb == GB_GENBANK) ? "GenBank" : "RefSeq"),
      accWhere);

result = sqlGetResult(conn, query);

while ((row = sqlNextRow(result)) != NULL)
    loadSeqRow(metaDataTbls, extFileTbl, checkExtSeqRecs,gbdbMapToCurrent,
               conn, row);
sqlFreeResult(&result);
extFileTblFree(&extFileTbl);
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
struct metaData* md;
int iRow = 0;
char *product;

/* columns: mrnaAcc,name,product,protAcc,geneName,prodName,locusLinkId,omimId */

md = metaDataTblsGet(metaDataTbls, row[iRow++]);
if (md->inRefLink)
    gbError("%s: occurs multiple times in the refLink table", md->acc);
md->inRefLink = TRUE;
safef(md->rlName, sizeof(md->rlName), "%s", row[iRow++]);
product = row[iRow++];
safef(md->rlProtAcc, sizeof(md->rlProtAcc), row[iRow++]);

/* check if ids are valid (zero is allowed, so just parse) */
strToUnsigned(row[iRow++], md->acc, "refLink.geneName", NULL);
strToUnsigned(row[iRow++], md->acc, "refLink.prodName", NULL);
strToUnsigned(row[iRow++], md->acc, "refLink.locusLinkId", NULL);
strToUnsigned(row[iRow++], md->acc, "refLink.omimId", NULL);
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

if (md->inMrna)
    {
    if (seqId != md->mrnaId)
        gbError("%s: gbStatus.gbSeq (%d) not same mrna.id (%d)", md->acc, seqId,
                md->mrnaId);
    if (md->gbsType != md->mrnaType)
        gbError("%s: gbStatus.type (%s) not same as mrna.type (%s)", md->acc,
                gbFmtSelect(md->gbsType), gbFmtSelect(md->mrnaType));
    if (md->gbsSrcDb != (md->typeFlags & GB_SRC_DB_MASK))
        gbError("%s: gbStatus.srcDb (%s) not same mrna.srcDb (%s)", md->acc,
                gbFmtSelect(md->gbsSrcDb), gbFmtSelect(md->typeFlags));
    if ((md->gbsModDate != md->mrnaModdate))
        gbError("%s: gbStatus.modDate (%s) not same mrna.moddate (%s)", md->acc,
                gbFormatDate(md->gbsModDate), gbFormatDate(md->mrnaModdate));
    /* verify either have or don't have a description */
    if (descOrgCats & md->gbsOrgCat)
        {
        if (!md->haveDesc)
            gbError("%s: should have mrna.description: %s", md->acc,
                    gbFmtSelect(md->gbsType|md->gbsOrgCat|md->gbsSrcDb));
        }
    else
        {
        if (md->haveDesc)
            gbError("%s: should not have mrna.description: %s", md->acc,
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
struct metaData* md;
/* RefSeq non-native are optional */
boolean excluded = ((entry->orgCat & select->orgCats) == 0);

md = metaDataTblsFind(metaDataTbls, entry->acc);  /* null if not found */

/* should only be in metadata if an aligned record exists */
if (entry->aligned != NULL)
    {
    if (md == NULL)
        {
        if (!excluded)
            gbError("%s: aligned gbIndex entry is not in the mrna, seq, or gbStatus tables",
                    entry->acc);
        }
    else if (!md->inGbStatus && !excluded)
        {
        gbError("%s: aligned gbIndex entry is not in the gbStatus table",
                entry->acc);
        }
    }
else
    {
    if (metaDataTblsFind(metaDataTbls, entry->acc) != NULL)
        gbError("%s: has no aligned gbIndex entries, however it in one "
                "or more metadata tables", entry->acc);
    }

/* create a metaData object if we don't have one */
if (md == NULL)
    md = metaDataTblsGet(metaDataTbls, entry->acc);

md->inGbIndex = TRUE;
md->isNative = (entry->orgCat == GB_NATIVE);  /* FIXME: dup field */
md->typeFlags |= ((entry->orgCat == GB_NATIVE) ? GB_NATIVE : GB_XENO);
md->excluded = excluded;

if (md->inGbStatus)
    {
    if (md->excluded)
        gbError("%s: excluded (xeno RefSeq) entry should not be in gbStatus table",
                entry->acc);
    else
        chkGbStatusGbEntry(select, entry, md);
    }
}

static char* getTablesDesc(struct metaData* md)
/* get string indicating which tables an acc was found in */
{
static char tbls[256];
tbls[0] = '\0';
if (md->inMrna)
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

static void metaDataCheckXRef(struct metaData* md)
/* check sanity of collected metadata */
{
if (!md->inMrna)
    gbError("%s: not in mrna table, referenced in %s", md->acc,
            getTablesDesc(md));
if (!md->inSeq)
    gbError("%s: not in seq table, referenced in %s", md->acc,
            getTablesDesc(md));
if (!md->inExtFile)
    gbError("%s: not in gbExtFile table, referenced in %s", md->acc,
            getTablesDesc(md));

if (md->typeFlags & GB_REFSEQ)
    {
    if (!md->inRefSeqStatus)
        gbError("%s: not in refSeqStatus table, and is RefSeq acc", md->acc);
    if (!md->inRefLink)
        gbError("%s: not in refLink table, and is RefSeq acc ", md->acc);
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

/* setup globals */
missingExtFileIds = hashNew(16);
missingExtFiles = hashNew(16);


gbVerbEnter(1, "load and check metadata tables: %s", gbSelectDesc(select));

metaDataTbls = metaDataTblsNew();

/* order is important here to allow checking between tables */
loadMrnaData(metaDataTbls, select, conn);
loadSeqData(metaDataTbls, select, conn, checkExtSeqRecs, gbdbMapToCurrent);
if (select->release->srcDb == GB_REFSEQ)
    {
    loadRefSeqStatus(metaDataTbls, conn);
    loadRefLink(metaDataTbls, conn);
    }
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
    metaDataCheckXRef(md);
gbVerbLeave(1, "cross check metadata");
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */


