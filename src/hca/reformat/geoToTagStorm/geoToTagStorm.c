/* geoToTagStorm - Convert from GEO soft format to tagStorm.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "tagStorm.h"
#include "csv.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "geoToTagStorm - Convert from GEO soft format to tagStorm.\n"
  "usage:\n"
  "   geoToTagStorm geoSoftFile out.tags\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};


char *weeds[] = {
   "series.sample_id",
   "series.platform_id",
   "series.platform_taxid",
   "series.sample_taxid",
   "sample.channel_count",
};

char *subs[][2] = {
   {"series.organism", "organism"},
   {"platform.organism", "organism"},
   {"sample.organism", "organism"},
   {"series.taxid", "taxid"},
   {"platform.taxid", "taxid"},
   {"sample.taxid", "taxid"},
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
int dpPartCount = 0;
struct dyString *escaperDy = dyStringNew(0);

char *line;
while (lineFileNext(lf, &line, NULL))
    {
    char typeChar = line[0];
    char lcSection[128];
    if (typeChar == '^')
        {
	dpPartCount = 0;

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

	/* Parse out tag, get rid of repetitive "_ch1" channel prefix if it's there, check of
	 * "_ch2" and abort if it's there because can only handle one channel */
	char *tag = nextWord(&line);
	int tagLen = strlen(tag);
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
	    errAbort("Nothing after = line %d of %s", lf->lineIx, lf->fileName);
	char outputTag[256];

	/* Write out the tag name, simple for most tags, but data_processing and 
	 * characteristics need special handling */
	if (sameString("data_processing", tag))
	    {
	    ++dpPartCount;
	    safef(outputTag, sizeof(outputTag), "%s.%s_%d", lcSection, tag, dpPartCount);
	    }
	else if (sameString("characteristics", tag))
	    {
	    /* The characteristics tag has a subtag between the = and a : */
	    char *colonPos = strchr(val, ':');
	    if (colonPos == NULL)
		errAbort("No colon after %s line %d of %s", tag, lf->lineIx, lf->fileName);
	    *colonPos++ = 0;
	    char *subTag = trimSpaces(val);
	    subChar(subTag, ' ', '_');
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

void geoToTagStorm(char *inSoft, char *outTags)
/* geoToTagStorm - Convert from GEO soft format to tagStorm.. */
{
/* Read in soft file */
struct hash *softHash = geoSoftToTagHash(inSoft);
verbose(1, "Got %d types of sections in %s\n", softHash->elCount, inSoft);

/* Find database tags and append series tags to database tags. */
struct tagStorm *topTags = mustFindSection(softHash, "DATABASE", inSoft);
struct tagStanza *topStanza = topTags->forest;
struct tagStorm *seriesTags = mustFindSection(softHash, "SERIES", inSoft);
topStanza->tagList = slCat(topStanza->tagList, seriesTags->forest->tagList);

/* Find platform tags, index them */
struct tagStorm *platformTags = mustFindSection(softHash, "PLATFORM", inSoft);
struct hash *platformHash = tagStormUniqueIndex(platformTags, "platform.geo_accession");
verbose(1, "Got %d platforms\n", platformHash->elCount);

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

/* Make platform tags children of the top tag */
topStanza->children = platformTags->forest;
for (stanza = topStanza->children; stanza != NULL; stanza = stanza->next)
    stanza->parent = topStanza;

/* Weed out useless tags, and substitute others */
tagStormWeedArray(topTags, weeds, ArraySize(weeds));
tagStormSubArray(topTags, subs, ArraySize(subs));

/* Write result */
tagStormWrite(topTags, outTags, 0);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
geoToTagStorm(argv[1], argv[2]);
return 0;
}
