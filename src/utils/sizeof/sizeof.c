/*
 *	sizeof - show size of various C types for reference
 *	$Id: sizeof.c,v 1.2 2005/12/02 19:17:24 hiram Exp $
 */
#include <stdio.h>

int main()
{
printf("     type   bytes    bits\n");
printf("     char\t%d\t%d\n", (int)sizeof(char), 8*(int)sizeof(char));
printf("      int\t%d\t%d\n", (int)sizeof(int), 8*(int)sizeof(int));
printf("     long\t%d\t%d\n", (int)sizeof(long), 8*(int)sizeof(long));
printf("long long\t%d\t%d\n", (int)sizeof(long long), 8*(int)sizeof(long long));
printf("   size_t\t%d\t%d\n", (int)sizeof(size_t), 8*(int)sizeof(size_t));
printf("   void *\t%d\t%d\n", (int)sizeof(void *), 8*(int)sizeof(void *));
printf(" unsigned\t%d\t%d\n", (int)sizeof(unsigned), 8*(int)sizeof(unsigned));
printf("long unsigned\t%d\t%d\n", (int)sizeof(long unsigned),
	8*(int)sizeof(long unsigned));
printf("    float\t%d\t%d\n", (int)sizeof(float), 8*(int)sizeof(float));
printf("   double\t%d\t%d\n", (int)sizeof(double), 8*(int)sizeof(double));
printf("long double\t%d\t%d\n", (int)sizeof(long double),
	8*(int)sizeof(long double));
printf("short int\t%d\t%d\n", (int)sizeof(short int), 8*(int)sizeof(short int));
printf("u short int\t%d\t%d\n", (int)sizeof(unsigned short int),
	8*(int)sizeof(unsigned short int));
printf("unsigned char\t%d\t%d\n", (int)sizeof(unsigned char),
	8*(int)sizeof(unsigned char));
return(0);
}
