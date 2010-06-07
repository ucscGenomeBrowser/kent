/* gpFakeData - Generate fake data for genotype/phenotype work.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "portable.h"
#include "sqlNum.h"

static char const rcsid[] = "$Id: gpFakeData.c,v 1.1 2006/05/02 03:17:39 kent Exp $";

int subjects = 1000;
int loci = 10000;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "gpFakeData - Generate fake data for genotype/phenotype work.\n"
  "usage:\n"
  "   gpFakeData chromInfo outDir\n"
  "options:\n"
  "   -subjects=N - number of subjects (default %d)\n"
  "   -loci=N - number of genomic loci (default %d)\n"
  , subjects, loci
  );
}

static struct optionSpec options[] = {
   {"subjects", OPTION_INT},
   {"loci", OPTION_INT},
   {NULL, 0},
};

struct subject
/* Info on one subject. */
    {
    struct subject *next;
    char *name;
    boolean isSpiked;
    };

struct subject *fakeSubject(int count)
/* Generate a list of fake subjects */
{
struct subject *list = NULL, *subject;
char buf[64];
int i;
for (i=1; i<=count; ++i)
    {
    safef(buf, sizeof(buf), "s%06d", i);
    AllocVar(subject);
    subject->name = cloneString(buf);
    subject->isSpiked = (rand()&1);
    slAddHead(&list, subject);
    }
slReverse(&list);
return list;
}

char *initConst[] = 
    {
    "",
    "b",
    "br",
    "cr",
    "ch",
    "d",
    "f",
    "fr",
    "g",
    "gr",
    "j",
    "k",
    "l",
    "m",
    "n",
    "p",
    "r",
    "pr",
    "s",
    "st",
    "sh",
    "sp",
    "st",
    "t",
    "th",
    "tr",
    "v",
    "w",
    };
char *finalConst[] = {"d", "f", "g", "k", "m", "n", "p", "r", "t", "z", ""};
char *vowels[] = { "ai", "i", "ee", "o", "oo", "u"};
char *nums[] = { "", "1", "2", "3", "4", "5"};

#define randEl(a) (a[rand()%ArraySize(a)])

void fakeWord(struct dyString *buf, int syllables)
{
int i;
dyStringClear(buf);
for (i=0; i<syllables; ++i)
    {
    dyStringAppend(buf, randEl(initConst));
    dyStringAppend(buf, randEl(vowels));
    }
dyStringAppend(buf, randEl(finalConst));
}

void fakeUniqueWord(struct dyString *buf, int syllables, struct hash *uniqHash)
/* Fake a word, make sure it's unique. */
{
for (;;)
    {
    fakeWord(buf, syllables);
    if (!hashLookup(uniqHash, buf->string))
        {
	hashAdd(uniqHash, buf->string, NULL);
	return;
	}
    }
}

struct alleleFreq
/* Info on allele frequency */
    {
    char *allele;
    double freq;
    };

struct locus
/* Info on a locus */
    {
    struct locus *next;
    char *name;
    int spikeResponse;
    struct alleleFreq af[3];
    };

char *acgt[] = {"a", "c", "g", "t", "-"};

char *randomAllele()
/* Get a random allele. */
{
return randEl(acgt);
}

void randomAlleles(struct locus *locus)
/* Get random allele info */
{
int ppt = 100 + rand()%350;
double p = 1.0 - ppt*0.001, left;
left = 1.0 - p;
locus->af[0].allele = randomAllele();
locus->af[0].freq = p;
do  {
    locus->af[1].allele = randomAllele();
    } while (locus->af[1].allele == locus->af[0].allele);
if ((rand()%100) != 0)
    {
    locus->af[1].freq = left;
    }
else
    {
    double thirdRatio = (rand()%11)*0.1;
    locus->af[1].freq = left * (1.0 - thirdRatio);
    locus->af[2].freq = left * thirdRatio;
    do {
	locus->af[2].allele = randomAllele();
	} while (locus->af[2].allele == locus->af[0].allele || locus->af[2].allele == locus->af[1].allele);

    }
}

struct locus *fakeLoci(int count)
/* Return a list of fake gene names. */
{
struct locus *list = NULL, *locus;
struct hash *hash = hashNew(17);
int i;
struct dyString *dy = dyStringNew(0);
boolean spikeResponse = FALSE;
int spikesMod = count/6;
int sylCount = 2;
if (count > 1000000)
     sylCount = 3;
for (i=0; i<count; ++i)
    {
    fakeUniqueWord(dy, sylCount, hash);
    dyStringAppend(dy, randEl(nums));
    AllocVar(locus);
    locus->name = cloneString(dy->string);
    if (spikeResponse)
        spikeResponse = ((rand()&3) != 0);
    else
        spikeResponse = ((rand()%spikesMod) == 0);
    locus->spikeResponse = spikeResponse;
    randomAlleles(locus);
    slAddHead(&list, locus);
    }
return list;
}


int spikedRand(int maxVal, boolean isSpiked)
/* Get something that is either random (if not spiked) or is
 * more likely to be higher (if spiked) */
{
int ix = rand()%maxVal;
if (isSpiked)
    {
    ix += rand()%maxVal;
    ix -= maxVal - 1;
    if (ix < 0)
         ix = -ix;
    ix = maxVal - 1 - ix;
    assert(ix >= 0 && ix < maxVal);
    }
return ix;
}


#define spikedEl(a, spiked) getSpiked(a, ArraySize(a), spiked)

char *getSpiked(char **a, int aSize, boolean isSpiked)
/* Get something that is either random (if not spiked) or is
 * more likely to be higher (if spiked) */
{
return a[spikedRand(aSize, isSpiked)];
}

void saveSubjects(struct subject *list, char *fileName)
/* Save fake info on subjects */
{
struct subject *name;
FILE *f = mustOpen(fileName, "w");
fprintf(f, "#ID");
fprintf(f, "\tcontrol");
fprintf(f, "\tsex");
fprintf(f, "\tage");
fprintf(f, "\tweight");
fprintf(f, "\trace");
fprintf(f, "\tranPhe1");
fprintf(f, "\tranPhe2");
fprintf(f, "\tranPhe3");
fprintf(f, "\tranPhe4");
fprintf(f, "\tranPhe5");
fprintf(f, "\tspiPhe1");
fprintf(f, "\tspiPhe2");
fprintf(f, "\tspiPhe3");
fprintf(f, "\tspiPhe4");
fprintf(f, "\tspiPhe5");
fprintf(f, "\n");
for (name = list; name != NULL; name = name->next)
    {
    static char *sexes[] = {"M", "F"};
    int age = rand()%44 + 21;
    int weight = rand()%200 + 80;
    static char *race[] = {"Earther", "Martian", "Jovian", "Loony", "Belter"};
    static char *binary[] = {"1", "0"};
    static char *threeLevel[] = {"0", "0.5", "1"};
    static char *fiveLevel[] = {"0", "0.25", "0.50", "0.75", "1.0"};
    boolean isSpiked = (rand()&1);
    fprintf(f, "%s\t", name->name);
    fprintf(f, "%d\t", !isSpiked);
    fprintf(f, "%s\t", randEl(sexes));
    fprintf(f, "%d\t", age);
    fprintf(f, "%d\t", weight);
    fprintf(f, "%s\t", randEl(race));
    fprintf(f, "%s\t", randEl(binary));
    fprintf(f, "%s\t", randEl(threeLevel));
    fprintf(f, "%s\t", randEl(fiveLevel));
    fprintf(f, "%d\t", rand()%101);
    fprintf(f, "%0.3f\t", rand()%10000 * 0.001);
    fprintf(f, "%s\t", spikedEl(binary, isSpiked));
    fprintf(f, "%s\t", spikedEl(threeLevel, isSpiked));
    fprintf(f, "%s\t", spikedEl(fiveLevel, isSpiked));
    fprintf(f, "%d\t", spikedRand(101, isSpiked));
    fprintf(f, "%0.3f\n", spikedRand(10000, isSpiked) * 0.001);
    }
carefulClose(&f);
}

struct chrom
/* Chromosome */
    {
    struct chrom *next;
    char *name;
    int size;
    };

struct chrom *loadChromInfo(char *fileName)
/* Read in chromInfo */
{
struct chrom *list = NULL, *el;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[2];
while (lineFileRow(lf, row))
    {
    char *name = row[0];
    if (strlen(name) < 8 && !sameString(name, "chrM"))
	{
	AllocVar(el);
	el->name = cloneString(name);
	el->size = sqlUnsigned(row[1]);
	slAddHead(&list, el);
	}
    }
slReverse(&list);
return list;
}

double genomeSize(struct chrom *chromList)
/* Get total size of genome. */
{
double total = 0;
struct chrom *chrom;
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    total += chrom->size;
return total;
}


void saveLoci(struct locus *list, char *chromInfo, char *fileName)
/* Save information on genetic loci */
{
FILE *f = mustOpen(fileName, "w");
struct chrom *chromList = loadChromInfo(chromInfo), *chrom;
int chromCount = slCount(chromList);
struct locus *name;
int i;
int aveDif = genomeSize(chromList)/slCount(list);
int doubleDif = aveDif+aveDif;
int pos = 0;

fprintf(f, "#name\tchrom\tstart\tend\tmajAll\tmajFreq\tminAll\tminFreq\tisSpiked\n");
chrom = chromList;
for (name = list; name != NULL; name = name->next)
    {
    pos += rand()%doubleDif;
    if (pos > chrom->size)
         {
	 pos -= chrom->size;
	 chrom = chrom->next;
	 if (chrom == NULL)
	     chrom = chromList;
	 }
    fprintf(f, "%s\t", name->name);
    fprintf(f, "%s\t", chrom->name);
    fprintf(f, "%d\t%d\t", pos, pos+1);
    fprintf(f, "%s\t%f\t", name->af[0].allele, name->af[0].freq);
    fprintf(f, "%s\t%f\t", name->af[1].allele, name->af[1].freq);
    fprintf(f, "%d\n", name->spikeResponse);
   }
carefulClose(&f);
}

void saveData(struct subject *subjectList, struct locus *locusList, 
	char *fileName)
/* Save fake data to file */
{
FILE *f = mustOpen(fileName, "w");
struct subject *subject;
struct locus *locus;

for (locus = locusList; locus != NULL; locus = locus->next)
    {
    fprintf(f, "%s", locus->name);
    for (subject = subjectList; subject != NULL; subject = subject->next)
	{
	if (locus->spikeResponse && subject->isSpiked)
	    {
	    fprintf(f, "\t%s", locus->af[1].allele);
	    fprintf(f, "X");
	    }
	else
	    {
	    double randOff = rand()%10000 * 0.0001;
	    if ((randOff -= locus->af[0].freq) <= 0.0)
	        fprintf(f, "\t%s", locus->af[0].allele);
	    else if ((randOff -= locus->af[1].freq) <= 0.0)
	        fprintf(f, "\t%s", locus->af[1].allele);
	    else
	        fprintf(f, "\t%s", locus->af[2].allele);
	    }
	}
    fprintf(f, "\n");
    }
carefulClose(&f);
}

void gpFakeData(char *chromInfo, char *outDir)
/* gpFakeData - Generate fake data for genotype/phenotype work.. */
{
struct subject *subjectList = fakeSubject(subjects), *subject;
struct locus *locusList = fakeLoci(loci), *locus;
char locusFile[PATH_LEN], subjectFile[PATH_LEN], dataFile[PATH_LEN];
safef(locusFile, PATH_LEN, "%s/%s", outDir, "loci");
safef(subjectFile, PATH_LEN, "%s/%s", outDir, "subject");
safef(dataFile, PATH_LEN, "%s/%s", outDir, "data");
makeDir(outDir);
saveSubjects(subjectList, subjectFile);
saveLoci(locusList, chromInfo, locusFile);
saveData(subjectList, locusList, dataFile);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
subjects = optionInt("subjects", subjects);
loci = optionInt("loci", loci);
if (argc != 3)
    usage();
gpFakeData(argv[1], argv[2]);
return 0;
}
