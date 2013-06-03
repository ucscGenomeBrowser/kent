/* ultraPlateau - Find bounds of conservation plateau around region. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "bed.h"
#include "hdb.h"
#include "wiggle.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "ultraPlateau - Find bounds of conservation plateau around region\n"
  "usage:\n"
  "   ultraPlateau database ultraTrack consTrack output.bed\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct bed *readBed4(struct sqlConnection *conn, char *table)
/* Read bed 4 table into list in memory. */
{
struct sqlResult *sr;
char **row;
char buf[256];
struct bed *bedList = NULL, *bed;

sqlSafef(buf, sizeof(buf), "select chrom,chromStart,chromEnd,name from %s", table);
sr = sqlGetResult(conn, buf);
while ((row = sqlNextRow(sr)) != NULL)
    {
    bed = bedLoad(row);
    slAddHead(&bedList, bed);
    }
sqlFreeResult(&sr);
slReverse(&bedList);
return bedList;
}

void dumpWiggleArray(struct wiggleArray *wa)
/* Print out info from wiggle array. */
{
int count = wa->winEnd - wa->winStart;
int i;
printf("%s:%d-%d\n", wa->chrom, wa->winStart, wa->winEnd);
for (i=0; i<count; ++i)
   printf("%f\n", wa->data[i]);
}

void expandUltra(struct bed *bed, char *database, char *consTrack,
	int dropBases, float dropThreshold)
/* Expand ultraconserved region until get at least dropBases that are
 * below dropThreshold in consTrack. */
{
struct wiggleDataStream *wds = newWigDataStream();
float *dataVals;
int i;
int ucStart, ucEnd;
int consStart, consEnd;
float sum, ave;

/* Expand range to grab from wiggle track a bit. */
int chromSize = hChromSize(bed->chrom);
int expMax = 10000, expSize;
int wigStart = bed->chromStart - expMax;
int wigEnd = bed->chromEnd + expMax;
if (wigStart < 0)
    wigStart = 0;
if (wigEnd > chromSize)
    wigEnd = chromSize;
expSize = wigEnd - wigStart;
ucStart = bed->chromStart - wigStart;
ucEnd = bed->chromEnd - wigStart;

/* Get wiggle data into dataVals. */
wds->setMaxOutput(wds, 1000000);
wds->setChromConstraint(wds, bed->chrom);
wds->setPositionConstraint(wds, wigStart, wigEnd);
wds->getData(wds, database, consTrack, wigFetchDataArray);
dataVals = wds->array->data;

/* Do sanity check making sure ultra is all above threshold. */
sum = 0;
for (i=ucStart; i < ucEnd; ++i)
    {
    float val = dataVals[i];
    if (!isnan(val))
       sum += val;
    }
ave = sum / (bed->chromEnd - bed->chromStart);
if (ave < dropThreshold)
    {
    warn("!!!Ultra at %s:%d-%d, average value is just %f\n",
    	bed->chrom, bed->chromStart+1, bed->chromEnd);
    }

/* Expand start of ultra. */
consStart = ucStart;
for (i=ucStart-1; i >= 0; --i)
    {
    float val = dataVals[i];
    if (isnan(val) || val < dropThreshold)
        {
	if (consStart - i >= dropBases)
	    break;
	}
    else
        {
	consStart = i;
	}
    }

/* Expand end of ultra. */
consEnd = ucEnd - 1;
for (i=ucEnd; i<expSize; ++i)
    {
    float val = dataVals[i];
    if (isnan(val) || val < dropThreshold)
        {
	if (i - consEnd >= dropBases)
	    break;
	}
    else
        {
	consEnd = i;
	}
    }

/* Set limits of bed. */
uglyf("%s:%d-%d   ->  ",
   bed->chrom, bed->chromStart+1, bed->chromEnd);
bed->chromStart = consStart + wigStart;
bed->chromEnd = consEnd + wigStart;
uglyf("%s:%d-%d\n", 
   bed->chrom, bed->chromStart+1, bed->chromEnd);

/* Clean up. */
destroyWigDataStream(&wds);
}

void ultraPlateau(char *database, char *ultraTrack, 
	char *consTrack, char *outBed)
/* ultraPlateau - Find bounds of conservation plateau around region. */
{
struct sqlConnection *conn = sqlConnect(database);
struct bed *bed, *bedList = readBed4(conn, ultraTrack);
FILE *f = mustOpen(outBed, "w");
struct hash *uniqHash = hashNew(0);
hSetDb(database);
printf("Read %d ultras from %s.%s\n", slCount(bedList), database, ultraTrack);
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    char uniqName[512];
    struct bed *dupeBed;
    expandUltra(bed, database, consTrack, 5, 0.85);
    bed->name[1] = 'x';
    safef(uniqName, sizeof(uniqName), 
    	"%s:%d-%d", bed->chrom, bed->chromStart, bed->chromEnd);
    dupeBed = hashFindVal(uniqHash, uniqName);
    if (dupeBed != NULL)
	{
        printf("Skipping %s at %s:%d-%d, same as %s\n",  bed->name,
		bed->chrom, bed->chromStart, bed->chromEnd, dupeBed->name);
	}
    else
	{
	hashAdd(uniqHash, uniqName, bed);
	fprintf(f, "%s\t%d\t%d\t%s\n", bed->chrom, bed->chromStart, bed->chromEnd,
		bed->name);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
ultraPlateau(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
