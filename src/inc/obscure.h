/* Obscure.h  - stuff that's relatively rarely used
 * but still handy. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */


long incCounterFile(char *fileName);
/* Increment a 32 bit value on disk. */

int digitsBaseTwo(unsigned long x);
/* Return base two # of digits. */

int digitsBaseTen(int x);
/* Return number of digits base 10. */

void sprintLongWithCommas(char *s, long l);
/* Print out a long number with commas a thousands, millions, etc. */

void printLongWithCommas(FILE *f, long l);
/* Print out a long number with commas a thousands, millions, etc. */

void readInGulp(char *fileName, char **retBuf, size_t *retSize);
/* Read whole file in one big gulp. */

void readAllWords(char *fileName, char ***retWords, int *retWordCount, char **retBuf);
/* Read in whole file and break it into words. You need to freeMem both
 * *retWordCount and *retBuf when done. */

struct slName *readAllLines(char *fileName);
/* Read all lines of file into a list.  (Removes trailing carriage return.) */

void copyFile(char *source, char *dest);
/* Copy file from source to dest. */

void cpFile(int s, int d);
/* Copy from source file to dest until reach end of file. */

void *intToPt(int i);
/* Convert integer to pointer. Use when really want to store an
 * int in a pointer field. */

int ptToInt(void *pt);
/* Convert pointer to integer.  Use when really want to store an
 * int in a pointer field. */

boolean parseQuotedString( char *in, char *out, char **retNext);
/* Read quoted string from in (which should begin with first quote).
 * Write unquoted string to out, which may be the same as in.
 * Return pointer to character past end of string in *retNext. 
 * Return FALSE if can't find end. */

struct hash *hashVarLine(char *line, int lineIx);
/* Return a symbol table from a line of form:
 *   var1=val1 var2='quoted val2' var3="another val" */

void shuffleArrayOfPointers(void *pointerArray, int arraySize, 
	int shuffleCount);
/* Shuffle array of pointers of given size given number of times. */

void shuffleList(void *pList, int shuffleCount);
/* Randomize order of slList.  Usage:
 *     randomizeList(&list)
 * where list is a pointer to a structure that
 * begins with a next field. */
