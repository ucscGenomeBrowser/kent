/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* jksql.h - Stuff to manage interface with SQL database. 
 *
 * To use - first open a connection, then pass a SQL query to 
 * sqlGetResult, then use sqlNextRow to examine result row by
 * row. The returned row is just an array of strings.  Use
 * sqlUnsigned, sqlSigned, and atof to convert numeric results
 * to normal form. 
 *
 * These routines will all print an error message and abort if 
 * there's a problem, cleaning up open connections, etc. on abort 
 * (or on program exit).  Do a pushAbortHandler if you want to 
 * catch the aborts.  The error messages from bad SQL syntax
 * are actually pretty good (they're just passed on from
 * mySQL). */

int sqlCountColumns(struct sqlResult *sr);

struct sqlConnection *sqlConnect(char *database);
/* Connect to database. */

void sqlDisconnect(struct sqlConnection **pSc);
/* Close down connection. */

struct sqlConnCache *sqlNewConnCache(char *database);
/* Return a new connection cache. (Useful if going to be
 * doing lots of different queries in different routines
 * to same database - reduces connection overhead.) */

void sqlFreeConnCache(struct sqlConnCache **pCache);
/* Dispose of a connection cache. */

struct sqlConnection *sqlAllocConnection(struct sqlConnCache *cache);
/* Allocate a cached connection. */

void sqlFreeConnection(struct sqlConnCache *cache,struct sqlConnection **pConn);
/* Free up a cached connection. */

void sqlUpdate(struct sqlConnection *conn, char *query);
/* Tell database to do something that produces no results table. */

boolean sqlExists(struct sqlConnection *conn, char *query);
/* Query database and return TRUE if it had a non-empty result. */

struct sqlResult *sqlGetResult(struct sqlConnection *sc, char *query);
/* Query database.
 * Returns NULL if result was empty.  Otherwise returns a structure
 * that you can do sqlRow() on. */

struct sqlResult *sqlMustGetResult(struct sqlConnection *sc, char *query);
/* Query database. If result empty squawk and die. */

void sqlFreeResult(struct sqlResult **pRes);
/* Free up a result. */

char *sqlQuickQuery(struct sqlConnection *sc, char *query, char *buf, int bufSize);
/* Does query and returns first field in first row.  Meant
 * for cases where you are just looking up one small thing.  
 * Returns NULL if query comes up empty. */

int sqlQuickNum(struct sqlConnection *conn, char *query);
/* Get numerical result from simple query */

char **sqlNextRow(struct sqlResult *sr);
/* Fetch next row from result.  If you need to save these strings
 * past the next call to sqlNextRow you must copy them elsewhere.
 * It is ok to write to the strings - replacing tabs with zeroes
 * for instance.  You can call this with a NULL sqlResult.  It
 * will then return a NULL row. */

int sqlTableSize(struct sqlConnection *conn, char *table);
/* Find number of rows in table. */

unsigned sqlUnsigned(char *s);
/* Convert series of digits to unsigned integer about
 * twice as fast as atoi (by not having to skip white 
 * space or stop except at the null byte.) */

int sqlSigned(char *s);
/* Convert string to signed integer.  Unlike atol assumes 
 * all of string is number. */

int sqlFloatArray(char *s, float *array, int maxArraySize);
int sqlUnsignedArray(char *s, unsigned *array, int maxArraySize);
int sqlSignedArray(char *s, int *array, int maxArraySize);
int sqlShortArray(char *s, short *array, int arraySize);
int sqlUshortArray(char *s, unsigned short *array, int arraySize);
int sqlByteArray(char *s, signed char *array, int arraySize);
int sqlUbyteArray(char *s, unsigned char *array, int arraySize);
/* Convert comma separated list of numbers to an array.  Pass in 
 * array and max size of array.  Returns actual array size.*/

void sqlFloatStaticArray(char *s, float **retArray, int *retSize);
void sqlUnsignedStaticArray(char *s, unsigned **retArray, int *retSize);
void sqlSignedStaticArray(char *s, int **retArray, int *retSize);
void sqlShortStaticArray(char *s, short **retArray, int *retSize);
void sqlUshortStaticArray(char *s, unsigned short **retArray, int *retSize);
void sqlByteStaticArray(char *s, signed char **retArray, int *retSize);
void sqlUbyteStaticArray(char *s, unsigned char **retArray, int *retSize);
/* Convert comma separated list of numbers to an array which will be
 * overwritten next call to this function or to sqlXxxxxxDynamicArray,
 * but need not be freed. */

void sqlFloatDynamicArray(char *s, float **retArray, int *retSize);
void sqlUnsignedDynamicArray(char *s, unsigned **retArray, int *retSize);
void sqlSignedDynamicArray(char *s, int **retArray, int *retSize);
void sqlShortDynamicArray(char *s, short **retArray, int *retSize);
void sqlUshortDynamicArray(char *s, unsigned short **retArray, int *retSize);
void sqlByteDynamicArray(char *s, signed char **retArray, int *retSize);
void sqlUbyteDynamicArray(char *s, unsigned char **retArray, int *retSize);
/* Convert comma separated list of numbers to an dynamically allocated
 * array, which should be freeMem()'d when done. */


int sqlStringArray(char *s, char **array, int maxArraySize);
/* Convert comma separated list of strings to an array.  Pass in 
 * array and max size of array.  Returns actual size.  This will
 * only persist as long as s persists.... Use sqlStringDynamicArray
 * if calling repeatedly. */

void sqlStringStaticArray(char *s, char  ***retArray, int *retSize);
/* Convert comma separated list of strings to an array which will be
 * overwritten next call to this function or to sqlUnsignedDynamicArray,
 * but need not be freed. */

void sqlStringDynamicArray(char *s, char ***retArray, int *retSize);
/* Convert comma separated list of strings to an dynamically allocated
 * array, which should be freeMem()'d when done. */

void sqlStringFreeDynamicArray(char ***pArray);
/* Free up a dynamic array (ends up freeing array and first string on it.) */

int sqlUnsignedComma(char **pS);
/* Return signed number at *pS.  Advance *pS past comma at end.
 * This function is used by the system that automatically loads
 * structured object from longblobs. */

int sqlSignedComma(char **pS);
/* Return signed number at *pS.  Advance *pS past comma at end */

float sqlFloatComma(char **pS);
/* Return floating point number at *pS.  Advance *pS past comma at end */

char *sqlStringComma(char **pS);
/* Return string at *pS.  (Either quoted or not.)  Advance *pS. */

void sqlFixedStringComma(char **pS, char *buf, int bufSize);
/* Copy string at *pS to buf.  Advance *pS. */

char *sqlEatChar(char *s, char c);
/* Make sure next character is 'c'.  Return past next char */

void sqlVaWarn(struct sqlConnection *sc, char *format, va_list args);
/* Error message handler. */

void sqlWarn(struct sqlConnection *sc, char *format, ...);
/* Printf formatted error message that adds on sql 
 * error message. */

void sqlAbort(struct sqlConnection  *sc, char *format, ...);
/* Printf formatted error message that adds on sql 
 * error message and abort. */

void sqlCleanupAll();
/* Cleanup all open connections and resources. */
