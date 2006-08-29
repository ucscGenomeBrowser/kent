/* ccdsCommon - common functions used by CCDS programs */
#ifndef CCDSCOMMON_H
#define CCDSCOMMON_H
struct sqlConnection;

void ccdsGetTblFileNames(char *tblFile, char *table, char *tabFile);
/* Get table and tab file name from a single input name.  If specified names
 * looks like a file name, use it as-is, otherwise generate a file name.
 * Output buffers should be PATH_LEN bytes long.  Either output argument maybe
 * be NULL. */

void ccdsRenameTable(struct sqlConnection *conn, char *oldTable, char *newTable);
/* rename a database table */

struct hash *ccdsStatusValLoad(struct sqlConnection *conn);
/* load values from the imported ccdsStatusVals table.  Table hashes
 * status name to uid.  Names are loaded both as-is and lower-case */

void ccdsStatusValCheck(struct hash *statusVals, char *stat);
/* check if a status value is valid, in a case-insensitive manner */

#endif
