/* bdwLib - routines shared by various bigDataWarehouse programs.    See also bigDataWarehouse
 * module for tables and routines to access structs built on tables. */

#ifndef BDWLIB_H
#define BDWLIB_H

#ifndef JKSQL_H
#include "jksql.h"
#endif

#define BDW_ACCESS_SIZE 65    /* Size of our access key - currently base64 encoded SHA384 with 
                               * NULL terminator */

extern char *bdwDatabase;   /* Name of database we connect to. */
extern char *bdwRootDir;    /* Name of root directory for our files, including trailing '/' */

void bdwMakeAccess(char *user, char *password, char access[BDW_ACCESS_SIZE]);
/* Convert user + password + salt to an access code */

boolean bdwCheckAccess(struct sqlConnection *conn, char *user, char *password, 
    char retSid[BDW_ACCESS_SIZE]);
/* Make sure user exists and password checks out. */

void bdwMustHaveAccess(struct sqlConnection *conn, char *user, char *password,
    char retSid[BDW_ACCESS_SIZE]);
/* Check user has access and abort with an error message if not. */

int bdwCheckEmailSize(char *email);
/* Make sure email address not too long. Returns size or aborts if too long. */

void bdwMakeSid(char *user, char sid[BDW_ACCESS_SIZE]);
/* Convert users to sid */

#endif /* BDWLIB_H */
