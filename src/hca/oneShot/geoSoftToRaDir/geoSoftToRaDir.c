/* geoSoftToRaDir - Convert a GEO Soft format file to a directory of several ra (flat tagStorm) 
 * files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "csv.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "geoSoftToRaDir - Convert a GEO Soft format file to a directory of several ra (flat tagStorm)\n"
  "files\n"
  "usage:\n"
  "   geoSoftToRaDir in.soft outDir\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void geoSoftToRaDir(char *inFile, char *outDir)
/* geoSoftToRaDir - Convert a GEO Soft format file to a directory of several ra (flat tagStorm) 
 * files. */
{
struct hash *outFileHash = hashNew(0);
struct lineFile *lf = lineFileOpen(inFile, TRUE);
FILE *f = NULL;
char linePrefix[128];  // Expected prefix for line, something like !Series_
char linePrefixSize = 0;
int dpPartCount = 0;

makeDirsOnPath(outDir);
char *line;
while (lineFileNext(lf, &line, NULL))
    {
    char typeChar = line[0];
    if (typeChar == '^')
        {
	if (f != NULL)
	    fputc('\n', f);
	dpPartCount = 0;

	/* Parse out first word after ^ and save lower case versions */
	line += 1;
	char *output = nextWord(&line);
	if (output == NULL)
	    errAbort("Short line %d of %s", lf->lineIx, lf->fileName);

	/* Save lower cased version of it */
	char lcOutput[128];
	safef(lcOutput, sizeof(lcOutput), "%s", output);
	tolowers(lcOutput);

	/* Get file associated with output, creating a new one if it doesn't exist */
	if ((f = hashFindVal(outFileHash, output)) == NULL)
	    {
	    char outFileName[PATH_LEN];
	    safef(outFileName, sizeof(outFileName), "%s/%s.ra", outDir, lcOutput);
	    f = mustOpen(outFileName, "w");
	    hashAdd(outFileHash, output, f);
	    }

	/* Make line prefix */
	safef(linePrefix, sizeof(linePrefix), "!%c%s_", output[0], lcOutput+1);
	linePrefixSize = strlen(linePrefix);
	}
    else if (typeChar == '!')
        {
	/* Do basic error checking of first word, and then skip over the repetitive prefix part */
	if (f == NULL)
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

	/* Write out the tag name, simple for most tags, but data_processing and 
	 * characteristics need special handling */
	if (sameString("data_processing", tag))
	    {
	    ++dpPartCount;
	    fprintf(f, "%s_%d ", tag, dpPartCount);
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
	    fprintf(f, "%s_%s ", tag, subTag);
	    }
	else
	    {
	    fprintf(f, "%s ", tag);
	    }

	/* Write out value */
	csvWriteVal(val, f);
	fputc('\n', f);
	}
    else
        errAbort("Unrecognized line %d of %s:\n%s", lf->lineIx, lf->fileName, line);
    }
if (f != NULL)
    {
    fputc('\n', f);
    carefulClose(&f);  
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
geoSoftToRaDir(argv[1], argv[2]);
return 0;
}
