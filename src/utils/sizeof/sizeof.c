/*
 *	sizeof - show size of various C types for reference
 *	$Id: sizeof.c,v 1.3 2005/12/02 19:21:42 hiram Exp $
 */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include <stdio.h>

int main()
{
unsigned int byteOrder = 0x12345678;
unsigned char *cp = (unsigned char *) &byteOrder;

printf("     type   bytes    bits\n");
printf("     char\t%d\t%d\n", (int)sizeof(char), 8*(int)sizeof(char));
printf("unsigned char\t%d\t%d\n", (int)sizeof(unsigned char),
	8*(int)sizeof(unsigned char));
printf("short int\t%d\t%d\n", (int)sizeof(short int), 8*(int)sizeof(short int));
printf("u short int\t%d\t%d\n", (int)sizeof(unsigned short int),
	8*(int)sizeof(unsigned short int));
printf("      int\t%d\t%d\n", (int)sizeof(int), 8*(int)sizeof(int));
printf(" unsigned\t%d\t%d\n", (int)sizeof(unsigned), 8*(int)sizeof(unsigned));
printf("     long\t%d\t%d\n", (int)sizeof(long), 8*(int)sizeof(long));
printf("unsigned long\t%d\t%d\n", (int)sizeof(unsigned long),
	8*(int)sizeof(unsigned long));
printf("long long\t%d\t%d\n", (int)sizeof(long long), 8*(int)sizeof(long long));
printf("u long long\t%d\t%d\n", (int)sizeof(unsigned long long),
	8*(int)sizeof(unsigned long long));
printf("   size_t\t%d\t%d\n", (int)sizeof(size_t), 8*(int)sizeof(size_t));
printf("   void *\t%d\t%d\n", (int)sizeof(void *), 8*(int)sizeof(void *));
printf("    float\t%d\t%d\n", (int)sizeof(float), 8*(int)sizeof(float));
printf("   double\t%d\t%d\n", (int)sizeof(double), 8*(int)sizeof(double));
printf("long double\t%d\t%d\n", (int)sizeof(long double),
	8*(int)sizeof(long double));
if (*cp == (char) 0x12)
    printf ("BIG ENDIAN machine detected\n");
else if (*cp == (char) 0x78)
    printf ("LITTLE ENDIAN machine detected\n");
else
    printf ("WARNING: can not determine BIG or LITTLE ENDIAN machine\n");
printf("byte order: normal order: 0x12345678 in memory: 0x");
int i;
for (i = 0; i < sizeof(byteOrder); ++i)
    {
    printf("%02x", *cp);
    ++cp;
    }
printf("\n");
return(0);
}
