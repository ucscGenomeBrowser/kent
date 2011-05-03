#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dMatrix.h"

struct dMatrix *dMatrixLoad(char *fileName)
/* Read in an R style result table. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct dMatrix *dM = NULL;
char **words = NULL;
int colCount=0, i=0;
char *line = NULL;
double **M = NULL;
int rowCount =0;
int rowMax = 1000;
char **rowNames = NULL;
char **colNames = NULL;
struct hash *iHash = newHash(12);
char buff[256];
char *tmp;
int currColCount = 0;
/* Get the headers. */
lineFileNextReal(lf, &line);
while(line[0] == '\t')
    line++;
colCount = chopByWhite(line, NULL, 0);
AllocArray(colNames, colCount);
AllocArray(words, colCount+1);
AllocArray(rowNames, rowMax);
AllocArray(M, rowMax);
tmp = replaceChars(line, "\"", "");
chopByChar(tmp, '\t', colNames, colCount);
while(lineFileNextReal(lf, &line))
    {
    currColCount = chopByWhite(line, words, colCount+1);
    /* Sanity check. */
    if(currColCount != colCount +1)
	errAbort("Wrong number of columns at line %d. Got %d expecting %d",
		 lf->lineIx, currColCount, colCount+1);
    if(rowCount+1 >=rowMax)
	{
	ExpandArray(rowNames, rowMax, 2*rowMax);
	ExpandArray(M, rowMax, 2*rowMax);
	rowMax = rowMax*2;
	}
    safef(buff, sizeof(buff), "%s", words[0]);
    tmp=strchr(buff,':');
    if(tmp != NULL)
	{
	assert(tmp);
	tmp=strchr(tmp+1,':');
	assert(tmp);
	tmp[0]='\0';
	}
    rowNames[rowCount] = replaceChars(words[0], "\"","");
    hashAddInt(iHash, rowNames[rowCount], rowCount);
    AllocArray(M[rowCount], colCount);
    for(i=1; i<=colCount; i++) /* Starting at 1 here as name is 0. */
	{
	M[rowCount][i-1] = atof(words[i]);
	}
    rowCount++;
    }
AllocVar(dM);
dM->nameIndex = iHash;
dM->colCount = colCount;
dM->colNames = colNames;
dM->rowCount = rowCount;
dM->rowNames = rowNames;
dM->matrix = M;
lineFileClose(&lf);
return dM;
}

void dMatrixWrite(struct dMatrix *dm, FILE *out)
/* Write out the matrix to a file. */
{
int i = 0, j = 0;
assert(dm);
for(i = 0; i < dm->colCount - 1; i++)
    fprintf(out, "%s\t", dm->colNames[i]);
fprintf(out, "%s\n", dm->colNames[i]);

for(i = 0; i < dm->rowCount; i++)
    {
    fprintf(out, "%s\t", dm->rowNames[i]);
    for(j = 0; j < dm->colCount-1; j++)
	fprintf(out, "%f\t", dm->matrix[i][j]);
    fprintf(out, "%f\n",  dm->matrix[i][j]);
    }
}
		


void dMatrixFree(struct dMatrix **pDMatrix)
/* Free up the memory used by a dMatrix. */
{
int i = 0;
struct dMatrix *dM = *pDMatrix; 
if(dM == NULL)
    return;
for(i = 0; i < dM->rowCount; i++)
    {
    freez(&dM->matrix[i]);
    freez(&dM->rowNames[i]);
    }
freez(&dM->matrix);
freez(&dM->colNames);
hashFree(&dM->nameIndex);
freez(pDMatrix);
}
       
void dMatrixWriteForSql(struct dMatrix *dM, char *tableName,
			char *toLoad, char *sqlSpec) 
/* Write out two files that together will make a table in an 
   sql database. */
{
int i = 0, j = 0;
FILE *sql = mustOpen(sqlSpec, "w");
FILE *data = mustOpen(toLoad, "w");

/* Make the sql file. */
fprintf(sql, "create table %s (\n"
	"   name varchar(255) not null,\n", tableName);
for(i = 0; i < dM->colCount; i++)
    {
    char *string = cloneString(dM->colNames[i]);
    subChar(string, '.', '_');
    fprintf(sql, "   %s float not null,\n", string);
    freez(&string);
    }
fprintf(sql, "   index(name(16))\n");
fprintf(sql, ");\n");
carefulClose(&sql);

/* print out the data matrix. */
for(i = 0; i < dM->rowCount; i++) 
    {
    fprintf(data, "%s", dM->rowNames[i]);
    for(j = 0; j < dM->colCount; j++) 
	{
	fprintf(data, "\t%.4f", dM->matrix[i][j]);
	}
    fprintf(data, "\n");
    }
carefulClose(&data);
}
