/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* Obscure.h  - stuff that's relatively rarely used
 * but still handy. */

long incCounterFile(char *fileName);
/* Increment a 32 bit value on disk. */

int digitsBaseTwo(unsigned long x);
/* Return base two # of digits. */

int digitsBaseTen(int x);
/* Return number of digits base 10. */

void readInGulp(char *fileName, char **retBuf, size_t *retSize);
/* Read whole file in one big gulp. */

void readAllWords(char *fileName, char ***retWords, int *retWordCount, char **retBuf);
/* Read in whole file and break it into words. You need to freeMem both
 * *retWordCount and *retBuf when done. */

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
