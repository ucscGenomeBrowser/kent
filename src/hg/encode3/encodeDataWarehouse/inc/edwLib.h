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

void edwMakeAccess(char *user, char *password, char access[EDW_ACCESS_SIZE]);
/* Convert user + password + salt to an access code */

boolean edwCheckAccess(struct sqlConnection *conn, char *user, char *password, 
    char retSid[EDW_ACCESS_SIZE]);
/* Make sure user exists and password checks out. */

void edwMustHaveAccess(struct sqlConnection *conn, char *user, char *password,
    char retSid[EDW_ACCESS_SIZE]);
/* Check user has access and abort with an error message if not. */

int edwCheckEmailSize(char *email);
/* Make sure email address not too long. Returns size or aborts if too long. */

void edwMakeSid(char *user, char sid[EDW_ACCESS_SIZE]);
/* Convert users to sid */

#endif /* EDWLIB_H */
