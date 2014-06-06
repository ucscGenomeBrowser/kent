/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hdb.h"
#include "jksql.h"


struct slName *chromNames = NULL;
char *dirName = NULL;

int chunkSize = 0;

void usage() 
{
errAbort("nmerParasolGenome - creates a series of parasol jobs to span the entire genome\n"
	 "for nmerAlign. Must be run from a machine that has the genome database\n"
	 "as it is used for getting chromsomes names, files, and sizes.\n"
	 "usage:\n\t"
	 "nmerParasolGenome <genome {hg12,hg13,etc}> <chunkSize> <fastaFile.fa> <oligoSize> <outputDir>\n");
}

void getChromNamesAndDirForDb(char *db)
{
struct sqlConnection *conn = hConnectCentral();
char query[512];
char buff[512];
char *tmpMark = NULL;
int buffSize = 512;

sqlSafef(query, sizeof(query), "select nibPath from dbDb where name='%s'", db);
if(sqlQuickQuery(conn, query, buff, buffSize) == NULL)
    errAbort("Coun't find nib dir for genome %s\n", db);
dirName = needMem(buffSize*sizeof(char));
tmpMark = strrchr(buff, '/');
if(tmpMark != NULL)
    *tmpMark = '\0';
snprintf(dirName, buffSize, "%s/mixedNib/", buff);
chromNames = hAllChromNames();
hDisconnectCentral(&conn);
}

void writeOutJobs(char *fastaFile, int size, int oligoSize, char *outDir)
{
struct slName *chromName = NULL;
char buff[512];
FILE *out = mustOpen("parasol.spec", "w");
for(chromName = chromNames; chromName != NULL; chromName = chromName->next) 
    {
    int chromSize = hChromSize(chromName->name);
    int mark = 0;
    while(mark + chunkSize < chromSize) 
	{
	fprintf(out, "nmerAlign %d %d %s%s.nib %s %s/%s:%d-%d.nmer nmerSize=%d\n", 
		mark, mark+chunkSize, dirName, chromName->name, fastaFile, outDir, chromName->name, mark, mark+chunkSize, oligoSize);
	mark += chunkSize - 25;
	}
    fprintf(out, "nmerAlign %d %d %s%s.nib %s %s/%s:%d-%d.nmer nmerSize=%d\n", 
		mark, chromSize , dirName, chromName->name, fastaFile, outDir, chromName->name, mark, chromSize, oligoSize);
    }
}

int main(int argc, char *argv[])
{
int oligoSize = 0;
if(argc != 6)
    usage();
hSetDb(argv[1]);
chunkSize = atoi(argv[2]);
oligoSize = atoi(argv[4]);
getChromNamesAndDirForDb(argv[1]);
writeOutJobs(argv[3], chunkSize, oligoSize, argv[5]);
return 0;
}
