/* edwFakeEncode2Submit - Fake up biosample table from meld of encode3 and encode2 sources.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"

struct hash *hg19DateHash, *hg19LabHash, *mm9DateHash, *mm9LabHash, *labUserHash;
boolean really;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwFakeEncode2Submit - Fake up edwSubmit records for encode2 files imported to encode3 data warehouse.\n"
  "usage:\n"
  "   edwFakeEncode2Submit encode2labs.tsv hg19_encode2_date.tab mm9_encode2_date.tab output.sql\n"
  "Where encode2labs.tsv is provided by encodedcc's redmine #1079.\n"
  "and hg19_encode2_date.tab and mm9_encode2_date.tab are the result of running the command on hgw1\n"
  "    mdbQuery \"select metaObject,lab,dateSubmitted,dateResubmitted,dateUnrestricted from hg19\" and \n"
  "    mdbQuery \"select metaObject,lab,dateSubmitted,dateResubmitted,dateUnrestricted from mm9\" "
  "and both files also live in the source tree.\n"
  "The output.sql is intended for sql statements to create the faked edwSubmit records.\n"
  "options:\n"
  "   -really - Needs to be set for anything to happen,  otherwise will just print sql statements.\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
    {"really", OPTION_BOOLEAN},
    {NULL, 0},
};

void createEncode2Hash(char *fName, struct hash *labHash, struct hash *dateHash)
/* Read the mdbQuery encode2 objects output hg19_encode2_date.tab and
 * mm9_encode2_date.tab, create two hash table to keep track the
 * provider 
 * and imported date of the encode2 object */
{
struct lineFile *lf = lineFileOpen(fName, TRUE);

char *row[5];
while (lineFileRow(lf, row))
    {
    char *object = row[0];
    char *lab = row[1];
    char *dateSubmit = row[2];
    char *dateReSubmit = row[3];
    char *dateImport;
    if (!sameWord(lab, "n/a") &&
	(!sameWord(lab, "n/a") || !sameWord(lab, "n/a")))
	{
    if (!sameWord(dateReSubmit, "n/a"))
        dateImport = dateReSubmit;
    else
        dateImport = dateSubmit;
	hashAdd(labHash, object, cloneString(lab));
	hashAdd(dateHash, object, cloneString(dateImport));
	}
    }
lineFileClose(&lf);
verbose(1, "Read %d from %s\n", labHash->elCount, fName);
}

void createLabUserHash(char *fName, struct hash *labUserHash)
/* Create the "Lab : email" hash from encode2labs.tsv */
{
struct lineFile *lf = lineFileOpen(fName, TRUE);
char *row[5];
while (lineFileRow(lf, row))
    {
    char *lab = row[0];
    char *userName = row[3];
    hashAdd(labUserHash, lab, cloneString(userName));
    }
}

void maybeDoUpdate(struct sqlConnection *conn, char *query, boolean really, FILE *f)
/* If really is true than do sqlUpdate with query, otherwise just print
 * it out. */
{
if (really)
    sqlUpdate(conn, query);
else
    fprintf(f,"Update: %s\n", query);
}

int createSubmitRow(struct sqlConnection *conn, char *url, long long ulTime, int uId, int nFile, long long tSize, char *nameLike, FILE *f)
/* create a new edwSubmit row, return the new row id. Return '90909090'
 * if really option is off. */
{
char query[512];
sqlSafef(query, sizeof(query),
    "insert into edwSubmit (url, startUploadTime, endUploadTime, userId, fileCount, newFiles, byteCount, newBytes) values (\"%s\", %lld, %lld, %d, %d, %d, %lld, %lld)",
    url, ulTime, ulTime, uId, nFile, nFile, tSize, tSize);
maybeDoUpdate(conn, query, really, f);

if (really)
    {
    sqlSafef(query, sizeof(query), "select id  from edwSubmit where url  like '%s'", nameLike);
    return sqlQuickNum(conn, query);
    }
else
    return 90909090;
}

void updateEdwFile(struct sqlConnection *conn, struct slName *fList, int sId, long long ulTime, char *nameLike, FILE *f)
/* once the encode2 object's edwSubmit row is created, update the
 * corresponding edwFile row(s) with the submit id and imported date. */ 
{
struct slName *sln;
char query[512];
for (sln = fList;  sln != NULL;  sln = sln->next)
    {
    sqlSafef(query, sizeof(query),
	"update edwFile set submitId = %d, startUploadTime = %lld, endUploadTime = %lld, updateTime = %lld  where SubmitFilename like '%s'", 
        sId, ulTime, ulTime, ulTime, nameLike);
    maybeDoUpdate(conn, query, really, f);
    }
}

void createEdwSubmitRow(struct sqlConnection *conn, struct hash *labHash, struct hash *dateHash, struct hash *labUserHash, FILE *f)
/* Create a fake edwSubmit row for each object imported from encode2 */
{
struct sqlResult *sr = NULL;
char **row = NULL;
char query[512];
struct hashEl *hel, *helList = hashElListHash(labHash);
char *name, *value;
for (hel = helList; hel != NULL; hel = hel->next)
    {
    name = hel->name;
    value = hel->val;
    char *submitLab = hashMustFindVal(labHash, name);
    char *dateImport = hashMustFindVal(dateHash, name); 
    char *submitUserMail = hashMustFindVal(labUserHash, submitLab);
    char submitUserName[256];
    strcpy(submitUserName, submitUserMail); 
    chopSuffixAt(submitUserName, '@');

    char nameLike[256];
    char *percent = "%";
    safef(nameLike, sizeof(nameLike), "%s%s%s", percent, name, percent);
    sqlSafef(query, sizeof(query), "select * from edwFile where SubmitFilename like '%s'", nameLike);
    sr = sqlGetResult(conn, query);
    int nFile = 0;
    long long tSize = 0;
    struct slName *list = NULL, *n;
    while ((row = sqlNextRow(sr)) != NULL)
	{
	struct edwFile *edwfile = edwFileLoad(row);
	nFile += 1;
	tSize += edwfile->size;
	/* remnber the submitFileName */
	n = slNameNew(edwfile->submitFileName);
	slAddHead(&list, n);
	} /* end while */	
    sqlFreeResult(&sr);
    if (nFile > 0 && list != NULL)
	{
        char url[512];
	safef(url, sizeof(url), "http://encodedcc.sdsc.edu/encode2/%s/users/%s", name, submitUserName);
	long long ulTime = dateToSeconds(dateImport, "%Y-%m-%d");
        struct edwUser *user = edwMustGetUserFromEmail(conn,submitUserMail);
	int sId = createSubmitRow(conn, url, ulTime, user->id, nFile, tSize, nameLike, f);
        if (sId > 0)
	    updateEdwFile(conn, list, sId, ulTime, nameLike, f);
	slFreeList(&list);
	}

    } /* end for each ele in list */
}

void edwFakeEncode2Submit(char *tsvLab, char *tsvHg19, char *tsvMm9, char *outSql)
/* edwFakeEncode2Submit - Fake up edwSubmit records for encode2 files imported to encode3 data warehouse. */
{
FILE *f = mustOpen(outSql, "w");
hg19LabHash = hashNew(0);
hg19DateHash = hashNew(0);
createEncode2Hash(tsvHg19, hg19LabHash, hg19DateHash);
mm9LabHash = hashNew(0);
mm9DateHash = hashNew(0);
createEncode2Hash(tsvMm9, mm9LabHash, mm9DateHash);
labUserHash = hashNew(0);
createLabUserHash(tsvLab, labUserHash);
verbose(1, "labUserHash has  %d elements.\n", labUserHash->elCount);
verbose(1, "hg19LabHash has  %d elements.\n", hg19LabHash->elCount);
verbose(1, "mm9DateHash has  %d elements.\n", mm9DateHash->elCount);
struct sqlConnection *conn =  edwConnectReadWrite(edwDatabase);
createEdwSubmitRow(conn, hg19LabHash, hg19DateHash, labUserHash,f);
createEdwSubmitRow(conn, mm9LabHash, mm9DateHash, labUserHash,f);
carefulClose(&f);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
really = optionExists("really");
if (really) verbose(1, "Really going to do it! \n");
if (argc != 5)
    usage();
edwFakeEncode2Submit(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
