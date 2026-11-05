/* trackHubSkipHubNameTester - check that trackHubSkipHubName only returns a pointer into
 * the name it was given.
 *
 * A hub track is called hub_<id>_<name> in the browser, and trackHubSkipHubName returns the
 * <name> part.  Given a name that starts hub_ but has no second underscore, it used to return
 * the address one past NULL, which a caller then read.  Names like that reach it from the
 * cart and from URLs, so they are not only a programming mistake.
 *
 * The test checks where the returned pointer lands before it reads anything through it, so
 * a regression prints a wrong line rather than crashing the test.
 *
 * refs #38414 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "trackHub.h"

static void check(char *name)
/* Print what trackHubSkipHubName makes of one name. */
{
char *label = (name == NULL) ? "NULL" : ((*name == 0) ? "(empty string)" : name);
char *result = trackHubSkipHubName(name);
if (name == NULL)
    printf("  %-32s -> %s\n", label, (result == NULL) ? "NULL" : "not NULL");
else if (result < name || result > name + strlen(name))
    printf("  %-32s -> points outside the name\n", label);
else if (result == name)
    printf("  %-32s -> the whole name\n", label);
else
    printf("  %-32s -> \"%s\"\n", label, result);
}

int main(int argc, char *argv[])
{
printf("a hub track name loses its hub_<id>_ prefix\n");
check("hub_12_myTrack");
check("hub_12_my_track_name");
check("hub_12_");
check("hub__myTrack");

printf("\na name that starts hub_ with no second underscore is returned whole\n");
check("hub_12");
check("hub_");
check("hub_myTrack");

printf("\nany other name is returned whole\n");
check("knownGene");
check("myhub_12_track");
check("HUB_12_track");
check("hub");
check("");
check(NULL);
return 0;
}
