#pragma comment(exestr, "@(#) UCSC Hg $Id: t.c,v 1.2 2003/05/06 07:41:06 kate Exp $")
/* Test program */

#include <stdio.h>


int countFaRecords(char *file) 
/* Count fasta records in a file */
{
int c;
FILE *f;
int count = 0;
int i = 0;

if ((f = fopen(file, "r")) == NULL)
   {
   fprintf(stderr, "Couldn't open %s", file);
   perror(" ");
   return 0;
   }
while ((c = getc(f)) != EOF)
    {
    if (c == '>') 
	count++;
    }
fclose(f);
return count;
}

main(int argc, char *argv[]) {
int count = countFaRecords("foo");
printf("Count=%d\n", count);
}

