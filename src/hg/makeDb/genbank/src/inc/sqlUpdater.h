#ifndef SQLUPDATER_H

#include "common.h"
struct sqlConnection;

struct sqlUpdater
/* Object to handle incremental update of sql tables.  This builds a list
 * of sql commands to update existing rows and a tab file of new data to
 * load.  These are committed as batch. */
{
    struct sqlUpdater* next;
    char table[128];                  /* name of the table */
    char tabFile[PATH_LEN];           /* path to tab file in tmp dir */
    struct lm* lm;                    /* local memory for sqlUpdate cmds */
    struct sqlUpdateCmd* updateCmds;  /* SQL commands to do update */
    FILE* tabFh;                      /* Tab-seperated file being built */
    int numUpdates;                   /* number of updates queued */
    int numAdds;                      /* number of adds queued, -1 if unknown */
    boolean verbose;                  /* verbose enabled */
};

struct sqlUpdater* sqlUpdaterNew(char* table, char* tmpDir,
                                 boolean verbEnabled,
                                 struct sqlUpdater** head);
/* create a new object associated with a table */

void sqlUpdaterFree(struct sqlUpdater** suPtr);
/* Free an object */

void sqlUpdaterAddRow(struct sqlUpdater* su, char* format, ...)
/* Queue adding a new row to the queue of entries to update */
#ifdef __GNUC__
__attribute__((format(printf, 2, 3)))
#endif
;

FILE *sqlUpdaterGetFh(struct sqlUpdater* su, int addCount);
/* Get the file handle to the tab file, used for using autoSql output
 * functions, increments count the the specified amount.*/

void sqlUpdaterModRow(struct sqlUpdater* su, int numRows, char* format, ...)
/* Queue modify an existing row of the table.  Arguments should be
 * the contents of UPDATE after the set (fields and WHERE). numRows
 * is the number of rows that should be updated (which is checked).
 */
#ifdef __GNUC__
__attribute__((format(printf, 3, 4)))
#endif
;

void sqlUpdaterCommit(struct sqlUpdater* su, struct sqlConnection *conn);
/* commit pending changes */

void sqlUpdaterCancel(struct sqlUpdater* su);
/* drop pending changes  */

#endif

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

