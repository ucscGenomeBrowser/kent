/* geoToTagStorm - Convert from GEO soft format to tagStorm.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "tagStorm.h"
#include "csv.h"
#include "localmem.h"
#include "obscure.h"

boolean clExpandArrays = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "geoToTagStorm - Convert from GEO soft format to tagStorm.\n"
  "usage:\n"
  "   geoToTagStorm geoSoftFile out.tags\n"
  "options:\n"
  "   -expandArrays - if set repeated tags are kept separate with [0], [1], [2] added to them\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"expandArrays", OPTION_BOOLEAN},
   {NULL, 0},
};


struct hash *geoSoftToTagHash(char *fileName)
/* Read in file in GEO soft format and return it as a hash of tagStorm files,
 * keyed by the lower case section name, things like 'database' or 'series' */
{
struct hash *sectionHash = hashNew(0);
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct tagStorm *tags = NULL;
struct tagStanza *stanza = NULL;
char linePrefix[128];  // Expected prefix for line, something like !Series_
char linePrefixSize = 0;
struct dyString *escaperDy = dyStringNew(0);

char *line;
while (lineFileNext(lf, &line, NULL))
    {
    char typeChar = line[0];
    char lcSection[128];
    if (typeChar == '^')
        {
	/* Parse out first word after ^ and save lower case versions */
	line += 1;
	char *section = nextWord(&line);
	if (section == NULL)
	    errAbort("Short line %d of %s", lf->lineIx, lf->fileName);

	/* Save lower cased version of it */
	safef(lcSection, sizeof(lcSection), "%s", section);
	tolowers(lcSection);

	/* Get tagStanza associated with section, creating a new one if it doesn't exist */
	if ((tags = hashFindVal(sectionHash, section)) == NULL)
	    {
	    tags = tagStormNew(section);
	    hashAdd(sectionHash, section, tags);
	    }
	stanza = tagStanzaNew(tags, NULL);

	/* Make line prefix */
	safef(linePrefix, sizeof(linePrefix), "!%c%s_", section[0], lcSection+1);
	linePrefixSize = strlen(linePrefix);
	}
    else if (typeChar == '!')
        {
	/* Do basic error checking of first word, and then skip over the repetitive prefix part */
	if (tags == NULL)
	    errAbort("No ^ line before ! line - is this really a soft file?");
	if (!startsWith(linePrefix, line))
	    errAbort("Expecting line beginning with %s line %d of %s but got:\n%s", 
		linePrefix, lf->lineIx, lf->fileName, line);
	line += linePrefixSize;

	/* Parse out tag. */
	char *tag = nextWord(&line);
	int tagLen = strlen(tag);

	/* Remove _1, _2, _30 suffixes.  These will be turned into arrays later */
	char *lastUnderbar = strrchr(tag, '_');
	if (lastUnderbar != NULL && isAllDigits(lastUnderbar+1))
	    {
	    lastUnderbar[0] = 's';
	    lastUnderbar[1] = 0;
	    }

	/* Get rid of repetitive "_ch1" channel prefix if it's there, check of
	 * "_ch2" and abort if it's there because can only handle one channel */
	int channelSuffixSize = 4;
	char *channelGoodSuffix = "_ch1";
	char *channelBadSuffix = "_ch2";
	if (tagLen > channelSuffixSize)
	    {
	    char *channelSuffix = tag + tagLen - channelSuffixSize;
	    if (sameString(channelGoodSuffix, channelSuffix))
	         *channelSuffix = 0;
	    else if (sameString(channelBadSuffix, channelSuffix))
		errAbort("Can't handle multiple channel soft files, sorry");
	    }


	/* Parse out the value, which happens after '=' */
	char *equ = nextWord(&line);
	if (!sameString("=", equ))
	    errAbort("Expecting = but got %s line %d of %s", equ, lf->lineIx, lf->fileName);
	char *val = skipLeadingSpaces(line);
	if (isEmpty(val))
	    {
	    verbose(2, "Nothing after = line %d of %s", lf->lineIx, lf->fileName);
	    continue;
	    }
	char outputTag[256];

	/* Write out the tag name, simple for most tags, but data_processing and 
	 * characteristics need special handling */
	if (sameString("characteristics", tag) || sameString("relation", tag))
	    {
	    /* These tags hava a subtag between the = and a : */
	    char *colonPos = strchr(val, ':');
	    if (colonPos == NULL)
		errAbort("No colon after %s line %d of %s", tag, lf->lineIx, lf->fileName);
	    *colonPos++ = 0;
	    char *subTag = trimSpaces(val);
	    subChar(subTag, ' ', '_');
	    subChar(subTag, '-', '_');
	    val = skipLeadingSpaces(colonPos);
	    safef(outputTag, sizeof(outputTag), "%s.%s_%s", lcSection, tag, subTag);
	    }
	else
	    {
	    safef(outputTag, sizeof(outputTag), "%s.%s", lcSection, tag);
	    }

	/* Write out value */
	char *escapedVal = csvEscapeToDyString(escaperDy, val);
	tagStanzaAppend(tags, stanza, outputTag, escapedVal);
	}
    else
        errAbort("Unrecognized line %d of %s:\n%s", lf->lineIx, lf->fileName, line);
    }
lineFileClose(&lf);
return sectionHash;
}

struct tagStorm *mustFindSection(struct hash *hash, char *section, char *fileName)
/* Find given section in hash or complain about it with an error message */
{
struct tagStorm *tags = hashFindVal(hash, section);
if (tags == NULL)
    errAbort("No ^%s in %s, aborting", section, fileName);
return tags;
}


void addArrayIndexesToMultis(struct tagStorm *tagStorm, struct tagStanza *stanza)
/* Add subscript indexes to tags that occur more than once in stanza */
{
/* Make up hash of all tags and of repeated tags */
struct hash *hash = hashNew(0), *repeatedHash = hashNew(0);
struct slPair *pair;
for (pair = stanza->tagList; pair != NULL; pair = pair->next)
    {
    char *tag = pair->name;
    if (hashLookup(hash, tag) == NULL)
	hashAdd(hash, tag, NULL);
    else
        {
	hashStore(repeatedHash, tag);
	}
    }

/* Alter names of duplicates */
for (pair = stanza->tagList; pair != NULL; pair = pair->next)
    {
    char *tag = pair->name;
    struct hashEl *hel = hashLookup(repeatedHash, tag);
    if (hel != NULL)
	{
	char *val = hel->val;
	char newTag[256];
	safef(newTag, sizeof(newTag), "%s[%d]", tag, ptToInt(val));
	pair->name = lmCloneString(tagStorm->lm, newTag);
	hel->val = val+1;
	}
    }

hashFree(&repeatedHash);
hashFree(&hash);
}

void rAddArrayIndexesToMultis(struct tagStorm *tagStorm, struct tagStanza *list)
/* Add subscript indexes to tags that occur more than once in a stanza */
{
struct tagStanza *stanza;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    addArrayIndexesToMultis(tagStorm, stanza);
    rAddArrayIndexesToMultis(tagStorm, stanza->children);
    }
}

void collapseMultis(struct tagStorm *tagStorm, struct tagStanza *stanza)
/* Collapse multiply occurring tags into a single tag has a comma separated
 * list as a value.  While we're at it we'll csv escape singly occurring tags
 * as well. */
{
/* Make up hash of all tags and of repeated tags.  Repeating tags has
 * empty dyStrings as values. */
struct hash *hash = hashNew(0), *repeatedHash = hashNew(0);
struct slPair *pair;
for (pair = stanza->tagList; pair != NULL; pair = pair->next)
    {
    char *tag = pair->name;
    if (hashLookup(hash, tag) == NULL)
	hashAdd(hash, tag, NULL);
    else
        {
	struct dyString *dy = hashFindVal(repeatedHash, tag);
	if (dy == NULL)
	    {
	    dy = dyStringNew(0);
	    hashAdd(repeatedHash, tag, dy);
	    }
	}
    }
hashFree(&hash);

/* Construct comma-separated lists in repeatedHash */
for (pair = stanza->tagList; pair != NULL; pair = pair->next)
    {
    struct dyString *dy = hashFindVal(repeatedHash, pair->name);
    if (dy != NULL)
	{
	if (dy->stringSize != 0)
	     dyStringAppendC(dy, ',');
	dyStringAppend(dy, pair->val);
	}
    }

/* replace first occurence of multi-tag with one with all values, and remove
 * rest */
struct slPair *newList = NULL, *next;
for (pair = stanza->tagList; pair != NULL; pair = next)
    {
    boolean skip = FALSE;
    next = pair->next;
    char *tag = pair->name;
    struct hashEl *hel = hashLookup(repeatedHash, tag);
    if (hel != NULL)
	{
	struct dyString *dy = hel->val;
	if (dy != NULL)
	    {
	    // First time we see this tag output combined value and free
	    // up the dyString that contained it
	    pair->val = lmCloneString(tagStorm->lm, dy->string);
	    dyStringFree(&dy);
	    hel->val = NULL;
	    }
	else
	    skip = TRUE;
	}
    if (!skip)
        slAddHead(&newList, pair);
    }
slReverse(&newList);
stanza->tagList = newList;

hashFree(&repeatedHash);
}


void rCollapseMultis(struct tagStorm *tagStorm, struct tagStanza *list)
/* Collapse tags that occure more than once in a stanza to a single 
 * tag with comma separated values. */
{
struct tagStanza *stanza;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    collapseMultis(tagStorm, stanza);
    rCollapseMultis(tagStorm, stanza->children);
    }
}

void geoToTagStorm(char *inSoft, char *outTags)
/* geoToTagStorm - Convert from GEO soft format to tagStorm.. */
{
/* Read in soft file */
struct hash *softHash = geoSoftToTagHash(inSoft);
verbose(2, "Got %d types of sections in %s\n", softHash->elCount, inSoft);

/* Find series tags and make sure it just has a single one. */
struct tagStorm *seriesTags = mustFindSection(softHash, "SERIES", inSoft);
struct tagStanza *seriesStanza = seriesTags->forest;
int seriesCount = slCount(seriesStanza);
if (seriesCount != 1)
    errAbort("%s has %d ^SERIES lines, can only handle 1.", inSoft, seriesCount);

/* Find platform tags, index them */
struct tagStorm *platformTags = mustFindSection(softHash, "PLATFORM", inSoft);
struct hash *platformHash = tagStormUniqueIndex(platformTags, "platform.geo_accession");
verbose(2, "Got %d platforms\n", platformHash->elCount);

/* Find sample tags and add them as children to the appropriate platform */
struct tagStorm *sampleTags = mustFindSection(softHash, "SAMPLE", inSoft);
slReverse(&sampleTags->forest);   // Going to reverse again since building at head
struct tagStanza *stanza, *nextStanza;
for (stanza = sampleTags->forest; stanza != NULL; stanza = nextStanza)
    {
    nextStanza = stanza->next;
    char *platform = tagFindLocalVal(stanza, "sample.platform_id");
    if (platform == NULL)
	errAbort("Missing Sample_platform_id");
    struct tagStanza *platformStanza = tagStanzaFindInHash(platformHash, platform);
    if (platformStanza == NULL)
	errAbort("Can't find %s platform", platform);
    stanza->parent = platformStanza;
    slAddHead(&platformStanza->children, stanza);
    }

/* Make platform tags children of the series tag and unreverse them. */
slReverse(&platformTags->forest);
seriesStanza->children = platformTags->forest;
for (stanza = platformTags->forest; stanza != NULL; stanza = stanza->next)
    {
    stanza->parent = seriesStanza;
    slReverse(&stanza->children);
    }

/* Add array subscripts to tags that are repeated */
if (clExpandArrays)
    rAddArrayIndexesToMultis(seriesTags, seriesTags->forest);
else
    rCollapseMultis(seriesTags, seriesTags->forest);

/* Write result */
tagStormWrite(seriesTags, outTags, 0);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
clExpandArrays = optionExists("expandArrays");
geoToTagStorm(argv[1], argv[2]);
return 0;
}
