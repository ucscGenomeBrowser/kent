/* This program retrieves records from a GenBank file. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "options.h"
#include "linefile.h"
#include "dystring.h"
#include "hash.h"
#include "verbose.h"


/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"accFile", OPTION_STRING},
    {"missingOk", OPTION_BOOLEAN},
    {NULL, 0}
};

void usage()
/* Explain usage and exit. */
{
errAbort(
"gbGetEntries - retrieve records from a GenBank flat file.\n"
"usage:\n"
"  gbGetEntries [options] gbFile [acc ...]\n"
"\n"
"The output will be printed to standard out\n"
"Either acc or acc.version maybe specified\n"
"\n"
"Options:\n"
"   -accFile=file - retrieve accessions in this file\n"
"   -missingOk - don't complain if all accessions are not found\n"
);
}

void unexpectedEof(struct lineFile *lf)
/* Complain about unexpected EOF and die. */
{
errAbort("Unexpected end of file line %d of %s", lf->lineIx, lf->fileName);
}

void invalidVersionLine(struct lineFile *lf)
/* Complain about an invalid version and die. */
{
errAbort("invalid VERSION line at %d of %s", lf->lineIx, lf->fileName);
}

boolean seekLocus(struct lineFile *lf)
/* search for locus line */
{
char *line;
while (TRUE)
    {
    if (!lineFileNext(lf, &line, NULL))
        return FALSE;
    if (startsWith("LOCUS", line))
        {
        lineFileReuse(lf);
        return TRUE;
        }
    }
}

void processEntry(struct lineFile *lf, struct hash *accTbl, 
                     struct dyString* headLines, FILE *out)
/* process one genbank entry. */
{
char *line;
char *words[3];
int numWords;
struct hashEl *accHel;

dyStringClear(headLines);

/* scan to VERSION lines */
while (TRUE)
    {
    if (!lineFileNext(lf, &line, NULL))
        unexpectedEof(lf);
    /* save line */
    dyStringAppend(headLines, line);
    dyStringAppendC(headLines, '\n');
    if (startsWith("VERSION ", line))
        break;
    if (startsWith("//", line))
        errAbort("LOCUS without VERSION at line %d of %s",
                 lf->lineIx, lf->fileName);
    }

/* parse VERSION line */
numWords = chopByWhite(line, words, ArraySize(words));
if (numWords < 3)
    invalidVersionLine(lf);

/* check with acc.ver */
accHel = hashLookup(accTbl, words[1]);
if (accHel == NULL)
    {
    /* check with just acc */
    char *dotPtr = strrchr(words[1], '.');
    if (dotPtr == NULL)
        invalidVersionLine(lf);
    *dotPtr = '\0';
    accHel = hashLookup(accTbl, words[1]);
    }

if (accHel != NULL)
    fputs(headLines->string, out);  /* output save lines */

/* scan to end of record, maybe outputting if selected */
while (TRUE)
    {
    if (!lineFileNext(lf, &line, NULL))
        unexpectedEof(lf);
    
    if (accHel != NULL)
        {
        fputs(line, out);
        fputc('\n', out);
        }
    if (startsWith("//", line))
        break;
    }

/* flag match as found */
if (accHel != NULL)
    accHel->val = (void*)TRUE;

}

static void addAcc(struct hash *accTbl, char *acc)
/* add an accession to a table, make sure it's only stored once */
{
struct hashEl *hel = hashStore(accTbl, acc);
hel->val = (void*)FALSE;
}

void loadAccs(char *accFile, struct hash *accTbl)
/* load accessions from a file */
{
struct lineFile *lf = lineFileOpen(accFile, TRUE);
char *line;
while (lineFileNextReal(lf, &line))
    addAcc(accTbl, trimSpaces(line));

lineFileClose(&lf);
}

void checkMissedAccs(struct hash *accTbl)
/* check for accessions that were not found */
{
int numMissed = 0;
struct hashCookie cookie = hashFirst(accTbl);
struct hashEl *accHel;
while ((accHel = hashNext(&cookie)) != NULL)
    {
    if (!accHel->val)
        {
        if (numMissed == 0)
            fprintf(stderr, "Error: Missing entries:\n");
        fprintf(stderr, "\t%s\n", accHel->name);
        numMissed++;
        }
    }
if (numMissed > 0)
    errAbort("%d accessions were not found", numMissed);
}

void extractAccsFromGb(char *gbFile, boolean missingOk, char *accFile,
                       char **accNames, int accCount)
/* Parse records of genBank file and print ones that match accession names. */
{
FILE *out = stdout;
struct hash *accTbl = hashNew(0);
struct lineFile *lf;
struct dyString* headLines = dyStringNew(4096);
int i;

if (accFile != NULL)
    loadAccs(accFile, accTbl);
for (i = 0; i < accCount; i++)
    addAcc(accTbl, accNames[i]);

/* process file */
lf = lineFileOpen(gbFile, TRUE);
while (seekLocus(lf))
    processEntry(lf, accTbl, headLines, out);

lineFileClose(&lf);

/* flush and check for errors */
carefulClose(&out);

if (!missingOk)
    checkMissedAccs(accTbl);
}

int main(int argc, char *argv[])
{
optionInit(&argc, argv, optionSpecs);
if (argc < 2)
    usage();
extractAccsFromGb(argv[1], optionExists("missingOk"),
                  optionVal("accFile", NULL), argv+2, argc-2);
return 0;
}
