#include "common.h"
#include "sample.h"
#include "cheapcgi.h"

static char const rcsid[] = "$Id: avgTranscriptomeExps.c,v 1.4.338.1 2008/07/31 06:43:34 markd Exp $";

boolean doAll = FALSE;
char *suffix = NULL;
void usage()
{
errAbort("Averages together replicates of the affy transcriptome data set.\n"
	 "Will skip certain experiments unless directed otherwise as they\n"
	 "were not in the original data set.\n"
	 "usage:\n\t"
	 "avgTranscriptomeExps <go> <optional -doAll> <optional suffix=pairs.sample.norm>\n");
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
	errAbort("Don't recognize num %d", num);
    }
return NULL;
}

struct sample *avgSamples(struct sample *s1,
			  struct sample *s2,
			  struct sample *s3,
			  int expIndex, int index)
{
struct sample *ret = NULL; //, *s1, *s2, *s3;
int i;
/* s1 = (struct sample *)slElementFromIx(sList1, index); */
/* s2 = (struct sample *)slElementFromIx(sList2, index); */
/* s3 = (struct sample *)slElementFromIx(sList3, index); */
AllocVar(ret);
ret->chrom = cloneString(s1->chrom);
ret->chromStart = s1->chromStart;
ret->chromEnd = s1->chromEnd;
ret->name = getNameForExp(expIndex);
snprintf(ret->strand, sizeof(ret->strand), "%s", s1->strand);
ret->sampleCount = s1->sampleCount;
AllocArray(ret->samplePosition, ret->sampleCount);
AllocArray(ret->sampleHeight, ret->sampleCount);
ret->score = (s1->score + s2->score + s3->score)/3;
for(i=0;i<ret->sampleCount; i++)
    {
    ret->samplePosition[i] = s1->samplePosition[i];
    if(!((s1->samplePosition[i] == s2->samplePosition[i]) && (s2->samplePosition[i] == s3->samplePosition[i])))
	errAbort("avgTranscriptomeExps::avgSamples() - for probe %s:%d-%d positions at index %i don't agree. %d, %d, %d", 
		 ret->chrom, ret->chromStart, ret->chromEnd, i, s1->samplePosition[i], s2->samplePosition[i], s3->samplePosition[i]);
    ret->sampleHeight[i] = (s1->sampleHeight[i] + s2->sampleHeight[i] + s3->sampleHeight[i])/3;
    }
return ret;
}

void avgTranscriptomeExps()
{
struct sample *s = NULL;
struct sample *s1,*s2,*s3;
struct sample *sList1, *sList2, *sList3 = NULL;
struct sample **sArray1, **sArray2, **sArray3 = NULL;
int i,j,k;
FILE *out = NULL;
for(i=1; i<12; i++)
    {
    for(j='a'; j<'d'; j++)
	{
	char buff[2048];
	int count;
	struct sample *avgList = NULL, *avg;
	if((i != 6 || j != 'a') || doAll)
	    {
	    snprintf(buff, sizeof(buff), "%d.%c.1.%s", i,j,suffix);
	    printf("Averaging %s\n", buff);
	    sList1 = sampleLoadAll(buff);
	    snprintf(buff, sizeof(buff), "%d.%c.2.%s", i,j,suffix);
	    sList2 = sampleLoadAll(buff);
	    snprintf(buff, sizeof(buff), "%d.%c.3.%s", i,j,suffix);
	    sList3 = sampleLoadAll(buff);
	    count = slCount(sList1);
	    AllocArray(sArray1,count);
	    AllocArray(sArray2,count);
	    AllocArray(sArray3,count);
	    for(k=0, s1=sList1, s2=sList2, s3=sList3; k<count; s1=s1->next,s2=s2->next,s3=s3->next, k++)
		{
		sArray1[k] = s1;
		sArray2[k] = s2;
		sArray3[k] = s3;
		}
	    for(k=0;k<count; k++)
		{
		avg = avgSamples(sArray1[k], sArray2[k], sArray3[k], i, k);
		slAddHead(&avgList, avg);
		}
	    slReverse(&avgList);
	    snprintf(buff, sizeof(buff), "%d.%c.1.%s.avg", i,j,suffix);
	    out = mustOpen(buff, "w");
	    for(s = avgList; s != NULL; s = s->next)
		sampleTabOut(s, out);
	    carefulClose(&out);
	    freez(&sArray1);
	    freez(&sArray2);
	    freez(&sArray3);
	    sampleFreeList(&avgList);
	    sampleFreeList(&sList1);
	    sampleFreeList(&sList2);
	    sampleFreeList(&sList3);
	    }
	}
    }
}

int main(int argc, char *argv[])
{
cgiSpoof(&argc, argv);
if(argc == 1)
    usage();
else 
    {
    suffix = cgiUsualString("suffix", "pairs.sample.norm");
    doAll = cgiBoolean("doAll");
    avgTranscriptomeExps();
    }
return 0;
}
