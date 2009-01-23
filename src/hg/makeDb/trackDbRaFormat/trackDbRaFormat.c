/* trackDbRaFormat - read trackDb.ra file and format with standard whitespace */
#include "common.h"
#include "linefile.h"
#include "options.h"
#include "ra.h"
#include "hash.h"

static char const rcsid[] = "$Header: /projects/compbio/cvsroot/kent/src/hg/makeDb/trackDbRaFormat/trackDbRaFormat.c,v 1.2 2009/01/23 23:42:39 kate Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "trackDbRaFormat - Format trackDb.ra canonically.\n\n"
  "usage:\n"
  "   trackDbRaFormat in.ra out.ra\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void trackDbRaFormat(char *inFile, char *outFile)
/* Output ra stanzas as lines */
{
struct lineFile *lf = lineFileOpen(inFile, TRUE);
FILE *of = mustOpen(outFile, "w");
char *line, *start;
int indent = 0;
char *words[1024];
int ct;
char *trackName;
struct hash *ra;
char *setting;
boolean stanza = FALSE;
int i;

/* load all ra stanzas into hash so we can lookup type while
 * processing line by line */

struct hash *raHash = raReadAll(inFile, "track");

while (lineFileNext(lf, &line, NULL))
    {
    start = skipLeadingSpaces(line);
    if (*start == 0)
        {
        /* empty line */
        fputc('\n', of);
        stanza = FALSE;
        }
    else if (startsWith("#", start))
        {
        /* comment */
        fputs(line, of);
        fputc('\n', of);
        }
    else if (!stanza)
        {
        /* first line in ra stanza -- should be track line */
        stanza = TRUE;
        if (differentString(nextWord(&line), "track"))
            errAbort("%s: not a trackDb.ra file", inFile);
        trackName = nextWord(&line);
        ra = (struct hash *)hashMustFindVal(raHash, trackName);

        /* determine  indent based on track type */
        indent = 0;
        if (hashFindVal(ra, "subTrack"))
            indent = 4;
        else 
            {
            setting = hashFindVal(ra, "superTrack");
            if (setting && sameString(setting, "on"))
                indent = 8;
            }
        spaceOut(of, indent);
        fprintf(of, "track %s\n", trackName);
        }
    else 
        {
        /* subsequent lines in ra stanza */
        /* indent properly, squeeze out extra whitespace within settings */
        spaceOut(of, indent);
        ct = chopByWhite(line, words, sizeof(words));
        for (i = 0; i < ct; i++)
            {
            fputs(words[i], of);
            if (i < ct-1)
                fputc(' ', of);
            else
                fputc('\n', of);
            }
        }
    }
fflush(of);
fclose(of);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
trackDbRaFormat(argv[1], argv[2]);
return 0;
}
