#include "common.h"
#include "obscure.h"
#include "hCommon.h"
#include "hash.h"
#include "hdb.h"
#include "fa.h"
#include "cheapcgi.h"
#include "portable.h"
#include "jalview.h"

void getSequenceInRange(struct dnaSeq **seqList, struct hash *hash, char *table, char *type, char *tName, int tStart, int tEnd)
/* Load a list of fasta sequences given tName tStart tEnd */
{
struct sqlResult *sr = NULL;
char **row;
struct psl *psl = NULL, *pslList = NULL;
boolean hasBin;
char splitTable[64];
char query[256];
struct sqlConnection *conn = hAllocConn();
struct dnaSeq *seq = NULL;

hFindSplitTable(tName, table, splitTable, &hasBin);
safef(query, sizeof(query), "select qName from %s where tName = '%s' and tEnd > %d and tStart < %d", splitTable, tName, tStart, tEnd);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *acc = cloneString(row[0]);
    struct dnaSeq *seq = hGenBankGetMrna(acc, NULL);
    verbose(9,"%s %s %d<br>\n",acc, tName, tStart);
    if (seq != NULL)
        {
        if (hashLookup(hash, acc) == NULL)
            {
            int len = strlen(seq->name);
            seq->name [len-1] = type[0];
            hashAdd(hash, acc, NULL);
            slAddHead(seqList, seq);
            }
        }
    }
sqlFreeResult(&sr);
slReverse(seqList);
hFreeConn(&conn);
}

int main(int argc, char *argv[])
{
cgiSpoof(&argc,argv);
struct dnaSeq *seqList = NULL, *seq;
struct hash *faHash = hashNew(0);
char *db=cgiString("db");
char *faIn=cgiString("fa");
char *g=cgiOptionalString("g");
struct tempName faTn, alnTn;
struct lineFile *lf;
char *line = NULL;
char clustal[512];
int ret;

//hSetDb(db);
printf("\n");
makeTempName(&alnTn, "clustal", ".aln");
safef(clustal, sizeof(clustal), \
    "clustalw -infile=%s -outfile=%s -quicktree > /dev/null ", \
    faIn, alnTn.forCgi);
ret = system(clustal);    
if (ret != 0)
    errAbort("Error in clustalW");
lf = lineFileOpen(alnTn.forCgi,TRUE);
while (lineFileNext(lf, &line, NULL))
    printf("%s\n",line);
lineFileClose(&lf);
return 0;
}
