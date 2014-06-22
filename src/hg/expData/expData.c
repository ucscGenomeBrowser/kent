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

typedef struct
{
char *name;
int count;
} expCell;

typedef struct
{
char *name;
float value;
}nameValue;


char* floatToString(float input)
{
char* result = needMem(sizeof(result));
sprintf(result,"%f", input);
return result;
freez(result);
}

int compareExpCells(expCell exp1, expCell exp2)
/* a compare function for the HAC tree */
{
int result = exp1.count - exp2.count;
return sqrt(result*result);
}

expCell mergeExpCells(expCell exp1, expCell exp2)
/* a merge function for the HAC tree */
{
expCell result;
result.count = (exp1.count + exp2.count)/2;
result.name = catTwoStrings(exp1.name,exp2.name);
return result;
}

int compareTwoRows(char *line1, char* line2)
/* seems to be working 
 * Takes in two rows from the table, as strings.
 * Parses the rows and finds the summation of distance between
 * the expression values of each row.  */
{
char *output1[10000];
char *output2[10000];
char *temp1 = cloneString(line1);
char *temp2 = cloneString(line2);
int size1 = 0;
size1 = chopTabs(temp1,output1);
int size2 = 0;
size2 = chopTabs(temp2,output2);
//double result = 1000000;
//if (size1 != size2)
//    {
//    return result;
//    }
int i;
double sum = 0;
for (i = 0; i<size1; ++i)
    {
    double diff = atoi(output1[i]) - atoi(output2[i]);
    sum += sqrt((diff * diff));
    }
return sum;
}



void fileIO (char *input, char *output)
{
FILE *f = mustOpen(output,"w");
//struct lm *localMem = lmInit(0);
struct lineFile *lf = lineFileOpen(input, TRUE);
char *line;
long count = 1;
int i, j;
struct hash *hashRows = hashNew(0);
if (!lineFileNext(lf, &line, NULL))
    {
    errAbort("%s This should be the column names", lf->fileName);
    }
while (lineFileNext(lf,&line,NULL))
    {
    char *temp = cloneString(line);
    hashAdd(hashRows,floatToString(count),temp);
    ++ count;
    }
for (i = 1; i < count; ++i)
    {
    for (j = i + 1 ; j < count ; ++j)
        {
	expCell exp;
	exp.name = catTwoStrings(floatToString(i), floatToString(j));
        char *temp1 = hashFindVal(hashRows, floatToString(i));
	char *temp2 = hashFindVal(hashRows, floatToString(j));
	exp.count = compareTwoRows(temp1,temp2);
	fprintf(f,"%d",compareTwoRows(temp1, temp2));
	fprintf(f,"%s\n"," ");
	}
    }
//struct hacTree *clusters = hacTreeFromItems((struct slList *)sldList, localMem, compareExpCell, mergeExpCell, NULL, NULL);
}


void printOutput(FILE *f, struct hash *hash, int count)
/* Prints the data to the output file.
 * Each row corresponds to a tissue sample; the first element.
 * ALl subsequent elements are in sequence name : expression value pairs. */
{
nameValue *output;
int i;
for (i = 0 ; i < count; ++i)
    {
    output = hashFindVal(hash,floatToString(i)); 
    struct hashEl *hel;
    fprintf(f,"%s", floatToString(i)); 
    fprintf(f,"%s", "    ");
    for (hel = hashLookup(hash,floatToString(i)); hel != NULL; hel = hashLookupNext(hel))
        {
	nameValue *temp = hel->val;
	char *name = temp->name;
	long score = temp->value;
        fprintf(f, "%s", name);
	fprintf(f, "%s", ":");
        fprintf(f, "%ld", score);
	fprintf(f, "%s", " + ");
	}  
    fprintf(f, "\n%s", " " );
    }
}




void expData(char *output)
/* Grabs expression data and formats it nicely */
{
FILE *f = mustOpen(output, "w");
struct sqlConnection *conn = sqlConnect("hgFixed");
char query[512];
sqlSafef(query, sizeof(query), "select * from gnfHumanAtlas2Median limit 2");
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
	nameValue *pair = needMem(sizeof(pair));
	pair->name = exp->name;
	pair->value = exp->expScores[i];
        hashAdd(hash,floatToString(i),pair);
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
if (argc != 3)
    usage();
fileIO(argv[1],argv[2]);
return 0;
}
