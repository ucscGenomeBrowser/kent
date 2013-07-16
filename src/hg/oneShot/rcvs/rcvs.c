/* rcvs - Compare riken noncoding vs. nonspliced. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "rcvs - Compare riken noncoding vs. nonspliced\n"
  "usage:\n"
  "   rcvs TUinfo_for_fig2.txt tu_fantom2cluster.txt\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void rcvs(char *codingTable, char *clusterTable)
/* rcvs - Compare riken noncoding vs. nonspliced. */
{
struct hash *idHash = newHash(16); // Key id1, val id2
struct hash *nonCodingHash = newHash(16);  // Key clusterId, value 
struct hash *splicedHash = newHash(16);  // Key id2, present if spliced
struct sqlConnection *conn = sqlConnect("mgsc");
struct sqlResult *sr;
char **row;
char *words[16];
int wordCount;
struct lineFile *lf;
int codingSpliced = 0;
int noncodingSpliced = 0;
int codingNonspliced = 0;
int noncodingNonspliced = 0;

/* Read id's into hash */
sr = sqlGetResult(conn, "NOSQLINJ select id1,id2 from rikenIds");
while ((row = sqlNextRow(sr)) != NULL)
    hashAdd(idHash, row[0], cloneString(row[1]));
sqlFreeResult(&sr);

/* Read spliced into hash */
sr = sqlGetResult(conn,
	"NOSQLINJ select name from rikenOrientInfo where intronOrientation != 0");
while ((row = sqlNextRow(sr)) != NULL)
    hashAdd(splicedHash, row[0], NULL);
sqlFreeResult(&sr);

/* Read noncoding clusters into hash */
lf = lineFileOpen(codingTable, TRUE);
while (lineFileNextRow(lf, words, 2))
    {
    if (sameString(words[1], "NoPProt"))
        hashAdd(nonCodingHash, words[0], NULL);
    }
lineFileClose(&lf);

/* Stream through cluster table counting and correlating. */
lf = lineFileOpen(clusterTable, TRUE);
while (lineFileNextRow(lf, words, 2))
    {
    char *cluster = words[0];
    char *id1 = words[1];
    char *id2 = hashMustFindVal(idHash, id1);
    if (hashLookup(nonCodingHash, cluster))
        {
	if (hashLookup(splicedHash, id2))
	    ++noncodingSpliced;
	else
	    ++noncodingNonspliced;
	}
    else
        {
	if (hashLookup(splicedHash, id2))
	    ++codingSpliced;
	else
	    ++codingNonspliced;
	}
    }
printf("noncodingNonspliced %d\n", noncodingNonspliced);
printf("noncodingSpliced %d\n", noncodingSpliced);
printf("codingNonspliced %d\n", codingNonspliced);
printf("codingSpliced %d\n", codingSpliced);
printf("total %d\n", noncodingNonspliced + noncodingSpliced + codingNonspliced + codingSpliced);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
rcvs(argv[1], argv[2]);
return 0;
}
