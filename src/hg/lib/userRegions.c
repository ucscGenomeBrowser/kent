/* userRegions: parse user regions entered as BED3, BED4 or chr:start-end
 * optionally followed by name. */

/* Copyright (C) 2015 The Regents of the University of California
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hui.h"
#include "linefile.h"
#include "trashDir.h"
#include "userRegions.h"

static boolean illegalCoordinate(char *db, char *chrom, int start, int end, char *line, int lineIx,
                                 struct dyString *dyWarn)
/* verify start and end are legal for this chrom */
{
int maxEnd = hChromSize(db, chrom);
if (start < 0)
    {
    dyStringPrintf(dyWarn, "line %d: '%s': chromStart (%d) less than zero\n",
                   lineIx, line, start);
    return TRUE;
    }
if (end > maxEnd)
    {
    dyStringPrintf(dyWarn, "line %d: '%s': chromEnd (%d) greater than chrom length (%s:%d)\n",
                   lineIx, line, end, chrom, maxEnd);
    return TRUE;
    }
if (start > end)
    {
    dyStringPrintf(dyWarn, "line %d: '%s': chromStart (%d) greater than chromEnd (%d)\n",
                   lineIx, line, start, end);
    return TRUE;
    }
return FALSE;
}

static struct bed4 *parseRegionInput(char *db, char *inputString, int maxRegions, int maxErrs,
                                     struct dyString *dyWarn)
/* scan the user region definition, turn into a bed list */
{
int regionCount = 0;
int errCount = 0;
struct bed4 *bedList = NULL;
struct lineFile *lf = lineFileOnString("userData", TRUE, inputString);
char *line = NULL;
while (lineFileNextReal(lf, &line))
    {
    char *chromName = NULL;
    int chromStart = 0;
    int chromEnd = 0;
    char *regionName = NULL;
    // Chop a copy of line so we can display line if there's an error.
    char copy[strlen(line)+1];
    safecpy(copy, sizeof(copy), line);
    char *words[5];
    int wordCount = chopByWhite(copy, words, ArraySize(words));
    boolean badFormat = FALSE;
    boolean gotError = FALSE;
    /*	might be something of the form: chrom:start-end optionalRegionName */
    if (((1 == wordCount) || (2 == wordCount)) &&
	    hgParseChromRange(NULL, words[0], &chromName,
		&chromStart, &chromEnd))
	{
	if (2 == wordCount)
	    regionName = cloneString(words[1]);
	}
    else if (!((3 == wordCount) || (4 == wordCount)))
	{
	dyStringPrintf(dyWarn, "line %d: '%s': "
                       "unrecognized format.  Please enter 3- or 4-column BED or "
                       "a chr:start-end position range optionally followed by a name.\n",
                       lf->lineIx, line);
        badFormat = TRUE;
        gotError = TRUE;
	}
    else
	{
	chromName = words[0];
        // Make sure chromStart and chromEnd are numbers
        if (!isNumericString(words[1]))
            {
            dyStringPrintf(dyWarn, "line %d: '%s': chromStart must be a number but is '%s'\n",
                           lf->lineIx, line, words[1]);
            gotError = TRUE;
            }
        if (!isNumericString(words[2]))
            {
            dyStringPrintf(dyWarn, "line %d: '%s': chromEnd must be a number but is '%s'\n",
                           lf->lineIx, line, words[2]);
            gotError = TRUE;
            }
        if (! gotError)
            {
            chromStart = atoi(words[1]);
            chromEnd = atoi(words[2]);
            if (wordCount > 3)
                regionName = cloneString(words[3]);
            }
	}
    char *officialChromName = chromName ? hgOfficialChromName(db, chromName) : NULL;
    if (! badFormat)
        {
        if (NULL == officialChromName)
            {
            dyStringPrintf(dyWarn,
                           "line %d: '%s': chrom name '%s' not recognized in this assembly\n",
                           lf->lineIx, line, chromName ? chromName : words[0]);
            gotError = TRUE;
            }
        else if (illegalCoordinate(db, officialChromName, chromStart, chromEnd, line, lf->lineIx,
                                   dyWarn))
            {
            gotError = TRUE;
            }
        }
    if (gotError)
        {
        errCount++;
        if (errCount > maxErrs && maxErrs > 0)
            {
            dyStringPrintf(dyWarn, "Exceeded maximum number of errors (%d), quitting\n", maxErrs);
            break;
            }
        else
            continue;
        }
    ++regionCount;
    if (regionCount > maxRegions && maxRegions > 0)
	{
	dyStringPrintf(dyWarn,
                       "line %d: limit of %d region definitions exceeded, skipping the rest\n",
                       lf->lineIx, maxRegions);
	break;
	}
    struct bed4 *bedEl = bed4New(officialChromName, chromStart, chromEnd, regionName);
    slAddHead(&bedList, bedEl);
    }
lineFileClose(&lf);
// Keep regions in same order as user entered them:
slReverse(&bedList);
return (bedList);
}

char *userRegionsParse(char *db, char *regionsText, int maxRegions, int maxErrs,
                       int *retRegionCount, char **retWarnText)
/* Parse user regions entered as BED3, BED4 or chr:start-end optionally followed by name.
 * Return name of trash file containing BED for parsed regions if regionsText contains
 * valid regions; otherwise NULL.
 * If maxRegions <= 0, it is ignored; likewise for maxErrs.
 * If retRegionCount is non-NULL, it will be set to the number of valid parsed regions
 * in the trash file.
 * If retWarnText is non-NULL, it will be set to a string containing warning and error
 * messages encountered while parsing input. */

{
char *trashFileName = NULL;
if (isNotEmpty(regionsText))
    {
    char *copy = cloneString(regionsText);
    struct dyString *dyWarn = dyStringNew(0);
    struct bed4 *bedList = parseRegionInput(db, copy, maxRegions, maxErrs, dyWarn);
    if (retWarnText != NULL)
        {
        if (dyWarn->stringSize > 0)
            *retWarnText = dyStringCannibalize(&dyWarn);
        else
            {
            *retWarnText = NULL;
            dyStringFree(&dyWarn);
            }
        }
    int regionCount = slCount(bedList);
    if (retRegionCount != NULL)
        *retRegionCount = regionCount;
    if (regionCount > 0)
        {
        struct tempName tn;
        trashDirFile(&tn, "hgtData", "user", ".region");
        trashFileName = cloneString(tn.forCgi);
        FILE *f = mustOpen(trashFileName, "w");
        struct bed4 *bed;
        for (bed = bedList; bed; bed = bed->next )
            {
            char *name = bed->name ? bed->name : "";
            fprintf(f, "%s\t%d\t%d\t%s\n",
                    bed->chrom, bed->chromStart, bed->chromEnd, name);
            }
        carefulClose(&f);
        }
    freeMem(copy);
    }
else
    {
    if (retRegionCount != NULL)
        *retRegionCount = 0;
    if (retWarnText != NULL)
        *retWarnText = NULL;
    }
return trashFileName;
}
