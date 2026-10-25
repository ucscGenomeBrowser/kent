/* ctLongFieldTester - custom track fields longer than the buffer they are copied into.
 *
 * Since v500 a BED or bedDetail custom track whose strand column held more than one
 * character ("+1") failed with "buffer overflow", and a GFF or GTF track whose group column
 * was 128 characters or longer failed with "buffer overflow, size 128".  Both copies now keep
 * what fits and terminate it.  A strand that is not + - or . still fails, now with the value
 * in the message.
 *
 * The BED and GFF cases go through customFactoryParse, the way an upload does.  The bedDetail
 * loader always loads into the customTrash database, so that case calls bedDetailLineFileLoad,
 * the row parser it uses, directly.
 *
 * refs #38473 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "linefile.h"
#include "errCatch.h"
#include "hdb.h"
#include "chromAlias.h"
#include "customTrack.h"
#include "customFactory.h"
#include "bedDetail.h"

static char *db = "hg38";

static void printError(struct errCatch *errCatch)
/* Print the first line of a caught error. */
{
char *msg = trimSpaces(errCatch->message->string);
char *nl = strchr(msg, '\n');
if (nl != NULL)
    *nl = 0;
printf("  aborted: %s\n", msg);
}

static void bedCase(char *what, char *text)
/* Parse text as an uploaded custom track and print each item's strand. */
{
printf("%s\n", what);
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    struct customTrack *ct = customFactoryParse(db, cloneString(text), FALSE, NULL, NULL);
    struct bed *bed;
    for (bed = ct->bedList; bed != NULL; bed = bed->next)
        printf("  %s strand '%s'\n", bed->name, bed->strand);
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    printError(errCatch);
errCatchFree(&errCatch);
}

static void bedDetailCase(char *what, char *line)
/* Parse one bedDetail row and print its strand. */
{
printf("%s\n", what);
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    struct lineFile *lf = lineFileOnString("bedDetail", TRUE, cloneString(line));
    char *row[14];
    char *s;
    lineFileNext(lf, &s, NULL);
    int size = chopTabs(s, row);
    struct bedDetail *item = bedDetailLineFileLoad(row, size, lf);
    printf("  %s strand '%s'\n", item->name, item->strand);
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    printError(errCatch);
errCatchFree(&errCatch);
}

static void gffCase(char *what, char *text)
/* Parse text as an uploaded GFF custom track and print each gene's name and length. */
{
printf("%s\n", what);
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    struct customTrack *ct = customFactoryParse(db, cloneString(text), FALSE, NULL, NULL);
    struct bed *bed;
    for (bed = ct->bedList; bed != NULL; bed = bed->next)
        printf("  name %s (%d characters)\n", bed->name, (int)strlen(bed->name));
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    printError(errCatch);
errCatchFree(&errCatch);
}

int main(int argc, char *argv[])
{
chromAliasSetup(db);

bedCase("BED strand +", "chr1\t100\t200\tone\t0\t+\n");
bedCase("BED strand +1, used to overflow", "chr1\t100\t200\tone\t0\t+1\n");
bedCase("BED strand -1, used to overflow", "chr1\t100\t200\tone\t0\t-1\n");
bedCase("BED strand x, refused", "chr1\t100\t200\tone\t0\tx\n");
bedCase("BED strand xy, refused with the whole value", "chr1\t100\t200\tone\t0\txy\n");

bedDetailCase("bedDetail strand +", "chr1\t100\t200\tone\t0\t+\tid1\tdescription");
bedDetailCase("bedDetail strand +1, used to overflow", "chr1\t100\t200\tone\t0\t+1\tid1\tdescription");
bedDetailCase("bedDetail strand xy, refused with the whole value",
              "chr1\t100\t200\tone\t0\txy\tid1\tdescription");

char longName[201];
memset(longName, 'm', 200);
longName[200] = 0;
struct dyString *gff = dyStringNew(0);
dyStringPrintf(gff, "chr1\tfimo\texon\t100\t200\t.\t+\t.\tshortGene\n");
gffCase("GFF group column short", gff->string);
dyStringClear(gff);
dyStringPrintf(gff, "chr1\tfimo\texon\t100\t200\t.\t+\t.\t%s\n", longName);
gffCase("GFF group column 200 characters, used to overflow at 128", gff->string);
dyStringClear(gff);
dyStringPrintf(gff, "chr1\tfimo\texon\t100\t200\t.\t+\t.\tcut;%s\n", longName);
gffCase("GFF group column cut at the first ;", gff->string);
return 0;
}
