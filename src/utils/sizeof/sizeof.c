/*
 *	sizeof - show size of various C types for reference
 *	$Id: sizeof.c,v 1.1 2005/12/02 19:13:55 hiram Exp $
 */
#include <stdio.h>

int main()
{
printf("     type   bytes    bits\n");
printf("     char\t%d\t%d\n", sizeof(char), 8*sizeof(char));
printf("      int\t%d\t%d\n", sizeof(int), 8*sizeof(int));
printf("     long\t%d\t%d\n", sizeof(long), 8*sizeof(long));
printf("long long\t%d\t%d\n", sizeof(long long), 8*sizeof(long long));
printf("   size_t\t%d\t%d\n", sizeof(size_t), 8*sizeof(size_t));
printf("   void *\t%d\t%d\n", sizeof(void *), 8*sizeof(void *));
printf(" unsigned\t%d\t%d\n", sizeof(unsigned), 8*sizeof(unsigned));
printf("long unsigned\t%d\t%d\n", sizeof(long unsigned), 8*sizeof(long unsigned));
printf("    float\t%d\t%d\n", sizeof(float), 8*sizeof(float));
printf("   double\t%d\t%d\n", sizeof(double), 8*sizeof(double));
printf("long double\t%d\t%d\n", sizeof(long double), 8*sizeof(long double));
printf("short int\t%d\t%d\n", sizeof(short int), 8*sizeof(short int));
printf("u short int\t%d\t%d\n", sizeof(unsigned short int), 8*sizeof(unsigned short int));
printf("unsigned char\t%d\t%d\n", sizeof(unsigned char), 8*sizeof(unsigned char));
return(0);
}
