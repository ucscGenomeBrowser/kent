/* Stuff for processing comma separated lists .*/
#ifndef SQLLIST_H
#define SQLLIST_H

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


#endif SQLLIST_H

