/* ccdsCommon - common functions used by CCDS programs */
#ifndef CCDSCOMMON_H
#define CCDSCOMMON_H
struct sqlConnection;

void ccdsGetTblFileNames(char *tblFile, char *table, char *tabFile);
/* Get table and tab file name from a single input name.  If specified names
 * looks like a file name, use it as-is, otherwise generate a file name.
 * Output buffers should be PATH_LEN bytes long. */

void ccdsRenameTable(struct sqlConnection *conn, char *oldTable, char *newTable);
/* rename a database table */

#endif
