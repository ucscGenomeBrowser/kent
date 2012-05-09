/* Stuff for processing comma separated lists - a little long so
 * in a separate module from jksql.c though interface is still
 * in jksql.c. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

/* The various static routines sql<Type>StaticArray are NOT thread-safe. */

#include "common.h"
#include "sqlNum.h"
#include "sqlList.h"
#include "dystring.h"
#include "hash.h"


int sqlByteArray(char *s, signed char *array, int arraySize)
/* Convert comma separated list of numbers to an array.  Pass in 
 * array an max size of array. */
{
unsigned count = 0;
for (;;)
    {
    char *e;
    if (s == NULL || s[0] == 0 || count == arraySize)
	break;
    e = strchr(s, ',');
    if (e != NULL)
	*e++ = 0;
    array[count++] = sqlSigned(s);
    s = e;
    }
return count;
}

void sqlByteStaticArray(char *s, signed char **retArray, int *retSize)
/* Convert comma separated list of numbers to an array which will be
 * overwritten next call to this function, but need not be freed. */
{
static signed char *array = NULL;
static unsigned alloc = 0;
unsigned count = 0;

for (;;)
    {
    char *e;
    if (s == NULL || s[0] == 0)
	break;
    e = strchr(s, ',');
    if (e != NULL)
	*e++ = 0;
    if (count >= alloc)
	{
	if (alloc == 0)
	    alloc = 64;
	else
	    alloc <<= 1;
	ExpandArray(array, count, alloc);
	}
    array[count++] = sqlSigned(s);
    s = e;
    }
*retSize = count;
*retArray = array;
}

void sqlByteDynamicArray(char *s, signed char **retArray, int *retSize)
/* Convert comma separated list of numbers to an dynamically allocated
 * array, which should be freeMem()'d when done. Thread-safe. */
{
signed char *array = NULL;
int count = 0;

if (s)
    {
    count = countSeparatedItems(s, ',');
    if (count > 0)
	{
	AllocArray(array, count);
	count = 0;
	for (;;)
	    {
	    array[count++] = sqlSignedInList(&s);
	    if (*s++ == 0)
		break;
	    if (*s == 0)
		break;
	    }
	}
    }
*retArray = array;
*retSize = count;
}

/*-------------------------*/

int sqlUbyteArray(char *s, unsigned char *array, int arraySize)
/* Convert comma separated list of numbers to an array.  Pass in 
 * array an max size of array. */
{
unsigned count = 0;
for (;;)
    {
    char *e;
    if (s == NULL || s[0] == 0 || count == arraySize)
	break;
    e = strchr(s, ',');
    if (e != NULL)
	*e++ = 0;
    array[count++] = sqlUnsigned(s);
    s = e;
    }
return count;
}

void sqlUbyteStaticArray(char *s, unsigned char **retArray, int *retSize)
/* Convert comma separated list of numbers to an array which will be
 * overwritten next call to this function, but need not be freed. */
{
static unsigned char *array = NULL;
static unsigned alloc = 0;
unsigned count = 0;

for (;;)
    {
    char *e;
    if (s == NULL || s[0] == 0)
	break;
    e = strchr(s, ',');
    if (e != NULL)
	*e++ = 0;
    if (count >= alloc)
	{
	if (alloc == 0)
	    alloc = 64;
	else
	    alloc <<= 1;
	ExpandArray(array, count, alloc);
	}
    array[count++] = sqlUnsigned(s);
    s = e;
    }
*retSize = count;
*retArray = array;
}

void sqlUbyteDynamicArray(char *s, unsigned char **retArray, int *retSize)
/* Convert comma separated list of numbers to an dynamically allocated
 * array, which should be freeMem()'d when done. Thread-safe. */
{
unsigned char *array = NULL;
int count = 0;

if (s)
    {
    count = countSeparatedItems(s, ',');
    if (count > 0)
	{
	AllocArray(array, count);
	count = 0;
	for (;;)
	    {
	    array[count++] = sqlUnsignedInList(&s);
	    if (*s++ == 0)
		break;
	    if (*s == 0)
		break;
	    }
	}
    }
*retArray = array;
*retSize = count;
}

/*-------------------------*/

int sqlCharArray(char *s, char *array, int arraySize)
/* Convert comma separated list of chars to an array.  Pass in 
 * array and max size of array. */
{
unsigned count = 0;
for (;;)
    {
    char *e;
    if (s == NULL || s[0] == 0 || count == arraySize)
	break;
    e = strchr(s, ',');
    if (e != NULL)
	*e++ = 0;
    array[count++] = s[0];
    s = e;
    }
return count;
}

void sqlCharStaticArray(char *s, char **retArray, int *retSize)
/* Convert comma separated list of chars to an array which will be
 * overwritten next call to this function, but need not be freed. */
{
static char *array = NULL;
static unsigned alloc = 0;
unsigned count = 0;

for (;;)
    {
    char *e;
    if (s == NULL || s[0] == 0)
	break;
    e = strchr(s, ',');
    if (e != NULL)
	*e++ = 0;
    if (count >= alloc)
	{
	if (alloc == 0)
	    alloc = 64;
	else
	    alloc <<= 1;
	ExpandArray(array, count, alloc);
	}
    array[count++] = s[0];
    s = e;
    }
*retSize = count;
*retArray = array;
}

void sqlCharDynamicArray(char *s, char **retArray, int *retSize)
/* Convert comma separated list of chars to a dynamically allocated
 * array, which should be freeMem()'d when done. Thread-safe. */
{
char *array = NULL;
int count = 0;

if (s)
    {
    count = countSeparatedItems(s, ',');
    if (count > 0)
	{
	AllocArray(array, count);
	count = 0;
	for (;;)
	    {
	    if (*s == ',')
		errAbort("Empty element in list. Each element should contain one character.");
	    array[count++] = *s++;
	    if (!(*s == 0 || *s == ','))
		{
		--s;
		char *e = strchr(s, ',');
		if (e)
		    *e = 0;
		errAbort("Invalid character: %s", s);
		}
	    if (*s++ == 0)
		break;
	    if (*s == 0)
		break;
	    }
	}
    }
*retArray = array;
*retSize = count;
}

/*-------------------------*/

int sqlShortArray(char *s, short *array, int arraySize)
/* Convert comma separated list of numbers to an array.  Pass in 
 * array an max size of array. */
{
unsigned count = 0;
for (;;)
    {
    char *e;
    if (s == NULL || s[0] == 0 || count == arraySize)
	break;
    e = strchr(s, ',');
    if (e != NULL)
	*e++ = 0;
    array[count++] = sqlSigned(s);
    s = e;
    }
return count;
}

void sqlShortStaticArray(char *s, short **retArray, int *retSize)
/* Convert comma separated list of numbers to an array which will be
 * overwritten next call to this function, but need not be freed. */
{
static short *array = NULL;
static unsigned alloc = 0;
unsigned count = 0;

for (;;)
    {
    char *e;
    if (s == NULL || s[0] == 0)
	break;
    e = strchr(s, ',');
    if (e != NULL)
	*e++ = 0;
    if (count >= alloc)
	{
	if (alloc == 0)
	    alloc = 64;
	else
	    alloc <<= 1;
	ExpandArray(array, count, alloc);
	}
    array[count++] = sqlSigned(s);
    s = e;
    }
*retSize = count;
*retArray = array;
}

void sqlShortDynamicArray(char *s, short **retArray, int *retSize)
/* Convert comma separated list of numbers to an dynamically allocated
 * array, which should be freeMem()'d when done. Thread-safe. */
{
short *array = NULL;
int count = 0;

if (s)
    {
    count = countSeparatedItems(s, ',');
    if (count > 0)
	{
	AllocArray(array, count);
	count = 0;
	for (;;)
	    {
	    array[count++] = sqlSignedInList(&s);
	    if (*s++ == 0)
		break;
	    if (*s == 0)
		break;
	    }
	}
    }
*retArray = array;
*retSize = count;
}

/*-------------------------*/

int sqlUshortArray(char *s, unsigned short *array, int arraySize)
/* Convert comma separated list of numbers to an array.  Pass in 
 * array an max size of array. */
{
unsigned count = 0;
for (;;)
    {
    char *e;
    if (s == NULL || s[0] == 0 || count == arraySize)
	break;
    e = strchr(s, ',');
    if (e != NULL)
	*e++ = 0;
    array[count++] = sqlUnsigned(s);
    s = e;
    }
return count;
}

void sqlUshortStaticArray(char *s, unsigned short **retArray, int *retSize)
/* Convert comma separated list of numbers to an array which will be
 * overwritten next call to this function, but need not be freed. */
{
static unsigned short *array = NULL;
static unsigned alloc = 0;
unsigned count = 0;

for (;;)
    {
    char *e;
    if (s == NULL || s[0] == 0)
	break;
    e = strchr(s, ',');
    if (e != NULL)
	*e++ = 0;
    if (count >= alloc)
	{
	if (alloc == 0)
	    alloc = 64;
	else
	    alloc <<= 1;
	ExpandArray(array, count, alloc);
	}
    array[count++] = sqlUnsigned(s);
    s = e;
    }
*retSize = count;
*retArray = array;
}

void sqlUshortDynamicArray(char *s, unsigned short **retArray, int *retSize)
/* Convert comma separated list of numbers to an dynamically allocated
 * array, which should be freeMem()'d when done. Thread-safe. */
{
unsigned short *array = NULL;
int count = 0;

if (s)
    {
    count = countSeparatedItems(s, ',');
    if (count > 0)
	{
	AllocArray(array, count);
	count = 0;
	for (;;)
	    {
	    array[count++] = sqlUnsignedInList(&s);
	    if (*s++ == 0)
		break;
	    if (*s == 0)
		break;
	    }
	}
    }
*retArray = array;
*retSize = count;
}

/*-------------------------*/
int sqlDoubleArray(char *s, double *array, int maxArraySize)
/* Convert comma separated list of floating point numbers to an array.  
 * Pass in array and max size of array. */
{
unsigned count = 0;
for (;;)
    {
    char *e;
    if (s == NULL || s[0] == 0 || count == maxArraySize)
	break;
    e = strchr(s, ',');
    if (e != NULL)
	*e++ = 0;
    array[count++] = atof(s);
    s = e;
    }
return count;
}


int sqlFloatArray(char *s, float *array, int maxArraySize)
/* Convert comma separated list of floating point numbers to an array.  
 * Pass in array and max size of array. */
{
unsigned count = 0;
for (;;)
    {
    char *e;
    if (s == NULL || s[0] == 0 || count == maxArraySize)
	break;
    e = strchr(s, ',');
    if (e != NULL)
	*e++ = 0;
    array[count++] = atof(s);
    s = e;
    }
return count;
}

void sqlDoubleStaticArray(char *s, double **retArray, int *retSize)
/* Convert comma separated list of numbers to an array which will be
 * overwritten next call to this function, but need not be freed. */
{
static double *array = NULL;
static unsigned alloc = 0;
unsigned count = 0;

for (;;)
    {
    char *e;
    if (s == NULL || s[0] == 0)
	break;
    e = strchr(s, ',');
    if (e != NULL)
	*e++ = 0;
    if (count >= alloc)
	{
	if (alloc == 0)
	    alloc = 64;
	else
	    alloc <<= 1;
	ExpandArray(array, count, alloc);
	}
    array[count++] = atof(s);
    s = e;
    }
*retSize = count;
*retArray = array;
}

void sqlFloatStaticArray(char *s, float **retArray, int *retSize)
/* Convert comma separated list of numbers to an array which will be
 * overwritten next call to this function, but need not be freed. */
{
static float *array = NULL;
static unsigned alloc = 0;
unsigned count = 0;

for (;;)
    {
    char *e;
    if (s == NULL || s[0] == 0)
	break;
    e = strchr(s, ',');
    if (e != NULL)
	*e++ = 0;
    if (count >= alloc)
	{
	if (alloc == 0)
	    alloc = 128;
	else
	    alloc <<= 1;
	ExpandArray(array, count, alloc);
	}
    array[count++] = atof(s);
    s = e;
    }
*retSize = count;
*retArray = array;
}

void sqlDoubleDynamicArray(char *s, double **retArray, int *retSize)
/* Convert comma separated list of numbers to an dynamically allocated
 * array, which should be freeMem()'d when done. Thread-safe.*/
{
double *array = NULL;
int count = 0;

if (s)
    {
    count = countSeparatedItems(s, ',');
    if (count > 0)
	{
	AllocArray(array, count);
	count = 0;
	for (;;)
	    {
	    array[count++] = sqlDoubleInList(&s);
	    if (*s++ == 0)
		break;
	    if (*s == 0)
		break;
	    }
	}
    }
*retArray = array;
*retSize = count;
}

void sqlFloatDynamicArray(char *s, float **retArray, int *retSize)
/* Convert comma separated list of numbers to an dynamically allocated
 * array, which should be freeMem()'d when done. Thread-safe. */
{
float *array = NULL;
int count = 0;

if (s)
    {
    count = countSeparatedItems(s, ',');
    if (count > 0)
	{
	AllocArray(array, count);
	count = 0;
	for (;;)
	    {
	    array[count++] = sqlFloatInList(&s);
	    if (*s++ == 0)
		break;
	    if (*s == 0)
		break;
	    }
	}
    }
*retArray = array;
*retSize = count;
}

/*-------------------------*/

int sqlUnsignedArray(char *s, unsigned *array, int arraySize)
/* Convert comma separated list of numbers to an array.  Pass in 
 * array and max size of array. */
{
unsigned count = 0;
for (;;)
    {
    char *e;
    if (s == NULL || s[0] == 0 || count == arraySize)
	break;
    e = strchr(s, ',');
    if (e != NULL)
	*e++ = 0;
    array[count++] = sqlUnsigned(s);
    s = e;
    }
return count;
}

void sqlUnsignedStaticArray(char *s, unsigned **retArray, int *retSize)
/* Convert comma separated list of numbers to an array which will be
 * overwritten next call to this function, but need not be freed. */
{
static unsigned *array = NULL;
static unsigned alloc = 0;
unsigned count = 0;

for (;;)
    {
    char *e;
    if (s == NULL || s[0] == 0)
	break;
    e = strchr(s, ',');
    if (e != NULL)
	*e++ = 0;
    if (count >= alloc)
	{
	if (alloc == 0)
	    alloc = 64;
	else
	    alloc <<= 1;
	ExpandArray(array, count, alloc);
	}
    array[count++] = sqlUnsigned(s);
    s = e;
    }
*retSize = count;
*retArray = array;
}

void sqlUnsignedDynamicArray(char *s, unsigned **retArray, int *retSize)
/* Convert comma separated list of numbers to an dynamically allocated
 * array, which should be freeMem()'d when done. Thread-safe. */
{
unsigned *array = NULL;
int count = 0;

if (s)
    {
    count = countSeparatedItems(s, ',');
    if (count > 0)
	{
	AllocArray(array, count);
	count = 0;
	for (;;)
	    {
	    array[count++] = sqlUnsignedInList(&s);
	    if (*s++ == 0)
		break;
	    if (*s == 0)
		break;
	    }
	}
    }
*retArray = array;
*retSize = count;
}

/*-------------------------*/

int sqlSignedArray(char *s, int *array, int arraySize)
/* Convert comma separated list of numbers to an array.  Pass in 
 * array an max size of array. */
{
int count = 0;
for (;;)
    {
    char *e;
    if (s == NULL || s[0] == 0 || count == arraySize)
	break;
    e = strchr(s, ',');
    if (e != NULL)
	*e++ = 0;
    array[count++] = sqlSigned(s);
    s = e;
    }
return count;
}

void sqlSignedStaticArray(char *s, int **retArray, int *retSize)
/* Convert comma separated list of numbers to an array which will be
 * overwritten next call to this function, but need not be freed. */
{
static int *array = NULL;
static int alloc = 0;
int count = 0;

for (;;)
    {
    char *e;
    if (s == NULL || s[0] == 0)
	break;
    e = strchr(s, ',');
    if (e != NULL)
	*e++ = 0;
    if (count >= alloc)
	{
	if (alloc == 0)
	    alloc = 64;
	else
	    alloc <<= 1;
	ExpandArray(array, count, alloc);
	}
    array[count++] = sqlSigned(s);
    s = e;
    }
*retSize = count;
*retArray = array;
}

void sqlSignedDynamicArray(char *s, int **retArray, int *retSize)
/* Convert comma separated list of numbers to an dynamically allocated
 * array, which should be freeMem()'d when done. Thread-safe. */
{
int *array = NULL;
int count = 0;

if (s)
    {
    count = countSeparatedItems(s, ',');
    if (count > 0)
	{
	AllocArray(array, count);
	count = 0;
	for (;;)
	    {
	    array[count++] = sqlSignedInList(&s);
	    if (*s++ == 0)
		break;
	    if (*s == 0)
		break;
	    }
	}
    }
*retArray = array;
*retSize = count;
}


/*-------------------------*/

int sqlLongLongArray(char *s, long long *array, int arraySize)
/* Convert comma separated list of numbers to an array.  Pass in 
 * array and max size of array. */
{
unsigned count = 0;
for (;;)
    {
    char *e;
    if (s == NULL || s[0] == 0 || count == arraySize)
	break;
    e = strchr(s, ',');
    if (e != NULL)
	*e++ = 0;
    array[count++] = sqlLongLong(s);
    s = e;
    }
return count;
}

void sqlLongLongStaticArray(char *s, long long **retArray, int *retSize)
/* Convert comma separated list of numbers to an array which will be
 * overwritten next call to this function, but need not be freed. */
{
static long long *array = NULL;
static unsigned alloc = 0;
unsigned count = 0;

for (;;)
    {
    char *e;
    if (s == NULL || s[0] == 0)
	break;
    e = strchr(s, ',');
    if (e != NULL)
	*e++ = 0;
    if (count >= alloc)
	{
	if (alloc == 0)
	    alloc = 64;
	else
	    alloc <<= 1;
	ExpandArray(array, count, alloc);
	}
    array[count++] = sqlLongLong(s);
    s = e;
    }
*retSize = count;
*retArray = array;
}

void sqlLongLongDynamicArray(char *s, long long **retArray, int *retSize)
/* Convert comma separated list of numbers to an dynamically allocated
 * array, which should be freeMem()'d when done. Thread-safe. */
{
long long *array = NULL;
int count = 0;

if (s)
    {
    count = countSeparatedItems(s, ',');
    if (count > 0)
	{
	AllocArray(array, count);
	count = 0;
	for (;;)
	    {
	    array[count++] = sqlLongLongInList(&s);
	    if (*s++ == 0)
		break;
	    if (*s == 0)
		break;
	    }
	}
    }
*retArray = array;
*retSize = count;
}

/*-------------------------*/


int sqlStringArray(char *s, char **array, int maxArraySize)
/* Convert comma separated list of strings to an array.  Pass in 
 * array and max size of array.  Returns actual size*/
{
int count = 0;
for (;;)
    {
    char *e;
    if (s == NULL || s[0] == 0 || count == maxArraySize)
	break;
    e = strchr(s, ',');
    if (e != NULL)
	*e++ = 0;
    array[count++] = s;
    s = e;
    }
return count;
}

void sqlStringStaticArray(char *s, char  ***retArray, int *retSize)
/* Convert comma separated list of strings to an array which will be
 * overwritten next call to this function,  but need not be freed. */
{
static char **array = NULL;
static int alloc = 0;
int count = 0;

for (;;)
    {
    char *e;
    if (s == NULL || s[0] == 0)
	break;
    e = strchr(s, ',');
    if (e != NULL)
	*e++ = 0;
    if (count >= alloc)
	{
	if (alloc == 0)
	    alloc = 64;
	else
	    alloc <<= 1;
	ExpandArray(array, count, alloc);
	}
    array[count++] = s;
    s = e;
    }
*retSize = count;
*retArray = array;
}

void sqlStringDynamicArray(char *s, char ***retArray, int *retSize)
/* Convert comma separated list of strings to an dynamically allocated
 * array, which should be freeMem()'d when done. As a speed option all
 * of the elements in the array are needMem()'d at the same time. This 
 * means that all the entries are free()'d by calling freeMem() on the
 * first element. For example:
 * sqlStringDynamicArray(s, &retArray, &retSize);
 * DoSomeFunction(retArray, retSize);
 * freeMem(retArray[0]);
 * freeMem(retArray);
 * Thread-safe. */
{
char **array = NULL;
int count = 0;
if (s)
    {
    count = countSeparatedItems(s, ',');
    if (count > 0)
	{
	AllocArray(array, count);
	count = 0;
	s = cloneString(s);
	for (;;)
	    {
	    char *e;
	    if (s == NULL || s[0] == 0)
		break;
	    e = strchr(s, ',');
	    if (e != NULL)
		*e++ = 0;
	    array[count++] = s;
	    s = e;
	    }
	}
    }
*retArray = array;
*retSize = count;
}

char *sqlDoubleArrayToString( double *array, int arraySize)
{
int i;
struct dyString *string = newDyString(256);
char *toRet = NULL;
for( i = 0 ; i < arraySize; i++ )
    {
    dyStringPrintf(string, "%f,", array[i]);
    }
toRet = cloneString(string->string);
dyStringFree(&string);
return toRet;
}

char *sqlFloatArrayToString( float *array, int arraySize)
{
int i;
struct dyString *string = newDyString(256);
char *toRet = NULL;
for( i = 0 ; i < arraySize; i++ )
    {
    dyStringPrintf(string, "%f,", array[i]);
    }
toRet = cloneString(string->string);
dyStringFree(&string);
return toRet;
}

char *sqlUnsignedArrayToString( unsigned *array, int arraySize)
{
int i;
struct dyString *string = newDyString(256);
char *toRet = NULL;
for( i = 0 ; i < arraySize; i++ )
    {
    dyStringPrintf(string, "%u,", array[i]);
    }
toRet = cloneString(string->string);
dyStringFree(&string);
return toRet;
}

char *sqlSignedArrayToString( int *array, int arraySize)
{
int i;
struct dyString *string = newDyString(256);
char *toRet = NULL;
for( i = 0 ; i < arraySize; i++ )
    {
    dyStringPrintf(string, "%d,", array[i]);
    }
toRet = cloneString(string->string);
dyStringFree(&string);
return toRet;
}

char *sqlShortArrayToString( short *array, int arraySize)
{
int i;
struct dyString *string = newDyString(256);
char *toRet = NULL;
for( i = 0 ; i < arraySize; i++ )
    {
    dyStringPrintf(string, "%d,", array[i]);
    }
toRet = cloneString(string->string);
dyStringFree(&string);
return toRet;
}

char *sqlUshortArrayToString( unsigned short *array, int arraySize)
{
int i;
struct dyString *string = newDyString(256);
char *toRet = NULL;
for( i = 0 ; i < arraySize; i++ )
    {
    dyStringPrintf(string, "%u,", array[i]);
    }
toRet = cloneString(string->string);
dyStringFree(&string);
return toRet;
}

char *sqlByteArrayToString( signed char *array, int arraySize)
{
int i;
struct dyString *string = newDyString(256);
char *toRet = NULL;
for( i = 0 ; i < arraySize; i++ )
    {
    dyStringPrintf(string, "%d,", array[i]);
    }
toRet = cloneString(string->string);
dyStringFree(&string);
return toRet;
}

char *sqlUbyteArrayToString( unsigned char *array, int arraySize)
{
int i;
struct dyString *string = newDyString(256);
char *toRet = NULL;
for( i = 0 ; i < arraySize; i++ )
    {
    dyStringPrintf(string, "%u,", array[i]);
    }
toRet = cloneString(string->string);
dyStringFree(&string);
return toRet;
}

char *sqlCharArrayToString( char *array, int arraySize)
{
int i;
struct dyString *string = newDyString(256);
char *toRet = NULL;
for( i = 0 ; i < arraySize; i++ )
    {
    dyStringPrintf(string, "%c,", array[i]);
    }
toRet = cloneString(string->string);
dyStringFree(&string);
return toRet;
}

char *sqlLongLongArrayToString( long long *array, int arraySize)
{
int i;
struct dyString *string = newDyString(256);
char *toRet = NULL;
for( i = 0 ; i < arraySize; i++ )
    {
    dyStringPrintf(string, "%lld,", array[i]);
    }
toRet = cloneString(string->string);
dyStringFree(&string);
return toRet;
}

char *sqlStringArrayToString( char **array, int arraySize)
{
int i;
struct dyString *string = newDyString(256);
char *toRet = NULL;
for( i = 0 ; i < arraySize; i++ )
    {
    dyStringPrintf(string, "%s,", array[i]);
    }
toRet = cloneString(string->string);
dyStringFree(&string);
return toRet;
}


/* -------------- */



void sqlStringFreeDynamicArray(char ***pArray)
/* Free up a dynamic array (ends up freeing array and first string on it.) */
{
char **array;
if ((array = *pArray) != NULL)
    {
    freeMem(array[0]);
    freez(pArray);
    }
}

int sqlUnsignedComma(char **pS)
/* Return signed number at *pS.  Advance *pS past comma at end */
{
char *s = *pS;
char *e = strchr(s, ',');
unsigned ret;

*e++ = 0;
*pS = e;
ret = sqlUnsigned(s);
return ret;
}


int sqlSignedComma(char **pS)
/* Return signed number at *pS.  Advance *pS past comma at end */
{
char *s = *pS;
char *e = strchr(s, ',');
int ret;

*e++ = 0;
*pS = e;
ret = sqlSigned(s);
return ret;
}

char sqlCharComma(char **pS)
/* Return char at *pS.  Advance *pS past comma after char */
{
char *s = *pS;
char *e = strchr(s, ',');
int ret;

*e++ = 0;
*pS = e;
ret = s[0];
return ret;
}

long long sqlLongLongComma(char **pS)
/* Return offset (often 64 bits) at *pS.  Advance *pS past comma at 
 * end */
{
char *s = *pS;
char *e = strchr(s, ',');
long long ret;

*e++ = 0;
*pS = e;
ret = sqlLongLong(s);
return ret;
}

float sqlFloatComma(char **pS)
/* Return signed number at *pS.  Advance *pS past comma at end */
{
char *s = *pS;
char *e = strchr(s, ',');
float ret;

*e++ = 0;
*pS = e;
ret = atof(s);
return ret;
}

double sqlDoubleComma(char **pS)
/* Return signed number at *pS.  Advance *pS past comma at end */
{
char *s = *pS;
char *e = strchr(s, ',');
double ret;

*e++ = 0;
*pS = e;
ret = atof(s);
return ret;
}


static char *findStringEnd(char *start, char endC)
/* Return end of string. */
{
char c;
char *s = start;

for (;;)
    {
    c = *s;
    if (c == endC)
	return s;
    else if (c == 0)
	errAbort("Unterminated string");
    ++s;
    }
}

static char *sqlGetOptQuoteString(char **pS)
/* Return string at *pS.  (Either quoted or not.)  Advance *pS. */
{
char *s = *pS;
char *e;
char c = *s;

if (c  == '"' || c == '\'')
    {
    s += 1;
    e = findStringEnd(s, c);
    *e++ = 0;
    if (*e++ != ',')
	errAbort("Expecting comma after string");
    }
else
    {
    e = strchr(s, ',');
    *e++ = 0;
    }
*pS = e;
return s;
}

char *sqlStringComma(char **pS)
/* Return string at *pS.  (Either quoted or not.)  Advance *pS. */
{
return cloneString(sqlGetOptQuoteString(pS));
}

void sqlFixedStringComma(char **pS, char *buf, int bufSize)
/* Copy string at *pS to buf.  Advance *pS. */
{
strncpy(buf, sqlGetOptQuoteString(pS), bufSize);
}

char *sqlEatChar(char *s, char c)
/* Make sure next character is 'c'.  Return past next char */
{
if (*s++ != c)
    errAbort("Expecting %c got %c (%d) in database", c, s[-1], s[-1]);
return s;
}

static struct hash *buildSymHash(char **values, boolean isEnum)
/* build a hash of values for either enum or set symbolic column */
{
struct hash *valHash = hashNew(0);
unsigned setVal = 1; /* not used for enum */
int iVal;
for (iVal = 0; values[iVal] != NULL; iVal++)
    {
    if (isEnum)
        hashAddInt(valHash, values[iVal], iVal);
    else
        {
        hashAddInt(valHash, values[iVal], setVal);
        setVal = setVal << 1;
        }
    }
return valHash;
}

unsigned sqlEnumParse(char *valStr, char **values, struct hash **valHashPtr)
/* parse an enumerated column value */
{
if (*valHashPtr == NULL)
    *valHashPtr = buildSymHash(values, TRUE);
return hashIntVal(*valHashPtr, valStr);
}

unsigned sqlEnumComma(char **pS, char **values, struct hash **valHashPtr)
/* Return enum at *pS.  (Either quoted or not.)  Advance *pS. */
{
return sqlEnumParse(sqlGetOptQuoteString(pS), values, valHashPtr);
}

void sqlEnumPrint(FILE *f, unsigned value, char **values)
/* print an enumerated column value */
{
fputs(values[value], f);
}

unsigned sqlSetParse(char *valStr, char **values, struct hash **valHashPtr)
/* parse a set column value */
{
if (*valHashPtr == NULL)
    *valHashPtr = buildSymHash(values, FALSE);
/* parse comma separated string */
unsigned value = 0;
char *val = strtok(valStr, ",");
while (val != NULL)
    {
    value |= hashIntVal(*valHashPtr, val);
    val = strtok(NULL, ",");
    }

return value;
}

unsigned sqlSetComma(char **pS, char **values, struct hash **valHashPtr)
/* Return set at *pS.  (Either quoted or not.)  Advance *pS. */
{
return sqlSetParse(sqlGetOptQuoteString(pS), values, valHashPtr);
}

void sqlSetPrint(FILE *f, unsigned value, char **values)
/* print a set column value */
{
int iVal;
unsigned curVal = 1;
int cnt = 0;
for (iVal = 0; values[iVal] != NULL; iVal++, curVal = curVal << 1)
    {
    if (curVal & value)
        {
        if (cnt > 0)
            fputc(',', f);
        fputs(values[iVal], f);
        cnt++;
        }
    }
}
