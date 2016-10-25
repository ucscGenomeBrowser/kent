
/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "axt.h"
#include "common.h"
#include "bigWig.h"
#include "bigBed.h"
#include "dystring.h"
#include "errCatch.h"
#include "hgBam.h"
#include "htmshell.h"
#include "htmlPage.h"
#include "hui.h"
#include "net.h"
#include "options.h"
#include "trackDb.h"
#include "trackHub.h"
#include "udc.h"
#include "vcf.h"
#include "mdb.h"

static int cacheTime = 1;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hubToMetaRa - Crawl a track data hub and output search strings\n"
  "usage:\n"
  "   hubToMetaRa http://yourHost/yourDir/hub.txt metaData.ra\n"
  "options:\n"
  "   -udcDir=/dir/to/cache - place to put cache for remote bigBed/bigWigs.\n"
  "   -cacheTime=N - set cache refresh time in seconds, default %d\n"
  "   -verbose=2            - output verbosely\n"
  , cacheTime
  );
}

static struct optionSpec options[] = {
   {"udcDir", OPTION_STRING},
   {"cacheTime", OPTION_INT},
   {NULL, 0},
};

char *removeBracketingQuotes(char *input)
// Remove quotes from string if they are matching and at the beginning and end of string
{
int length = strlen(input);
if ((*input == '\"') && (input[length - 1] == '\"'))
    {
    input[length - 1] = 0;
    return &input[1];
    }
else
    return input;
}

void outputTrackMeta(struct trackHub *hub, struct trackHubGenome *genome, struct trackDb *tdb, FILE *output)
// Output a track stanza.
{
char *metaData = trackDbSetting(tdb, "metadata");
if (metaData != NULL)
    {
    const struct mdbObj *mdbObj = metadataForTableFromTdb(tdb);
    struct mdbVar *mdbVar;
    fprintf(output, "track\t%s\n", trackHubSkipHubName(tdb->track));
    for (mdbVar=mdbObj->vars;mdbVar!=NULL;mdbVar=mdbVar->next)  
        {      
        if (sameString(mdbVar->var, "objType"))
            continue;
        mdbVar->var = replaceChars(mdbVar->var, " ", "_");
        mdbVar->var = replaceChars(mdbVar->var, "\"", " ");
        mdbVar->var = trimSpaces(mdbVar->var);
        mdbVar->val = trimSpaces(mdbVar->val);
        mdbVar->val = removeBracketingQuotes(mdbVar->val);
        mdbVar->val = trimSpaces(mdbVar->val);

        if (isEmpty(mdbVar->val))
            continue;
        
        fprintf(output, "%s\t%s\n", mdbVar->var,mdbVar->val);
        }
    fprintf(output,"\n");
    }
}


int outTrackDb(struct trackHub *hub, struct trackHubGenome *genome, struct trackDb *tdb, FILE *output)
// Open a track hub and output a RA file with its metadata.
{
int retVal = 0;

struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    outputTrackMeta(hub, genome, tdb, output);
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    retVal = 1;
    fprintf(stderr, "%s", errCatch->message->string);
    }
errCatchFree(&errCatch);

return retVal;
}

int outMetaRa(char *hubUrl, char *metaRaFile)
/* Crawl a track data hub and output metadata. */
{
struct errCatch *errCatch = errCatchNew();
struct trackHub *hub = NULL;
int retVal = 0;

if (errCatchStart(errCatch))
    {
    hub = trackHubOpen(hubUrl, "hub_0");
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    retVal = 1;
    fprintf(stderr, "%s\n", errCatch->message->string);
    }
errCatchFree(&errCatch);

if (hub == NULL)
    return 1;

FILE *output = mustOpen(metaRaFile, "w");
struct trackHubGenome *genome;
for (genome = hub->genomeList; genome != NULL; genome = genome->next)
    {
    struct errCatch *errCatch = errCatchNew();
    struct trackDb *tdbList = NULL;

    if (errCatchStart(errCatch))
        {
        tdbList = trackHubTracksForGenome(hub, genome);
        trackHubPolishTrackNames(hub, tdbList);
        }
    errCatchEnd(errCatch);
    if (errCatch->gotError)
        {
        retVal = 1;
        fprintf(stderr, "%s", errCatch->message->string);
        }
    errCatchFree(&errCatch);

    verbose(2, "%d tracks in %s\n", slCount(tdbList), genome->name);
    struct trackDb *tdb;
    for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
        {
        retVal |= outTrackDb(hub, genome, tdb, output);
        }
    }

trackHubClose(&hub);
return retVal;
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);

if (argc != 3)
    usage();


udcSetCacheTimeout(cacheTime);
// UDC cache dir: first check for hg.conf setting, then override with command line option if given.
setUdcCacheDir();
udcSetDefaultDir(optionVal("udcDir", udcDefaultDir()));

if (outMetaRa(argv[1], argv[2]))
    {
    return 1;
    }
return 0;
}
