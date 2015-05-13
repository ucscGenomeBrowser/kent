/* cirmTags - A program that may eventually handle the .xlxs format (Excel), and parse tags. Rough draft for now. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "common.h"
#include "options.h"
#include "cdwValid.h"
#include "tokenizer.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cirmTags - A program that may eventually handle the .xlxs format (Excel), and parse tags. Rough draft for now\n"
  "usage:\n"
  "   cirmTags XXX\n"
  "options:\n"
  "   cirmTags input.csv output.somethingUseful\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void cirmTags(char *input, char *output)
/* cirmTags - A program that may eventually handle the .xlxs format (Excel), and parse tags. Rough draft for now. */
{
struct lineFile *lf = lineFileOpen(input, TRUE);
//FILE *f = mustOpen(output, "w");
char *line = NULL; 
//lineFileNext(lf, &line, NULL);
//printf("%s\n", line);
/* OLD 
struct tokenizer *tkz = tokenizerOnLineFile(lf);
char *curToken = tokenizerNext(tkz); 
for (; curToken != NULL; curToken = tokenizerNext(tkz))
    {
    printf("%s\n", curToken);
    }
exit(0);
*/ //END OLD
char *table[100][2];
int rowCount = 0;
struct hash *fullHash = newHash(4);
while(lineFileNext(lf, &line,  NULL))
    /* Goes over each row */
    {
    int i=0,j=0, k=chopString(line,"\t",NULL,j);
    char *choppedLine[k];
    printf("%s\n", line);
    chopString(line, "\t", choppedLine, k);
    for (; i<k; ++i)
	/* This is successfully iterating over the values in a row */
	{
	table[rowCount][i] = choppedLine[i];
	printf("The %ith element is %s \n",i,choppedLine[i]);
	}
    hashAdd(fullHash, choppedLine[0], choppedLine[1]);
    ++rowCount; 
    }
struct hashEl *temp = hashLookup(fullHash,"html"); 

printf("the html hashLookup hEl name is %s value is %s \n", temp->name, (char *)(temp->val));
}



int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
cirmTags(argv[1], argv[2]);
return 0;
}
