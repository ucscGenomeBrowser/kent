/* bdwLib - routines shared by various bigDataWarehouse programs.    See also bigDataWarehouse
 * module for tables and routines to access structs built on tables. */

#ifndef BDWLIB_H
#define BDWLIB_H

#ifndef JKSQL_H
#include "jksql.h"
#endif

#define BDW_SHA_SIZE 64    /* Size of our SHA hash - 64 bytes or 512 bits*/

extern char *bdwDatabase;   /* Name of database we connect to. */

void bdwMakeAccess(char *user, char *password, unsigned char access[BDW_SHA_SIZE]);
/* Convert user + password + salt to an access code */

boolean bdwCheckAccess(struct sqlConnection *conn, char *user, char *password, 
    unsigned char retSid[BDW_SHA_SIZE]);
/* Make sure user exists and password checks out. */

void bdwMustHaveAccess(struct sqlConnection *conn, char *user, char *password,
    unsigned char retSid[BDW_SHA_SIZE]);
/* Check user has access and abort with an error message if not. */

int bdwCheckEmailSize(char *email);
/* Make sure email address not too long. Returns size or aborts if too long. */

void bdwMakeSid(char *user, unsigned char sid[BDW_SHA_SIZE]);
/* Convert users to sid */

#endif /* BDWLIB_H */
