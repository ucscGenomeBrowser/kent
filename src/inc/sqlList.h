/* Stuff for processing comma separated lists .
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#ifndef SQLLIST_H
#define SQLLIST_H
struct hash;

int sqlDoubleArray(char *s, double *array, int maxArraySize);
double sqlSumDoublesCommaSep(char *s);
/* Return sum of double values in a comma-separated list */

int sqlFloatArray(char *s, float *array, int maxArraySize);
int sqlUnsignedArray(char *s, unsigned *array, int maxArraySize);
int sqlSignedArray(char *s, int *array, int maxArraySize);
int sqlShortArray(char *s, short *array, int arraySize);
int sqlUshortArray(char *s, unsigned short *array, int arraySize);
int sqlByteArray(char *s, signed char *array, int arraySize);
int sqlUbyteArray(char *s, unsigned char *array, int arraySize);
int sqlCharArray(char *s, char *array, int arraySize);
int sqlLongLongArray(char *s, long long *array, int arraySize);
/* Convert comma separated list of numbers to an array.  Pass in 
 * array and max size of array.  Returns actual array size.*/

void sqlDoubleStaticArray(char *s, double **retArray, int *retSize);
void sqlFloatStaticArray(char *s, float **retArray, int *retSize);
void sqlUnsignedStaticArray(char *s, unsigned **retArray, int *retSize);
void sqlSignedStaticArray(char *s, int **retArray, int *retSize);
void sqlShortStaticArray(char *s, short **retArray, int *retSize);
void sqlUshortStaticArray(char *s, unsigned short **retArray, int *retSize);
void sqlByteStaticArray(char *s, signed char **retArray, int *retSize);
void sqlUbyteStaticArray(char *s, unsigned char **retArray, int *retSize);
void sqlCharStaticArray(char *s, char **retArray, int *retSize);
void sqlLongLongStaticArray(char *s, long long **array, int *retSize);
/* Convert comma separated list of numbers to an array which will be
 * overwritten next call to this function or to sqlXxxxxxDynamicArray,
 * but need not be freed. */

void sqlDoubleDynamicArray(char *s, double **retArray, int *retSize);
void sqlFloatDynamicArray(char *s, float **retArray, int *retSize);
void sqlUnsignedDynamicArray(char *s, unsigned **retArray, int *retSize);
void sqlSignedDynamicArray(char *s, int **retArray, int *retSize);
void sqlShortDynamicArray(char *s, short **retArray, int *retSize);
void sqlUshortDynamicArray(char *s, unsigned short **retArray, int *retSize);
void sqlByteDynamicArray(char *s, signed char **retArray, int *retSize);
void sqlUbyteDynamicArray(char *s, unsigned char **retArray, int *retSize);
void sqlCharDynamicArray(char *s, char **retArray, int *retSize);
void sqlLongLongDynamicArray(char *s, long long **retArray, int *retSize);
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

char *sqlDoubleArrayToString( double *array, int arraySize);
char *sqlFloatArrayToString( float *array, int arraySize);
char *sqlUnsignedArrayToString( unsigned *array, int arraySize);
char *sqlSignedArrayToString( int *array, int arraySize);
char *sqlShortArrayToString( short *array, int arraySize);
char *sqlUshortArrayToString( unsigned short *array, int arraySize);
char *sqlByteArrayToString( signed char *array, int arraySize);
char *sqlUbyteArrayToString( unsigned char *array, int arraySize);
char *sqlCharArrayToString( char *array, int arraySize);
char *sqlLongLongArrayToString( long long *array, int arraySize);
char *sqlStringArrayToString( char **array, int arraySize);
/* Convert arrays into comma separated strings. The char *'s returned
 * should be freeMem()'d when done */

char *sqlEscapeString(const char *orig);
/* Prepares string for inclusion in a SQL statement . Remember to free
 * returned string.  returned string contains strlen(length)*2+1 as many bytes
 * as orig because in worst case every character has to be escaped.
 * Example 1: The Gene's Name -> The Gene''s Name
 * Example 2: he said "order and orient" -> he said ""order and orient"" */

char *sqlEscapeString2(char *to, const char* from);
/* Prepares a string for inclusion in a sql statement.  Output string
 * must be 2*strlen(from)+1 */

int sqlUnsignedComma(char **pS);
/* Return signed number at *pS.  Advance *pS past comma at end.
 * This function is used by the system that automatically loads
 * structured object from longblobs. */

int sqlSignedComma(char **pS);
/* Return signed number at *pS.  Advance *pS past comma at end */

char sqlCharComma(char **pS);
/* Return char at *pS.  Advance *pS past comma after char */

long long sqlLongLongComma(char **pS);
/* Return long long number at *pS.  Advance *pS past comma at end */

double sqlDoubleComma(char **pS);
/* Return double floating number at *pS.  Advance *pS past comma at end */

float sqlFloatComma(char **pS);
/* Return floating point number at *pS.  Advance *pS past comma at end */

char *sqlStringComma(char **pS);
/* Return string at *pS.  (Either quoted or not.)  Advance *pS. */

void sqlFixedStringComma(char **pS, char *buf, int bufSize);
/* Copy string at *pS to buf.  Advance *pS. */

char *sqlEatChar(char *s, char c);
/* Make sure next character is 'c'.  Return past next char */

unsigned sqlEnumParse(char *valStr, char **values, struct hash **valHashPtr);
/* parse an enumerated column value */

unsigned sqlEnumComma(char **pS, char **values, struct hash **valHashPtr);
/* Return enum at *pS.  (Either quoted or not.)  Advance *pS. */

void sqlEnumPrint(FILE *f, unsigned value, char **values);
/* print an enumerated column value */

unsigned sqlSetParse(char *valStr, char **values, struct hash **valHashPtr);
/* parse a set column value */

unsigned sqlSetComma(char **pS, char **values, struct hash **valHashPtr);
/* Return set at *pS.  (Either quoted or not.)  Advance *pS. */

void sqlSetPrint(FILE *f, unsigned value, char **values);
/* print a set column value */

#endif /* SQLLIST_H */

