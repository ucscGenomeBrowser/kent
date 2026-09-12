/* backupParseTest - check how the session backup page treats a malformed pair
 * in a stored cart contents string. */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hash.h"
#include "cart.h"
#include "cheapcgi.h"
#include "errCatch.h"
#include "hgConfig.h"

struct downloadResults
/* Only the first two fields of backup.c's struct, which is all slCount and a
 * db read here need.  next must stay first. */
    {
    struct downloadResults *next;
    char *db;
    };

struct downloadResults *processCtsForDownloadInternals(char *contents, char **pTrackHubsVar);

/* Three symbols backup.c wants from hgSession.c, which this test does not link. */
struct cart *cart = NULL;
char *database = NULL;

char *cgiDecodeClone(char *encStr)
/* Same as hgSession.c's. */
{
char *s = cloneString(encStr);
cgiDecode(encStr, s, strlen(encStr));
return s;
}

static char *cases[] = {
/* one custom track, found */
"db=hg38&ctfile_hg38=../trash/ct/probe.bed",
/* an empty pair in front of it names it "&ctfile_hg38", which does not match
 * the ctfile_ prefix, so the custom track is left out of the backup with no
 * warning.  The reader gets a backup that is missing a track.  refs #38185 */
"db=hg38&&ctfile_hg38=../trash/ct/probe.bed",
/* a pair with no =value in front of it does the same.  refs #38340 */
"db=hg38&i&ctfile_hg38=../trash/ct/probe.bed",
/* either one after it aborts the whole backup instead */
"db=hg38&ctfile_hg38=../trash/ct/probe.bed&g-catV2",
"db=hg38&ctfile_hg38=../trash/ct/probe.bed&&",
/* two assemblies, both found */
"db=hg38&ctfile_hg38=../trash/ct/a.bed&ctfile_mm39=../trash/ct/b.bed",
};

int main(int argc, char *argv[])
{
int i;
printf("hg.conf skipMalformedCgiPairs=%s\n\n",
       cfgOptionBooleanDefault("skipMalformedCgiPairs", FALSE) ? "on" : "off");
for (i = 0;  i < ArraySize(cases);  ++i)
    {
    printf("in  : %s\n", cases[i]);
    printf("found: ");
    struct errCatch *errCatch = errCatchNew();
    if (errCatchStart(errCatch))
	{
	struct downloadResults *list, *result;
	list = processCtsForDownloadInternals(cloneString(cases[i]), NULL);
	printf("%d", slCount(list));
	for (result = list;  result != NULL;  result = result->next)
	    printf(" [%s]", result->db);
	}
    errCatchEnd(errCatch);
    if (errCatch->gotError)
	printf("aborted: %s", trimSpaces(errCatch->message->string));
    errCatchFree(&errCatch);
    printf("\n\n");
    }
return 0;
}
