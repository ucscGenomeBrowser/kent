#include "sqlUpdater.h"
#include "common.h"
#include "hgRelate.h"
#include "gbVerb.h"
#include "jksql.h"
#include "localmem.h"
#include "gbFileOps.h"
#include "errabort.h"

static char const rcsid[] = "$Id: sqlUpdater.c,v 1.8 2006/03/11 00:07:59 markd Exp $";

#define UPDATE_QUERY_MAX (10*1024)

struct sqlUpdateCmd 
/* object to hold one update command */
{
    struct sqlUpdateCmd* next;
    int numRows;                 /* number of rows to update */
    char cmd[1];                 /* command start here (must be last) */
};

struct sqlUpdater* sqlUpdaterNew(char* table, char* tmpDir,
                                 boolean verbEnabled,
                                 struct sqlUpdater** head)
/* create a new object associated with a table.  If head is not null,
 * add to the list. */
{
struct sqlUpdater* su;
AllocVar(su);
su->verbose = verbEnabled;
safef(su->table, sizeof(su->table), "%s", table);
safef(su->tabFile, sizeof(su->tabFile), "%s/%s.tab", tmpDir, table);
if (head != NULL)
    slAddHead(head, su);
gbMakeDirs(tmpDir);
return su;
}

void sqlUpdaterFree(struct sqlUpdater** suPtr)
/* Free an object */
{
struct sqlUpdater* su = *suPtr;
if (su != NULL)
    {
    /* require commit */
    assert((su->updateCmds == NULL) && (su->tabFh == NULL));
    freez(suPtr);
    }
}

void sqlUpdaterAddRow(struct sqlUpdater* su, char* format, ...)
/* Queue adding a new row to the queue of entries to update */
{
va_list args;
if (su->tabFh == NULL)
    su->tabFh = mustOpen(su->tabFile, "w");
va_start(args, format);
vfprintf(su->tabFh, format, args);
va_end(args);
fputc('\n', su->tabFh);
if (ferror(su->tabFh))
    errnoAbort("writing %s", su->tabFile);
su->numAdds++;
}

FILE *sqlUpdaterGetFh(struct sqlUpdater* su, int addCount)
/* Get the file handle to the tab file, used for using autoSql output
 * functions, increments count the the specified amount.*/
{
if (su->tabFh == NULL)
    su->tabFh = mustOpen(su->tabFile, "w");
su->numAdds += addCount;
return su->tabFh;
}

void sqlUpdaterModRow(struct sqlUpdater* su, int numRows, char* format, ...)
/* Queue modify an existing row of the table.  Arguments should be
 * the contents of UPDATE after the set (fields and WHERE). numRows
 * is the number of rows that should be updated (which is checked).
 */
{
va_list args;
char query[UPDATE_QUERY_MAX];
struct sqlUpdateCmd* cmd;
if (su->lm == NULL)
    su->lm = lmInit(64*1024);
va_start(args, format);
vasafef(query, sizeof(query), format, args);
va_end(args);

cmd = lmAlloc(su->lm, sizeof(*cmd)+strlen(query));
strcpy(cmd->cmd, query);
cmd->numRows = numRows;
slAddHead(&su->updateCmds, cmd);
su->numUpdates++;
}

static void updateRows(struct sqlUpdater* su, struct sqlConnection *conn)
/* execute update commands  */
{
char query[UPDATE_QUERY_MAX+256];
struct sqlUpdateCmd *cmd;
int numMatched;

/* boolean arg enables for current verbose level (so level is under control
 * of caller) */
if (su->verbose)
    gbVerbMsg(gbVerbose, "update %d rows in %s", su->numUpdates,
              su->table);
        
for (cmd = su->updateCmds; cmd != NULL; cmd = cmd->next)
    {
    safef(query, sizeof(query), "UPDATE %s SET %s", su->table, cmd->cmd);
    sqlUpdateRows(conn, query, &numMatched);
    if (numMatched != cmd->numRows)
        errAbort("expected %d matching row(s) on update of %s, got %d: %s",
                 cmd->numRows, su->table, numMatched, query);
    }
lmCleanup(&su->lm);
su->updateCmds = NULL;
}

static void addRows(struct sqlUpdater* su, struct sqlConnection *conn)
/* add new rows  */
{
if (su->verbose)
    {
    if (su->numAdds < 0)
        gbVerbMsg(gbVerbose, "add rows to %s", su->table);
    else
        gbVerbMsg(gbVerbose, "add %d rows to %s", su->numAdds,
                  su->table);
    }
if (ferror(su->tabFh))
    errnoAbort("writing %s", su->tabFile);
carefulClose(&su->tabFh);
sqlLoadTabFile(conn, su->tabFile, su->table,
               SQL_TAB_FILE_ON_SERVER);
}

void sqlUpdaterCommit(struct sqlUpdater* su, struct sqlConnection *conn)
/* commit pending changes  */
{
if (su->updateCmds != NULL)
    updateRows(su, conn);
if (su->tabFh != NULL)
    addRows(su, conn);
}

void sqlUpdaterCancel(struct sqlUpdater* su)
/* drop pending changes  */
{
lmCleanup(&su->lm);
su->updateCmds = NULL;
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */


