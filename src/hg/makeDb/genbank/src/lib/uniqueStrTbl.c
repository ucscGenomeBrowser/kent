#include "common.h"
#include "uniqueStrTbl.h"
#include "gbFileOps.h"
#include "hash.h"
#include "jksql.h"
#include "sqlUpdater.h"

static char const rcsid[] = "$Id: uniqueStrTbl.c,v 1.3 2005/08/22 19:55:35 markd Exp $";

/*
 * The id is not auto_increment, as the zero id didn't work with mysqlimport
 * and zero had to be hacked in by explictly changing the id.
 *
 * Using incremental loading of these tables significantly reduced the memory
 * required to do updates.  However select by name was very slow when a table
 * had a large number of rows (mysql 3.23.54, mrnaClone table).  The
 * information returned by examine was inconsistent; sometimes it reported
 * that the entire table would be searched. The crc column was added to
 * address this.  However, examine reported the crc index was not used,
 * however it was signficanly faster.  Forcing the use of the crc index was
 * not as fast (but still faster than the original).  This is most likely a
 * mysql bug.  To test this, load the unique string tables, but drop the other
 * tables and load on of the large EST partations (full est.bi), compare the
 * time on the metadata update.
 */
static char* createSql =
    "CREATE TABLE %s (" 
    "id INT NOT NULL PRIMARY KEY,"
    "name LONGTEXT NOT NULL,"
    "crc INT UNSIGNED NOT NULL,"   /* must be last for compat */
    "INDEX(name(32)),"
    "INDEX(crc))";


static void createTable(struct uniqueStrTbl* ust, struct sqlConnection *conn)
/* create a new unique string table */
{
static char* NA_STR = "n/a";
char query[1024];
safef(query, sizeof(query), createSql, ust->updater->table);
sqlRemakeTable(conn, ust->updater->table, query);
/* Add entry for null value, which will display n/a. */
safef(query, sizeof(query), "INSERT INTO %s VALUES(0,'%s',%u)",
      ust->updater->table, NA_STR, hashCrc(NA_STR));
sqlUpdate(conn, query);
ust->nextId = 1;
}

static void loadTable(struct uniqueStrTbl* ust, struct sqlConnection *conn)
/*  an existing string table */
{
char query[128];
struct sqlResult* sr;
char **row;
safef(query, sizeof(query), "SELECT id,name FROM %s", ust->updater->table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    HGID id = gbParseUnsigned(NULL, row[0]);
    if (id != 0)
        hashAddInt(ust->hash, row[1], id);
    }
sqlFreeResult(&sr);
}

struct uniqueStrTbl *uniqueStrTblNew(struct sqlConnection *conn,
                                     char *table, int hashPow2Size,
                                     boolean prefetch, char *tmpDir,
                                     boolean verbose)
/* Create an object to accessed the specified unique string table, creating
 * table if it doesn't exist.  Optionally prefetch the table into memory. */
{
struct uniqueStrTbl *ust;

AllocVar(ust);
ust->hash = hashNew(hashPow2Size);
ust->updater = sqlUpdaterNew(table, tmpDir, verbose, NULL);

if (!sqlTableExists(conn, table))
    createTable(ust, conn);
else
    {
    if (prefetch)
        loadTable(ust, conn);
    ust->nextId = hgGetMaxId(conn, ust->updater->table) + 1;
    }

return ust;
}

static struct hashEl *loadStr(struct uniqueStrTbl *ust, 
                              struct sqlConnection* conn, char* str)
/* load a string if it already exists in the database */
{
struct hashEl* hel = NULL;
bits32 crc = hashCrc(str);
char *escStr = sqlEscapeString2(alloca(2*strlen(str)+1), str);
int qlen = strlen(escStr)+256;
char *query = alloca(qlen);
HGID id;

safef(query, qlen, "SELECT id FROM %s WHERE (crc = %u) and (name = '%s')",
      ust->updater->table, crc, escStr);

id = sqlQuickNum(conn, query);
if (id != 0)
    hel = hashAddInt(ust->hash, str, id);
return hel;
}

HGID uniqueStrTblFind(struct uniqueStrTbl *ust, struct sqlConnection* conn,
                      char* str, char** strVal)
/* Lookup a string in the table, returning the id.  If strVal is not NULL,
 * a pointer to the string in the table is returned.  If not found, return
 * 0 and NULL in strVal */
{
struct hashEl *hel;
hel = hashLookup(ust->hash, str);

if (hel == NULL)
    hel = loadStr(ust, conn, str);
if (hel != NULL)
    {
    if (strVal != NULL)
        *strVal = hel->name;
    return (unsigned)(size_t)hel->val;
    }
else
    {
    if (strVal != NULL)
        *strVal = NULL;
    return 0;
    }
}

static HGID uniqueStrTblAdd(struct uniqueStrTbl *ust,
                            struct sqlConnection *conn, char* str,
                            char** strVal)
/* Add a string that is know not to exists (as indicated by
 * uniqueStrTblFind).  */
{
struct hashEl *el;
bits32 crc = hashCrc(str);
char *escStr = sqlEscapeTabFileString2(alloca(2*strlen(str)+1), str);

sqlUpdaterAddRow(ust->updater, "%u\t%s\t%u", ust->nextId, escStr, crc);
el = hashAddInt(ust->hash, str, ust->nextId);
ust->nextId++;
if (strVal != NULL)
    *strVal = el->name;
return (unsigned)(size_t)el->val;
}

HGID uniqueStrTblGet(struct uniqueStrTbl *ust, struct sqlConnection *conn,
                     char* str, char** strVal)
/* Lookup a string in the table.  If it doesn't exists, add it.  If strVal is
 * not NULL, a pointer to the string in the table is returned.  */
{
HGID id = uniqueStrTblFind(ust, conn, str, strVal);
if (id == 0)
    id = uniqueStrTblAdd(ust, conn, str, strVal);
return id;
}

void uniqueStrTblCommit(struct uniqueStrTbl* ust, struct sqlConnection* conn)
/* commit pending table changes */
{
sqlUpdaterCommit(ust->updater, conn);
}

void uniqueStrTblFree(struct uniqueStrTbl** ust)
/* Free a uniqueStrTbl object */
{
sqlUpdaterFree(&(*ust)->updater);
#ifdef DUMP_HASH_STATS
hashPrintStats((*ust)->hash, "uniqueStr", stderr);
#endif
hashFree(&(*ust)->hash);
freez(ust);
}
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

