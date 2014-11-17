/* cdwMakeWigSpotQa - Look at incoming files and if possible make a cdwQaWigSpot entry for them.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "cdw.h"
#include "../../eap/inc/eapDb.h"
#include "cdwLib.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwMakeWigSpotQa - Look at incoming files and if possible make a cdwQaWigSpot entry for them.\n"
  "usage:\n"
  "   cdwMakeWigSpotQa startId endId\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

struct eapOutput *eapOutputFromFileId(struct sqlConnection *conn, long long fileId)
/* Return output that created this file if any */
{
char query[128];
sqlSafef(query, sizeof(query), "select * from eapOutput where fileId=%lld", fileId);
return eapOutputLoadByQuery(conn, query);
}


void makeWigSpot(struct sqlConnection *conn, long long wigId, long long spotId)
/* Look and see if the wigSpot calcs have been done.  If not, do them. */
{
struct cdwQaWigSpot *spot = cdwQaWigSpotFor(conn, wigId, spotId);
if (spot == NULL)
    spot = cdwMakeWigSpot(conn, wigId, spotId);
cdwQaWigSpotFree(&spot);
}

boolean isPeaky(char *format)
/* Return TRUE if it's a good peak format. */
{
return sameString(format, "narrowPeak") || sameString(format, "broadPeak") || 
    sameString(format, "bigBed");
}

void makeWigSpotQaOnRun(struct sqlConnection *conn, unsigned runId)
/* Load up run and make connections between the wig and the peak outputs */
{
uglyf("makeWigSpotQaOnRun(%p %u)\n", conn, runId);
char query[256];
sqlSafef(query, sizeof(query), "select * from eapRun where id=%u", runId);
struct eapRun *run = eapRunLoadByQuery(conn, query);
if (wildMatch("macs2_*", run->analysisStep) || wildMatch("*hotspot", run->analysisStep))
    {
    sqlSafef(query, sizeof(query), 
	"select cdwValidFile.* from eapRun "
	" join eapOutput on eapRun.id = eapOutput.runId "
	" join cdwValidFile on eapOutput.fileId = cdwValidFile.fileId "
	"where eapRun.id = %u",  runId);
    struct cdwValidFile *vfList = cdwValidFileLoadByQuery(conn, query);
    uglyf("%d on vfList\n", slCount(vfList));

    /* What we'll do is loop through the list twice, taking action or at least considering it, when
     * we get a wig/peak combo */
    struct cdwValidFile *wigVf, *spotVf;
    for (wigVf = vfList; wigVf != NULL; wigVf = wigVf->next)
	{
	if (!sameString(wigVf->format, "bigWig"))
	    continue;
	for (spotVf = vfList; spotVf != NULL; spotVf = spotVf->next)
	    {
	    if (isPeaky(spotVf->format))
		{
		makeWigSpot(conn, wigVf->fileId, spotVf->fileId);
		}
	    }
	}
    cdwValidFileFreeList(&vfList);
    }
eapRunFree(&run);
}


void cdwMakeWigSpotQa(unsigned startId, unsigned endId)
/* cdwMakeWigSpotQa - Look at incoming files and if possible make a cdwQaWigSpot entry for them. */
{
struct sqlConnection *conn = cdwConnectReadWrite();
struct cdwFile *ef, *efList = cdwFileAllIntactBetween(conn, startId, endId);
for (ef = efList; ef != NULL; ef = ef->next)
    {
    /* What we do is look up associated run,  see if it is a run of type we care about,
     * and if so,  if we are the last output, we make sure the cdwQaWigSpot record is made */
    struct eapOutput *pipeOut = eapOutputFromFileId(conn, ef->id);
    if (pipeOut != NULL)
        {
	char query[128];
	sqlSafef(query, sizeof(query), 
	    "select max(fileId) from eapOutput where runId=%u", pipeOut->runId);
	long long responsibleId = sqlQuickLongLong(conn, query);
	if (ef->id == responsibleId)
	    {
	    /* Ah rats, we are the ones stuck calculating this.  We are nested deep
	     * enough let's handle actually handling it for the run in a separate function. */
	    makeWigSpotQaOnRun(conn, pipeOut->runId);
	    }
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
cdwMakeWigSpotQa(sqlUnsigned(argv[1]), sqlUnsigned(argv[2]));
return 0;
}
