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
/* Get the headers. */
lineFileNextReal(lf, &line);
while(line[0] == '\t')
    line++;
colCount = chopString(line, "\t", NULL, 0);
AllocArray(colNames, colCount);
AllocArray(words, colCount+1);
AllocArray(rowNames, rowMax);
AllocArray(M, rowMax);
tmp = cloneString(line);
chopByChar(tmp, '\t', colNames, colCount);
while(lineFileNextRow(lf, words, colCount+1))
    {
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
    hashAddInt(iHash, buff, rowCount);
    rowNames[rowCount] = cloneString(words[0]);
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
       
