/* bdwSubmit - Submit URL with validated.txt to warehouse.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "localmem.h"
#include "net.h"
#include "options.h"
#include "errabort.h"
#include "dystring.h"
#include "errCatch.h"
#include "sqlNum.h"
#include "cheapcgi.h"
#include "net.h"
#include "hex.h"
#include "bigDataWarehouse.h"
#include "bdwLib.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bdwSubmit - Submit URL with validated.txt to warehouse.\n"
  "usage:\n"
  "   bdwSubmit validatedUrl user password\n"
  "Generally user is an email address\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

struct fieldedRow
/* An array of strings with a little extra info, can be hung on a list. */
    {
    struct fieldedRow *next;
    char **row; // Array of strings
    int id;	// In the file case this is the line of file row starts in
    };

struct fieldedTable
/* A table with a name for each field. */
    {
    struct fieldedTable *next;
    char *name;	    /* Often the file name */
    struct lm *lm;  /* All allocations done out of this memory pool. */
    int fieldCount; /* Number of fields. */
    char **fields;  /* Names of fields. */
    struct fieldedRow *rowList;  /* list of parsed out fields. */
    struct fieldedRow **cursor;  /* Pointer to where we add next item to list. */
    };

struct fieldedTable *fieldedTableNew(char *name, char **fields, int fieldCount)
/* Create a new empty fieldedTable with given name. */
{
struct fieldedTable *table;
AllocVar(table);
struct lm *lm = table->lm = lmInit(0);
table->name = lmCloneString(lm, name);
table->cursor = &table->rowList;
table->fieldCount = fieldCount;
int i;
char **row = lmAllocArray(lm, table->fields, fieldCount);
for (i=0; i<fieldCount; ++i)
    {
    row[i] = lmCloneString(lm, fields[i]);
    }
return table;
}

void fieldedTableFree(struct fieldedTable **pTable)
/* Free up memory resources associated with table. */
{
struct fieldedTable *table = *pTable;
if (table != NULL)
    {
    lmCleanup(&table->lm);
    freez(pTable);
    }
}


struct fieldedRow *fieldedTableAdd(struct fieldedTable *table,  char **row, int rowSize, int id)
/* Create a new row and add it to table.  Return row. */
{
/* Make sure we got right number of fields. */
if (table->fieldCount != rowSize)
    errAbort("%s starts with %d fields, but at line %d has %d fields instead",
	    table->name, table->fieldCount, id, rowSize);

/* Allocate field from local memory and start filling it in. */
struct lm *lm = table->lm;
struct fieldedRow *fr;
lmAllocVar(lm, fr);
lmAllocArray(lm, fr->row, rowSize);
fr->id = id;
int i;
for (i=0; i<rowSize; ++i)
    fr->row[i] = lmCloneString(lm, row[i]);

/* Add it to end of list using cursor to avoid slReverse hassles. */
*(table->cursor) = fr;
table->cursor = &fr->next;

return fr;
}

int bdwOpenAndRecord(struct sqlConnection *conn, 
	char *submissionDir, char *submissionFile, char *url)
/* Return a low level read socket handle on URL if possible.  Consult and
 * possibly update the bdwHost and bdwDir tables so don't keep beating up
 * and timing out on same host or hub. The url should be made by combining
 * submissionDir and submissionFile. Note submissionFile itself may have
 * further directory info. */
{
/* Wrap netUrlOpen in errCatch and remember whether it works. */
struct errCatch *errCatch = errCatchNew();
int sd = -1;
if (errCatchStart(errCatch))
    {
    sd = netUrlOpen(url);
    }
errCatchEnd(errCatch);
boolean success = TRUE;
if (errCatch->gotError)
    {
    success = FALSE;
    warn("Error: %s", errCatch->message->string);
    }

/* Parse url into pieces */
struct netParsedUrl npu;
ZeroVar(&npu);
netParseUrl(url, &npu);
char urlDir[PATH_LEN], urlFileName[PATH_LEN], urlExtension[PATH_LEN];
splitPath(npu.file, urlDir, urlFileName, urlExtension);

/* Get or make pieces of database for submission. */
#ifdef SOON
int hostId = bdwGetHost(conn, npu.host);
recordIntoHistory(conn, hostId, "bdwHost", "name", npu.host, status);
int submissionDirId = bdwGetSubmissionDir(conn, hostId, submissionDir);
recordIntoHistory(conn, submissionDirId, "bdwSubmissionDir", "url", submissionDir, status);
#endif /* SOON */

/* Clean up and go home. */
errCatchFree(&errCatch);
return sd;
}

void bdwCheckWorthAttempting(char *url)
/* See if it's even worth attempting to connect with this URL.  We may know
 * host is down.... */
{
/* For now we do nothing, assume it's ok. */
}
    

struct fieldedTable *fieldedTableFromTabFile(char *url, char *requiredFields[], int requiredCount)
/* Read table, possible from remote url. */
{
struct lineFile *lf = lineFileOpen(url, TRUE);
char *line;

/* Get first line and turn it into field list. */
if (!lineFileNext(lf, &line, NULL))
   errAbort("%s is empty", url);
if (line[0] != '#')
   errAbort("%s must start with '#' and field names on first line", url);
line = skipLeadingSpaces(line+1);
int fieldCount = chopByChar(line, '\t', NULL, 0);
char *fields[fieldCount];
chopTabs(line, fields);

/* Make sure that all required fields are present. */
int i;
for (i = 0; i < requiredCount; ++i)
    {
    char *required = requiredFields[i];
    int ix = stringArrayIx(required, fields, fieldCount);
    if (ix < 0)
        errAbort("%s is missing required field '%s'", url, required);
    }

/* Create fieldedTable . */
struct fieldedTable *table = fieldedTableNew(url, fields, fieldCount);
while (lineFileRowTab(lf, fields))
    {
    fieldedTableAdd(table, fields, fieldCount, lf->lineIx);
    }


/* Clean up and go home. */
lineFileClose(&lf);
return table;
}

void cgiEncodeIntoDy(char *var, char *val, struct dyString *dy)
/* Add a CGI-encoded &var=val string to dy. */
{
if (dy->stringSize != 0)
    dyStringAppendC(dy, '&');
dyStringAppend(dy, var);
dyStringAppendC(dy, '=');
char *s = cgiEncode(val);
dyStringAppend(dy, s);
freez(&s);
}


struct bdwFile *bdwFileFromFieldedTable(struct fieldedTable *table,
    int fileIx, int md5Ix, int sizeIx, int modifiedIx)
/* Turn parsed out table (still all just strings) into list of bdwFiles. */
{
struct bdwFile *bf, *bfList = NULL;
struct fieldedRow *fr;
struct dyString *tags = dyStringNew(0);
for (fr = table->rowList; fr != NULL; fr = fr->next)
    {
    char **row = fr->row;
    AllocVar(bf);
    bf->submitFileName = cloneString(row[fileIx]);
    safef(bf->md5, sizeof(bf->md5), "%s", row[md5Ix]);
    bf->size = sqlLongLong(row[sizeIx]);
    bf->updateTime = sqlLongLong(row[modifiedIx]);

    /* Add as tags any fields not included in fixed fields. */
    dyStringClear(tags);
    int i;
    for (i=0; i<table->fieldCount; ++i)
        {
	if (i != fileIx && i != md5Ix && i != sizeIx && i != modifiedIx)
	    {
	    cgiEncodeIntoDy(table->fields[i], row[i], tags);
	    }
	}
    bf->tags = cloneString(tags->string);

    /* Fake other fields. */
    strcpy(bf->licensePlate, "fakeLicense");
    bf->bdwFileName  = cloneString("fakeBdwFileName");
    slAddHead(&bfList, bf);
    }
slReverse(&bfList);
dyStringFree(&tags);
return bfList;
}

#ifdef EXAMPLE
void netParseUrl(char *url, struct netParsedUrl *parsed);
struct netParsedUrl
/* A parsed URL. */
   {
   char protocol[16];	/* Protocol - http or ftp, etc. */
   char user[128];	/* User name (optional)  */
   char password[128];	/* Password  (optional)  */
   char host[128];	/* Name of host computer - www.yahoo.com, etc. */
   char port[16];       /* Port, usually 80 or 8080. */
   char file[1024];	/* Remote file name/query string, starts with '/' */
   ssize_t byteRangeStart; /* Start of byte range, use -1 for none */
   ssize_t byteRangeEnd;   /* End of byte range use -1 for none */
   };
#endif /* EXAMPLE */

int makeNewEmptySubmissionRecord(struct sqlConnection *conn, char *submissionUrl, 
    unsigned char *userSid)
/* Create a submission record around URL and return it's id. */
{
char hexedUserSid[2*BDW_SHA_SIZE+1];
hexBinaryString(userSid, BDW_SHA_SIZE, hexedUserSid, sizeof(hexedUserSid));

struct dyString *query = dyStringNew(0);
dyStringAppend(query, 
    "insert bdwSubmission "
    "(url, startUploadTime, endUploadTime, userSid, submitFileId, errorMessage) ");
dyStringPrintf(query, "VALUES('%s', %lld, %lld, 0x%s, 0, '')",
    submissionUrl, (long long)time(NULL), (long long)0, hexedUserSid);
uglyf("query=%s\n", query->string);
sqlUpdate(conn, query->string);
dyStringFree(&query);
return sqlLastAutoId(conn);
}

void bdwSubmit(char *submissionUrl, char *user, char *password)
/* bdwSubmit - Submit URL with validated.txt to warehouse. */
{
/* Parse out url a little into submissionDir and submissionFile */
char *lastSlash = strrchr(submissionUrl, '/');
if (lastSlash == NULL)
    errAbort("%s is not a valid URL - it has no '/' in it.", submissionUrl);
char *submissionFile = lastSlash+1;
int submissionDirSize = submissionFile - submissionUrl;
char submissionDir[submissionDirSize+1];
memcpy(submissionDir, submissionUrl, submissionDirSize);
submissionDir[submissionDirSize] = 0;  // Add trailing zero
uglyf("%s -> %s %s\n", submissionUrl, submissionDir, submissionFile);

/* Make sure user has access. */
struct sqlConnection *conn = sqlConnect(bdwDatabase);
unsigned char userSid[BDW_SHA_SIZE];
bdwMustHaveAccess(conn, user, password, userSid);

/* Make a submission record. */
int submissionId = makeNewEmptySubmissionRecord(conn, submissionUrl, userSid);
uglyf("submissionId = %d\n", submissionId);

#ifdef SOON
/* Copy over manifest file */
int fd = bdwOpenAndRecord(conn, submissionDir, submissionFile, submissionUrl);
int fileId = makeNewEmptyFileRecord(conn, submissionId, submissionDir, submissionFile);
bdwFetchFileNoCheck(conn, fileId);
char fileNameOnServer[PATH_LEN];
bdwFileNameOnServer(conn, fileId, fileNameOnServer);

/* About here is where we should wrap things in an errCatch so can put error message in 
 * the bdwSubmission.errorMessage field. */

/* Fill in MD5sum and the like of submission file */
unsigned char md5bin[16];
md5ForFile(fileNameOnServer, md5bin);
char md5[33];
hexBinaryString(md5bin, sizeof(md5bin), md5, sizeof(md5));
time_t updateTime = fileModTime(fileNameOnServer);
off_t size = fileSize(fileNameOnServer);
char query[256];
safef(query, sizeof(query), 
    "update bdwFile set updateTime = %lld, size=%lld, md5=0x%s where id=%u\n",
    (long long)updateTime, (long long)size, md5, fileId);
sqlUpdate(conn, query);

/* Load up submission file as fielded table, make sure all required fields are there,
 * and calculate indexes of required fields. */
char *requiredFields[] = {"file_name", "md5_sum", "size", "modified"};
struct fieldedTable *table = fieldedTableFromTabFile(fileNameOnServer, 
	requiredFields, ArraySize(requiredFields));
int fileIx = stringArrayIx("file_name", table->fields, table->fieldCount);
int md5Ix = stringArrayIx("md5_sum", table->fields, table->fieldCount);
int sizeIx = stringArrayIx("size", table->fields, table->fieldCount);
int modifiedIx = stringArrayIx("modified", table->fields, table->fieldCount);

struct bdwFile *bfList = bdwFileFromFieldedTable(table, fileIx, md5Ix, sizeIx, modifiedIx);
uglyf("Got %d files in bfList\n", slCount(bfList));
struct bdwFile *bf;
for (bf = bfList; bf != NULL; bf = bf->next)
    {
    bdwFileTabOut(bf, uglyOut); 
    }
sqlDisconnect(&conn);
#endif /* SOON */
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
bdwSubmit(argv[1], argv[2], argv[3]);
return 0;
}
