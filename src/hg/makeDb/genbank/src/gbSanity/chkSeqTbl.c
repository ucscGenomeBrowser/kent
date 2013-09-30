/* chkSeqTbl - validation of seq with extFile and fastas */
#include "common.h"
#include "chkSeqTbl.h"
#include "chkCommon.h"
#include "gbDefs.h"
#include "gbRelease.h"
#include "gbVerb.h"
#include "extFileTbl.h"
#include "metaData.h"
#include "fa.h"


/* hash tables used to avoid repeating error messages */
static struct hash* missingExtFileIds = NULL;  /* missing ext file ids that
                                                * are not in the table */
static struct hash* missingExtFiles = NULL;  /* Missing or wrong size files in
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

struct seqFields
/* object to hold seqFields. really want autoSql, but it doesn't handle the
 * enums, and all that is need for now hold parsed rows.  No dynamic data
 * is involved, fields point into row. */
{
    unsigned id;
    char *acc;
    short version;
    unsigned size;
    unsigned gbExtFile;
    off_t file_offset;
    unsigned file_size;
    unsigned type;
    unsigned srcDb;
};

static void parseGbSeqRow(char **row, struct seqFields *seq)
/* parse a row from gbSeq.  No dynamic memory is allocated */
{
char *acc = row[1];
int iRow = 0;
seq->id = strToUnsigned(row[iRow++], acc, "gbSeq.id", NULL);
seq->acc = row[iRow++];
seq->version = strToUnsigned(row[iRow++], acc, "gbSeq.version", NULL);
seq->size = strToUnsigned(row[iRow++], acc, "gbSeq.size", NULL);
seq->gbExtFile = strToUnsigned(row[iRow++], acc, "gbSeq.gbExtFile", NULL);
seq->file_offset = strToOffset(row[iRow++], acc, "gbSeq.file_offset");
seq->file_size = strToOffset(row[iRow++], acc, "gbSeq.file_size");
if (sameWord(row[iRow], "PEP"))
    iRow++;  /* type for peptides not supported by gbParseType */
else
    seq->type = gbParseType(row[iRow++]);
seq->srcDb = gbParseSrcDb(row[iRow++]);
}

static struct extFile* getExtFile(char *acc, struct extFileTbl* extFileTbl,
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
        gbError("%s: gbSeq.gbExtFile (%d) not in gbExtFile table", acc,
                seqExtFile);
        flagReported(missingExtFileIds, seqExtStr);
        }
    }
return extFile;
}

static void checkExtRecord(struct seqFields *seq,
                           char *extPath)
/* Check the external file record for a sequence (slow). Assumes
 * that bounds have been sanity check for a file. */
{
/* read range into buffer */
FILE *fh = mustOpen(extPath, "r");
char *faBuf;
char accVer[GB_ACC_BUFSZ];
struct dnaSeq *dnaSeq;
if (fseeko(fh, seq->file_offset, SEEK_SET) < 0)
    {
    gbError("%s: can't seek %s", seq->acc, extPath);
    carefulClose(&fh);
    }
faBuf = needMem(seq->file_size+1);
mustRead(fh, faBuf, seq->file_size);
faBuf[seq->file_size] = '\0';
carefulClose(&fh);

/* verify contents */
if (faBuf[0] != '>')
    {
    gbError("%s: gbExtFile offset %lld doesn't start a fasta record: %s",
            seq->acc, (long long)seq->file_offset, extPath);
    free(faBuf);
    return;
    }
dnaSeq = faFromMemText(faBuf);
safef(accVer, sizeof(accVer), "%s.%d", seq->acc, seq->version);

if (!sameString(dnaSeq->name, accVer))
    gbError("%s: name in fasta header \"%s\" doesn't match expected \"%s\": %s",
            seq->acc, dnaSeq->name, accVer, extPath);
if (dnaSeq->size != seq->size)
    gbError("%s: size of fasta sequence (%d) doesn't match expected (%d): %s",
            seq->acc, dnaSeq->size, seq->size, extPath);
freeDnaSeq(&dnaSeq);
}

static boolean verifySeqExtFile(struct seqFields *seq,
                                struct extFileTbl* extFileTbl,
                                boolean checkExtSeqRecs,
                                char* gbdbMapToCurrent)
/* verify a seq table information with it's extFile entry. return true
 * if found in extFile table. */
{
#if 0 /* FIXME: for disabled code below */
int mapLen = (gbdbMapToCurrent != NULL) ? strlen(gbdbMapToCurrent) : 0;
#endif
char* extPath;
boolean badSeq = FALSE;
struct extFile* extFile = getExtFile(seq->acc, extFileTbl, seq->gbExtFile);
if (extFile == NULL)
    return FALSE; /* all that can be checked */

/* now, sanity check seq with extFile */
if ((seq->file_offset+seq->file_size) > extFile->size)
    {
    gbError("%s: gbSeq.file_offset+gbSeq.file_size > gbExtFile.size", seq->acc);
    badSeq = TRUE;
    }

/* map path to local directory if requested */
extPath = extFile->path;
#if 0 /* FIXME: load stores full path when redirected */
if ((gbdbMapToCurrent != NULL) && startsWith(gbdbMapToCurrent, extPath)
    && (extPath[mapLen] == '/'))
    extPath += mapLen+1;
#endif

/* check readability and size of file. if found unreadable before, don't repeat
 * message or rest of tests. */
if (alreadyReported(missingExtFiles, extPath))
    return TRUE;  /* already reported, nothing more to do */

if (access(extPath, R_OK) < 0)
    {
    gbError("%s: extFile does not exist or is not readable: %s",
            seq->acc, extPath);
    flagReported(missingExtFiles, extPath);
    badSeq = TRUE;
    }
else if (fileSize(extPath) != extFile->size)
    {
    gbError("%s: disk file size (%lld) does not match ext.size (%lld): %s",
            seq->acc, (long long)fileSize(extPath), (long long)extFile->size, extPath);
    flagReported(missingExtFiles, extPath);
    badSeq = TRUE;
    }

if (!badSeq && checkExtSeqRecs)
    checkExtRecord(seq, extPath);
return TRUE;
}

static void loadSeqCDnaRow(struct metaDataTbls* metaDataTbls,
                           struct extFileTbl* extFileTbl,
                           boolean checkExtSeqRecs,
                           char* gbdbMapToCurrent,
                           struct sqlConnection* conn, char **row)
/* load one row for a cDNA from the seq table */
{
struct seqFields seq;
struct metaData* md;
parseGbSeqRow(row, &seq);
md = metaDataTblsGet(metaDataTbls, seq.acc);
if (md->inSeq)
    gbError("%s: acc occurs multiple times in the seq table", seq.acc);
md->inSeq = TRUE;
md->seqSize = seq.size;

if (md->inGbCdnaInfo)
    {
    if (seq.id != md->gbCdnaInfoId)
        gbError("%s: gbSeq.id (%d) not same gbCdnaInfo.id (%d)", seq.acc, seq.id, md->gbCdnaInfoId);
    if (seq.type != md->gbCdnaInfoType)
        gbError("%s: gbSeq.type (%s) not same as gbCdnaInfo.type (%s)", seq.acc,
                gbFmtSelect(seq.type), gbFmtSelect(md->gbCdnaInfoType));
    if ((seq.srcDb & md->typeFlags) == 0)
        gbError("%s: gbSeq.srcDb (%s) not same gbCdnaInfo.srcDb (%s)", seq.acc,
                gbFmtSelect(seq.srcDb), gbFmtSelect(md->typeFlags));
    if (md->seqSize >= seq.file_size)
        gbError("%s: gbSeq.size >= gbSeq.file_size", seq.acc);
    }

if (verifySeqExtFile(&seq, extFileTbl, checkExtSeqRecs, gbdbMapToCurrent))
    md->inExtFile = TRUE;
}

static void loadSeqCDnaData(struct metaDataTbls* metaDataTbls,
                            struct gbSelect* select, struct sqlConnection* conn,
                            boolean checkExtSeqRecs, char* gbdbMapToCurrent,
                            struct extFileTbl* extFileTbl)
/* load cDNA data from the seq table */
{
char accWhere[64];
char query[512];
char **row;
struct sqlResult* result;
// FIXME: this could be re-written to use sqlDyString functions instead.
gbVerbMsg(2, "load gbSeq cDNA table data");
accWhere[0] = '\0';
if (select->accPrefix != NULL)
    sqlSafefFrag(accWhere, sizeof(accWhere), " AND (acc LIKE '%s%%')",
          select->accPrefix);
sqlSafef(query, sizeof(query), 
      "SELECT * FROM gbSeq WHERE (type='%s') AND (srcDb='%s')%-s",
      ((select->type == GB_MRNA) ? "mRNA" : "EST"),
      ((select->release->srcDb == GB_GENBANK) ? "GenBank" : "RefSeq"),
      accWhere);

result = sqlGetResult(conn, query);

while ((row = sqlNextRow(result)) != NULL)
    loadSeqCDnaRow(metaDataTbls, extFileTbl, checkExtSeqRecs,gbdbMapToCurrent,
                   conn, row);
sqlFreeResult(&result);
}

static void loadSeqPepRow(struct metaDataTbls* metaDataTbls,
                          struct extFileTbl* extFileTbl,
                          boolean checkExtSeqRecs,
                          char* gbdbMapToCurrent,
                          struct sqlConnection* conn, char **row)
/* load one row for a  from the seq table */
{
struct seqFields seq;
struct metaData* md;
parseGbSeqRow(row, &seq);
md = metaDataTblsGetByPep(metaDataTbls, seq.acc);
if (md == NULL)
    {
#if 0 //FIXME: disabled due to known and harmless bug
    gbError("%s: peptide in gbSeq not found in refLink", seq.acc);
#endif
    }
else
    {
    if (md->protInSeq)
        gbError("%s: acc occurs multiple times in the seq table", seq.acc);
    md->protInSeq = TRUE;
    if (verifySeqExtFile(&seq, extFileTbl, checkExtSeqRecs, gbdbMapToCurrent))
        md->protInExtFile = TRUE;
    }
}

static void loadSeqPepData(struct metaDataTbls* metaDataTbls,
                           struct sqlConnection* conn,
                           boolean checkExtSeqRecs, char* gbdbMapToCurrent,
                           struct extFileTbl* extFileTbl)
/* load refseq peptide data from the seq table */
{
char query[512];
char **row;
struct sqlResult* result;
gbVerbMsg(2, "load gbSeq peptide table data");
sqlSafef(query, sizeof(query), 
      "SELECT * FROM gbSeq WHERE (type='PEP') AND (srcDb='RefSeq')");

result = sqlGetResult(conn, query);

while ((row = sqlNextRow(result)) != NULL)
    loadSeqPepRow(metaDataTbls, extFileTbl, checkExtSeqRecs,gbdbMapToCurrent,
                  conn, row);
sqlFreeResult(&result);
}

void loadSeqData(struct metaDataTbls* metaDataTbls,
                 struct gbSelect* select, struct sqlConnection* conn,
                 boolean checkExtSeqRecs, char* gbdbMapToCurrent)
/* load seq table data, gbCdnaInfo table should be loaded. For
* refseq, refLink should also have been loaded*/
{
struct extFileTbl* extFileTbl;

gbVerbMsg(2,  "load gbExtFile table data");
extFileTbl = extFileTblLoad(conn);

/* setup globals */
if (missingExtFileIds == NULL) 
    {
    missingExtFileIds = hashNew(16);
    missingExtFiles = hashNew(16);
    }
loadSeqCDnaData(metaDataTbls,select, conn, checkExtSeqRecs, gbdbMapToCurrent,
                extFileTbl);
if (select->release->srcDb == GB_REFSEQ)
    loadSeqPepData(metaDataTbls, conn, checkExtSeqRecs, gbdbMapToCurrent,
                   extFileTbl);

extFileTblFree(&extFileTbl);
}
