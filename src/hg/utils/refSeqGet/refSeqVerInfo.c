/* object to track refseqs that are to be retrieved */
#include "common.h"
#include "refSeqVerInfo.h"
#include "linefile.h"
#include "hash.h"
#include "jksql.h"
#include "sqlNum.h"

enum refSeqVerInfoStatus
/* validation status */
{
    refSeqVerInfoOk,
    refSeqVerInfoIgnore,  // duplicate, ignore
    refSeqVerInfoError
};

static int refSeqVerInfoCmp(const void *va, const void *vb)
/* Compare by accession. */
{
const struct refSeqVerInfo *a = *((struct refSeqVerInfo **)va);
const struct refSeqVerInfo *b = *((struct refSeqVerInfo **)vb);
return strcmp(a->acc, b->acc);
}


static struct refSeqVerInfo *refSeqVerInfoNewDb(char **row)
/* construct a refSeqVerInfo object from a database query */
{
struct refSeqVerInfo *rsvi;
AllocVar(rsvi);
rsvi->acc = cloneString(row[0]);
rsvi->ver = sqlSigned(row[1]);
return rsvi;
}

struct hash *refSeqVerInfoFromDb(struct sqlConnection *conn, boolean getNM, boolean getNR, struct refSeqVerInfo **refSeqVerInfoList)
/* load refSeqVerInfo table for all native refseqs in the database */
{
struct hash *refSeqVerInfoTbl = hashNew(18);
*refSeqVerInfoList = NULL;
char *accRestrict = "";
if (getNM && !getNR)
    accRestrict = " AND (acc LIKE \"NM%\")";
else if (!getNM && getNR)
    accRestrict = " AND (acc LIKE \"NR%\")";
char query[128];
safef(query, sizeof(query),
      "SELECT acc, version FROM gbStatus WHERE (srcDb = \"RefSeq\") AND (orgCat = \"native\")%s", accRestrict);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct refSeqVerInfo *rsvi = refSeqVerInfoNewDb(row);
    hashAdd(refSeqVerInfoTbl, rsvi->acc, rsvi);
    slAddHead(refSeqVerInfoList, rsvi);
    }

sqlFreeResult(&sr);
slSort(refSeqVerInfoList, refSeqVerInfoCmp);
return refSeqVerInfoTbl;
}

static struct refSeqVerInfo *refSeqVerInfoNewFile(char *acc)
/* construct a refSeqVerInfo object from a file */
{
struct refSeqVerInfo *rsvi;
AllocVar(rsvi);
char *dot = strchr(acc, '.');
rsvi->acc = cloneStringZ(acc, ((dot != NULL) ? (dot - acc) : strlen(acc)));
if (dot != NULL)
    rsvi->requestVer = sqlUnsigned(dot+1);
return rsvi;
}

static void prAccReqVer(struct refSeqVerInfo *rsvi, FILE *fh)
/* print acc + requested version, or just acc if no version */
{
fputs(rsvi->acc, fh);
if (rsvi->requestVer != 0)
    fprintf(fh, ".%d", rsvi->requestVer);
}

static enum refSeqVerInfoStatus dupCheck(struct hash *refSeqVerInfoTbl, struct refSeqVerInfo *rsvi, struct sqlConnection *conn)
{
struct refSeqVerInfo *rsvi0 = hashFindVal(refSeqVerInfoTbl, rsvi->acc);
if (rsvi0 != NULL)
    {
    if (rsvi0->requestVer == rsvi->requestVer)
        return refSeqVerInfoIgnore;  // already there with the same version, ignore
    else
        {
        fprintf(stderr, "Error: RefSeq %s specified multiple times in accList, once as ", rsvi->acc);
        prAccReqVer(rsvi0, stderr);
        fputs(" and once as ", stderr);
        prAccReqVer(rsvi0, stderr);
        fputc('\n', stderr);
        return refSeqVerInfoError;
        }
    }
else
    return refSeqVerInfoOk;
}

int refSeqVerInfoGetVersion(char *acc, struct sqlConnection *conn)
/* get the version from the database, or zero if accession is not found */
{
char query[128];
safef(query, sizeof(query),
      "SELECT version FROM gbSeq WHERE (acc = \"%s\")", acc);
return sqlQuickNum(conn, query);
}

static enum refSeqVerInfoStatus versionGetCheck(struct refSeqVerInfo *rsvi, struct sqlConnection *conn)
/* get or validate the version from the file against the database */
{
int dbVer = refSeqVerInfoGetVersion(rsvi->acc, conn);
if (dbVer == 0)
    {
    fprintf(stderr, "Error: RefSeq %s not in database\n", rsvi->acc);
    return refSeqVerInfoError;
    }
if ((rsvi->requestVer != 0) && (dbVer != rsvi->requestVer))
    {
    fprintf(stderr, "Error: RefSeq %s.%d requested in accList, database contains %s.%d \n", rsvi->acc, rsvi->requestVer, rsvi->acc, dbVer);
    return refSeqVerInfoError;
    }
rsvi->ver = dbVer;
return refSeqVerInfoOk;
}

static enum refSeqVerInfoStatus fromFileAdd(struct hash *refSeqVerInfoTbl, struct refSeqVerInfo *rsvi, struct sqlConnection *conn, struct refSeqVerInfo **refSeqVerInfoList)
/* add a refseq parsed from a file to the table, validating against database
 * and setting the actually version number. */
{
enum refSeqVerInfoStatus stat = dupCheck(refSeqVerInfoTbl, rsvi, conn);
if (stat != refSeqVerInfoOk)
    return stat;
stat = versionGetCheck(rsvi, conn);
if (stat != refSeqVerInfoOk)
    return stat;
hashAdd(refSeqVerInfoTbl, rsvi->acc, rsvi);
slAddHead(refSeqVerInfoList, rsvi);
return refSeqVerInfoOk;
}

struct hash *refSeqVerInfoFromFile(struct sqlConnection *conn, char *accList, struct refSeqVerInfo **refSeqVerInfoList)
/* load refSeqVerInfo table for all native refseqs specified in a file, then validate it against
 * the database. */
{
struct hash *refSeqVerInfoTbl = hashNew(18);
*refSeqVerInfoList = NULL;
struct lineFile *lf = lineFileOpen(accList, TRUE); 
int errCnt = 0;
char *line;
while (lineFileNextReal(lf, &line))
    {
    char *acc = trimSpaces(line);
    if (fromFileAdd(refSeqVerInfoTbl, refSeqVerInfoNewFile(acc), conn, refSeqVerInfoList) == refSeqVerInfoError)
        errCnt++;
    }
lineFileClose(&lf);
if (errCnt > 0)
    errAbort("%d errors detected loading RefSeq accessioned from %s", errCnt, accList);
slSort(refSeqVerInfoList, refSeqVerInfoCmp);
return refSeqVerInfoTbl;
}

