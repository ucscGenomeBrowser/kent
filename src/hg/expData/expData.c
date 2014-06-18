/* expData -  Takes in a relational database and outputs expression information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "expData.h"
#include "sqlList.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "expData -  Takes in a relational database and outputs expression information\n"
  "usage:\n"
  "   expData inputDataBase output\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

char* floatToString(float input)
{
char* result = needMem(sizeof(result));
sprintf(result,"%f", input);
return result;
freez(result);
}


void printOutput(FILE *f, struct hash *hash, int count)
{
char *result = needMem(sizeof(result)); 
int i;
for (i = 0 ; i < count; ++i)
    {
    result = hashFindVal(hash,floatToString(i)); 
    struct hashEl *hel;
    fprintf(f,"%s", floatToString(i)); 
    fprintf(f,"%s", "    ");
    for (hel = hashLookup(hash,floatToString(i)); hel != NULL; hel = hashLookupNext(hel))
        {
	char *value = hel->val;
        fprintf(f, "%s", value);
        fprintf(f, "%s", "    ");
	}
    fprintf(f,"%s\n", ";"); 
    }
}






void expData(char *output)
/* Grabs expression data and formats it nicely */
{
FILE *f = mustOpen(output, "w");
struct sqlConnection *conn = sqlConnect("hgFixed");
char query[512];
sqlSafef(query, sizeof(query), "select * from gnfHumanAtlas2Median limit 1");
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
char *line = needMem(sizeof(line));
struct expData *list = NULL;
struct hash *hash = hashNew(0);
int count = 0;
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct expData *exp = expDataLoad(row);
    int i;
    count = exp->expCount;
    for (i = 0; i<exp->expCount; ++i)
        {
	char *prepName = catTwoStrings(exp->name," : ");
	char *index = floatToString(exp->expScores[i]);
	line = catTwoStrings(prepName,index);
        hashAdd(hash,floatToString(i),line);
        }
    slAddHead(&list, exp);
    }
printOutput(f,hash,count);
slReverse(&list);
expDataFreeList(&list);
carefulClose(&f);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
expData(argv[1]);
return 0;
}
