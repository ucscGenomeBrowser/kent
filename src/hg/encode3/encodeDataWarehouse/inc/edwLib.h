/* edwLib - routines shared by various encodeDataWarehouse programs.    See also encodeDataWarehouse
 * module for tables and routines to access structs built on tables. */

#ifndef EDWLIB_H
#define EDWLIB_H

#ifndef JKSQL_H
#include "jksql.h"
#endif

#define EDW_ACCESS_SIZE 65    /* Size of our access key - currently base64 encoded SHA384 with 
                               * NULL terminator */

extern char *edwDatabase;   /* Name of database we connect to. */
extern char *edwRootDir;    /* Name of root directory for our files, including trailing '/' */

void edwMakeAccess(char *password, char access[EDW_ACCESS_SIZE]);
/* Convert password + salt to an access code */

int edwCheckAccess(struct sqlConnection *conn, char *user, char *password);
/* Make sure user exists and password checks out. Returns (non-zero) user ID on success*/

int edwMustHaveAccess(struct sqlConnection *conn, char *user, char *password);
/* Check user has access and abort with an error message if not. Returns user id. */

int edwCheckUserNameSize(char *user);
/* Make sure user name not too long. Returns size or aborts if too long. */

void edwMakeSid(char *user, char sid[EDW_ACCESS_SIZE]);
/* Convert users to sid */

#endif /* EDWLIB_H */
