/* forwardSlices - match slice imageFiles in Mahoney 
 * with the corresponding JAX mahoney imageFile. 
 *
 * First it matches on primers and imageFile.id,
 * depending critically on the fact that both visiGene and MGI JAX
 * use auto-increment and processed the rows in the same order.
 * 
 * After that, it tries to match remaining slices without primers,
 * using gene and age to link.  Probably also make some use of imageFile.id ordering.
 * Because wholeMount is bodyPart=1 (varies with db), we just find the ones not equal to 1.
 *
 * Note that wholeMount preserved original images,
 * so we were able to use a different process based on md5sum of .jpg images.
 */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "options.h"
#include "portable.h"
#include "obscure.h"
#include "jksql.h"
#include "spDb.h"
#include "jpegSize.h"

int testMax=0;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "forwardSlices - match slice imageFiles in Mahoney to JAX\n"
  "usage:\n"
  "   forwardSlices visiGeneDb mahoneySlicesSubmissionSet jaxMahoneySubmissionSet wholeBodyPart\n"
  "e.g."
  "   forwardSlices visiJaxMahoney 2 1820 1\n"
  /*
  "options:\n"
  "   -testMax=N - for testing purposes only output first N directories\n"
  */
  );
}

static struct optionSpec options[] = {
   {"testMax", OPTION_INT},
   {NULL, 0},
};



#define MAXPARTS 20

void forwardSlices(char *database, 
    int mahoneySlicesSS, int jaxMahoneySS, int wholeBodyPart, boolean isProbe)
{
struct sqlConnection *conn = sqlConnect(database);
struct sqlConnection *connUpdate = sqlConnect(database);
int imageFiles[MAXPARTS];
int partCount = 0, partIndex=-1;
struct dyString *query = dyStringNew(0);
struct sqlResult *sr=NULL;
char **row=NULL;
unsigned int probe=0, lastProbe=0, ss=0, imf=0;
char *fileName=NULL;
char *lastFile=cloneString("");
int count=0;
if (isProbe)
    {
    dyStringPrintf(query,
	"select distinct probe.id, image.submissionSet, imageFile.id, fileName"
	" from imageFile, image, imageProbe, probe, specimen"
	" where imageProbe.probe = probe.id and imageProbe.image=image.id"
	" and imageFile.id=image.imageFile and image.specimen=specimen.id"
	" and imageFile.submissionSet in (%d,%d) and bodyPart <> %d"
	" order by probe.id, image.submissionSet%s, imageFile.id"
	,mahoneySlicesSS,jaxMahoneySS,wholeBodyPart, (mahoneySlicesSS < jaxMahoneySS) ? " desc" : "");
    }    
else  /* gene (not isProbe) */
    {
    dyStringPrintf(query,
	"select distinct probe.gene, image.submissionSet, imageFile.id, fileName"
	" from imageFile, image, imageProbe, probe, specimen"
	" left join imageFileFwd iffa on iffa.fromIf = imageFile.id"
	" left join imageFileFwd iffb on iffb.toIf = imageFile.id"
	" where imageProbe.probe = probe.id and imageProbe.image=image.id"
	" and imageFile.id=image.imageFile and image.specimen=specimen.id"
	" and bodyPart <> %d"
	" and (((iffa.fromIf is null) and (imageFile.submissionSet=%d))"
	"   or ((iffb.toIf is null) and (imageFile.submissionSet=%d)))"
	" order by probe.gene, image.submissionSet%s, imageFile.id"	
	,wholeBodyPart,mahoneySlicesSS,jaxMahoneySS, (mahoneySlicesSS < jaxMahoneySS) ? " desc" : "" );
    }    
sr = sqlGetResult(conn, query->string);
while (TRUE)
    {
    row = sqlNextRow(sr);
    if (row != NULL)
	{
    	probe = sqlUnsigned(row[0]);
	ss = sqlUnsigned(row[1]);
    	imf = sqlUnsigned(row[2]);
	fileName = cloneStringZ(row[3],6);
	}
    else
	{
    	probe = 0;
	ss = 0;
    	imf = 0;
	fileName = cloneString("");
	}
    if (probe != lastProbe)
	{
	if (partIndex >=0 && (partIndex+1) != partCount)
	    verbose(1,"mismatch probe=%d, partIndex=%d, partCount=%d\n",probe-1,partIndex,partCount);
    	lastProbe = probe;
	partCount = 0;	    
	partIndex = -1;
	freez(&lastFile);
	lastFile = cloneString("");
	}
    	    
    if (ss == jaxMahoneySS)
	{
	imageFiles[partCount] = imf; 
	++partCount;
	if (partCount > MAXPARTS)
	    errAbort("unexpected partCount = %d > MAXPARTS",partCount);
	}
    else
	{
	if (partCount > 0)  /* don't do anything unless we have something */
	    {
	    char query[256];
	    if (!sameString(fileName,lastFile))
		{
		++partIndex;
		freez(&lastFile);
		lastFile = cloneString(fileName);
		}
	    if (partIndex >= partCount)
		{
		verbose(1,"Out of sync: probe=%d, partIndex=%d, partCount=%d\n",probe,partIndex,partCount);
		--partIndex;	
		}
	    safef(query,sizeof(query),"insert into imageFileFwd set fromIf=%d, toIf=%d",
		imf,imageFiles[partIndex]);
	    verbose(2,"%d\t%d\n",imf,imageFiles[partIndex]);
	    sqlUpdate(connUpdate,query);
	    ++count;
	    }
	}
    
	
    freez(&fileName);
    if (!row)
	break;
	
    }
sqlFreeResult(&sr);
dyStringFree(&query);
sqlDisconnect(&conn);
sqlDisconnect(&connUpdate);

printf("added %d new records to imageFileFwd for isProbe=%d\n", count, isProbe);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
testMax = optionInt("testMax", testMax);
if (argc != 5)
    usage();
int mSS = atoi(argv[2]);
int jSS = atoi(argv[3]);
int wholeBodyPart = atoi(argv[4]);
if (mSS < 1 || jSS < 1 || wholeBodyPart < 1)
    usage();
forwardSlices(argv[1], mSS, jSS, wholeBodyPart, TRUE );  /* do match on probe id */
forwardSlices(argv[1], mSS, jSS, wholeBodyPart, FALSE);  /* do match on gene  id */
return 0;
}
