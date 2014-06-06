/* trackDbRaFormat - read trackDb.ra file and format with standard whitespace */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "options.h"
#include "ra.h"
#include "hash.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "trackDbRaFormat - Format trackDb.ra canonically.\n\n"
  "usage:\n"
  "   trackDbRaFormat in.ra out.ra\n"
  );
}

/* for checking label lengths */
#define MAX_SHORT_LABEL 17
#define MAX_LONG_LABEL 80

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
struct hash *raHash = hashNew(0);
char *setting;
boolean stanza = FALSE;
int i;
char *label;
int len;

/* load all track stanzas into hash so we can lookup type while
 * processing line by line */

while ((ra = raNextRecord(lf)) != NULL)
    {
    trackName = hashFindVal(ra, "track");
    if (trackName != NULL)
        hashAdd(raHash, trackName, ra);
    }

lineFileSeek(lf, 0, SEEK_SET);

/* read and format line by line */

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
        /* first line in ra stanza -- test for track line */
        if (differentString(nextWord(&line), "track"))
            continue;
        stanza = TRUE;
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

        /* check label lengths */
        label = hashFindVal(ra, "shortLabel");
        if (label)
            if ((len = strlen(label)) > MAX_SHORT_LABEL)
                verbose(1, "Short label '%s' too long (%d chars) for track '%s'\n", 
                                label, len-MAX_SHORT_LABEL, trackName);
        label = hashFindVal(ra, "longLabel");
        if (label)
            if ((len = strlen(label)) > MAX_LONG_LABEL)
                verbose(1, "Long label '%s' too long (%d chars) for track '%s'\n", 
                                label, len - MAX_LONG_LABEL, trackName);
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
