/* Download the coordinates for a range of human coordinates of
   form chrN:1-100. Designed for getting probe values for exons. */
#include "common.h"
#include "hdb.h"
#include "jksql.h"
#include "hgConfig.h"
#include "sample.h"

struct genomeBit
/* Piece of the genome */
{
    struct genomeBit *next;
    char *fileName;
    char *chrom;
    int chromStart;
    int chromEnd;
};

void usage() 
{
errAbort("samplesForCoordinates - Small program that takes coordinates for hg7 build\n"
	 "and cuts out the results from the affy transcriptome tracks.\n"
	 "usage:\n\t"
	 "samplesForCoordinates <chrN:100-200>\n");
}

struct genomeBit *parseGenomeBit(char *name)
/* take a name like chrN:1-1000 and parse it into a genome bit */
{
struct genomeBit *gp = NULL;
char *pos1 = NULL, *pos2 = NULL;
AllocVar(gp);
pos1 = strstr(name, ":");
 if(pos1 == NULL)
   errAbort("runSlam::parseGenomeBit() - %s doesn't look like chrN:1-1000", name);
gp->chrom = name;
*pos1 = '\0';
gp->chrom = cloneString(gp->chrom);
pos1++;
pos2 = strstr(pos1, "-");
 if(pos2 == NULL)
   errAbort("runSlam::parseGenomeBit() - %s doesn't look like chrN:1-1000, name");
*pos2 = '\0';
pos2++;
gp->chromStart = atoi(pos1);
gp->chromEnd = atoi(pos2);
return gp;
}

struct sqlConnection *getHgdbtestConn(char *db) 
/* Read .hg.conf and return connection. */
{
char *host = cfgOption( "hgdbtest.host");
char *user = cfgOption( "hgdbtest.user");
char *password = cfgOption( "hgdbtest.password");
hSetDbConnect(host,db,user,password);
return sqlConnectRemote(host, user,password, db);
}

int sampleCmpName(const void *va, const void *vb)
{
/* Compare to sort based on name, then on chromStart */
const struct sample *a = *((struct sample **)va);
const struct sample *b = *((struct sample **)vb);
int dif;
dif = strcmp(a->name, b->name);
if (dif == 0)
    dif = a->chromStart - b->chromEnd;
return dif;
}

char *getNameForExp(int num)
{
switch(num)
    {
    case 1 : 
	return cloneString("A-375");
    case 2 : 
	return cloneString("CCRF-Cem");
    case 3 : 
	return cloneString("COLO-205");
    case 4 : 
	return cloneString("FHs-738Lu");
    case 5 : 
	return cloneString("HepG2");
    case 6 : 
	return cloneString("Jurkat");
    case 7 : 
	return cloneString("NCCIT");
    case 8 : 
	return cloneString("NIH_OVCAR-3");
    case 9 : 
	return cloneString("PC3");
    case 10 : 
	return cloneString("SK-N-AS");
    case 11 : 
	return cloneString("U-87MG");
    default:
	errAbort("Don't recognize num %d");
    }
}

char *getHumanName(char *name)
{
char *s = cloneString(name);
char *mark = strstr(s,".");
int exp = 0;
char buff[256];
assert(mark);
*mark = '\0';
exp = atoi(s);
freez(&s);
s = getNameForExp(exp);
snprintf(buff, sizeof(buff), "%s.%s", s, name);
return cloneString(buff);
}

void outputSamples(struct sample *sList, FILE *out, int chromStart, int chromEnd)
/* Output the sample results in tab delimited form where
   first column is experiment name and following columns are
   the probe results for that experiment. */
{
struct sample *s = NULL;
char *name = NULL;
char *outputName = NULL;
int i;
int start, end;
slSort(&sList, sampleCmpName);
assert(sList);
name = sList->name;
outputName = getHumanName(name);
fprintf(out, "%s", outputName);

for(s = sList; s != NULL; s = s->next)
    {
    start = s->chromStart;
    end = s->chromEnd;
    if(differentString(name, s->name))
	{
	name = s->name;
	outputName = getHumanName(name);
	fprintf(out,"\n%s",outputName);
	}
    for(i=0; i< s->sampleCount; i++)
	{
	if(start + s->samplePosition[i] >= chromStart && start + s->samplePosition[i] <= chromEnd)
	    fprintf(out, "\t%d", s->sampleHeight[i]);
	}
    }
fprintf(out,"\n");
}


void getSamples(char *coordinates, FILE *out)
/* Select the samples and output. */
{
struct genomeBit *gb = parseGenomeBit(coordinates);
struct sqlConnection *conn = getHgdbtestConn("sugnet");
struct sqlResult *sr = NULL;
char **row;
struct sample *sList=NULL,*s=NULL;
int rowOffset;
warn("Starting query for coordiantes %s:%d-%d, size %d", gb->chrom, gb->chromStart, gb->chromEnd, (gb->chromEnd-gb->chromStart));
sr = hRangeQuery(conn, "affyTrans", gb->chrom, gb->chromStart, gb->chromEnd, NULL, &rowOffset);
warn("Putting into samples list");
while((row = sqlNextRow(sr)) != NULL)
    {
    s = sampleLoad(row+rowOffset);
    slAddHead(&sList, s);
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
warn("Outputting results");
outputSamples(sList, out, gb->chromStart, gb->chromEnd);
sampleFreeList(&sList);
}

int main(int argc, char *argv[])
{
if(argc == 1)
    usage();
getSamples(argv[1],stdout);
warn("Done");
return 0;
}









