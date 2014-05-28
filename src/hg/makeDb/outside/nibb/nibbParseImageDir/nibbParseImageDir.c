/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

/* NOTE!!! 
 
 last time we got files with embedded spaces in them.
 /san/sanvol1/visiGene/gbdb/full/inSitu/XenopusLaevis/nibb/XL013p16_W-2-1b10_r45_28/XL013p16_ St17_dorsal.jpg
 There is a space just before "St17_dorsal.jpg" and there were several other files like this that 
 had to be manually fixed.

 Also, for next time we should make the structure be the same.
 Right now, there are 4 extra directory levels in offline/nibbFrog (XL001-018  XL019-043  XL044-095  XL095-500)
 which are NOT reflected under visiGene/gbdb/{full|200}/inSitu/XenopusLaevis/nibb/
 Had we made the structure the same, then we could use the standard vgPrepImage utility,
 and we wouldn't need the nibbPrepImage utility at all.

 I may just go ahead and get rid of (XL001-018  XL019-043  XL044-095  XL095-500) 
 and put all their contents into their parent dir offline/nibbFrog/.
 
*/


/* nibbParseImageDir - Look through nibb image directory and allowing for 
 * typos and the like create a table that maps a file name to clone name, 
 * developmental stage, and view of body part. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "nibbParseImageDir - Look through nibb image directory and allowing for typos and the like create a table that maps a file name to clone name, developmental stage, and view of body part\n"
  "usage:\n"
  "   nibbParseImageDir sourceDir good.tab bad.tab\n"
  "The output files, good and bad.tab, will contain 6 tab separated\n"
  "columns:\n"
  "  cloneName - XDB clone name (starts with XL)\n"
  "  stage - developmental stage (starts with St, or is 'mixed')\n"
  "  view - body part or view for whole embryo shots\n"
  "  dir - directory (under source directory) of image\n"
  "  subDir - subdirectory\n"
  "  fileName - file name\n"
  "The good.tab contains all the images where it could figure out the stage,\n"
  "view, and clone name from the file name.  The bad.tab contains cases\n"
  "where it couldn't.  Currently the program seems to be working well enough\n"
  "that bad.tab is empty.  (And it only took 600 lines of code....)\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct imageInfo
    {
    struct imageInfo *next;	/* Next in list. */
    char *clone;		/* XL name of clone. */
    char *stage;		/* Stage name. */
    char *view;			/* View. */
    char *dir;			/* Directory. */
    char *subDir;		/* Subdirectory. */
    char *file;			/* File name */
    };

boolean isAllish(char *s)
/* Return TRUE if s is all or something like that. */
{
return (sameWord(s, "all") || sameWord(s, "all1") 
	|| sameWord(s, "all2") || sameWord(s, "all3")
	|| sameWord(s, "al"));
}

void setAllishStages(char *s, struct imageInfo *image)
/* Set stage and view of image if s looks like 'all' */
{
if (isAllish(s))
    {
    image->stage = "mixed";
    image->view = "mixed";
    }
}

void stripSuffix(char *s, char *suffix)
/* Strip suffix if it is present in s */
{
if (endsWith(s, suffix))
    {
    int suffLen = strlen(suffix);
    int sLen = strlen(s);
    s[sLen - suffLen] = 0;
    }
}

void setStage(struct imageInfo *image, char *flakyClone, char *flakyStage)
/* Set image->stage from stage name if it looks good. */
{
flakyStage = skipLeadingSpaces(flakyStage);

/* Strip out -all suffix if any. */
stripSuffix(flakyStage, "-all");
stripSuffix(flakyStage, "-2");
stripSuffix(flakyStage, "-12");

if (startsWith("St", flakyStage))
    {
    char *num = flakyStage+2;
    int numSize = strlen(num);
    if (isdigit(num[0]) && isdigit(num[1]))
        {
	if (numSize == 2 || 
		(numSize == 4 && num[2] == '.' && isdigit(num[3])))
	    image->stage = flakyStage;
	}
    }
if (image->stage == NULL)
    {
    if (sameString(flakyStage, "St105"))
	image->stage = "St10.5";
    else if (sameString(flakyStage, "St8"))
	image->stage = "St8";
    else if (sameString(flakyStage, "St9"))
	image->stage = "St9";
    else if (sameString(flakyStage, "St10-1"))
	image->stage = "St10";
    else if (sameString(flakyStage, "St12a"))
	image->stage = "St12";
    else if (sameString(flakyStage, "S20"))
	image->stage = "St20";
    else if (sameString(flakyStage, "S24"))
	image->stage = "St24";
    else if (sameString(flakyStage, "S25"))
	image->stage = "St25";
    }
}

void upperUnvowel(char *full, char *stripped, int maxStrippedSize)
/* Convert full to stripped by getting rid of vowels and numbers
 * and upper casing. */
{
char c;
int strippedSize = 0;
while ((c = *full++) != 0)
    {
    c = toupper(c);
    if (isalpha(c))
	{
	if (c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U')
	    ;
	else
	    {
	    stripped[strippedSize] = c;
	    strippedSize += 1;
	    if (strippedSize >= maxStrippedSize)
		internalErr();
	    }
	}
    }
stripped[strippedSize] = 0;
}

boolean sameConsonants(char *a, char *b)
/* Return TRUE if a and b are the same ignoring vowels. */
{
char aStripped[256], bStripped[256];
upperUnvowel(a, aStripped, sizeof(aStripped));
upperUnvowel(b, bStripped, sizeof(bStripped));
return sameString(aStripped, bStripped);
}

boolean isAnimal(char *s)
/* Return TRUE if name looks like animal or a misspelling thereof */
{
return sameConsonants(s, "animal")
	|| sameString(s, "aninmal")
	|| sameString(s, "animnal")
	|| sameString(s, "animall")
	|| sameString(s, "animal_trabs")
	|| sameString(s, "animals")
	;
}

boolean isAnterior(char *s)
/* Return TRUE if name looks like anterior or a misspelling thereof. */
{
return sameConsonants(s, "anterior")
	|| sameString(s, "anteior")
	|| sameString(s, "antrrior")
	|| sameString(s, "anteriors")
	|| sameString(s, "anterrior")
	;
}

boolean isBody(char *s)
/* Return TRUE if name looks like body. */
{
return sameString(s, "body")
	|| sameString(s, "body2");
}

boolean isDorsal(char *s)
/* Return TRUE if name looks like dorsal or a misspelling thereof. */
{
return sameConsonants(s, "dorsal")
	|| sameString(s, "dordsal")
	|| sameString(s, "dorsall")
	|| sameString(s, "dorsdal")
	|| sameString(s, "dosal")
	|| sameString(s, "doesal")
	|| sameString(s, "dosal2")
	|| sameString(s, "dorsalbody")
	|| sameString(s, "dorsalanterior")
	|| sameString(s, "dorsalposterior")
	|| startsWith("dorsal_trans", s)
	|| startsWith("dorslateral_trans", s)
	;
}

boolean isHead(char *s)
/* Return TRUE if name looks like head or a misspelling thereof. */
{
return sameString(s, "head")
	|| sameString(s, "head2")
	|| sameString(s, "head_2")
	|| sameString(s, "head")
	|| sameString(s, "dorsalhead")
	|| sameString(s, "dorsal_head")
	|| sameString(s, "ventralhead")
	;;
}

boolean isLateral(char *s)
/* Return TRUE if name looks like lateral or a misspelling thereof. */
{
return sameConsonants(s, "lateral")
	|| sameString(s, "laterarl")
	|| sameString(s, "ateral")
	|| sameString(s, "lateal")
	|| sameString(s, "latera2")
	|| sameString(s, "lateraljpg")
	|| sameString(s, "lateral2_left")
	|| sameString(s, "lateral2_right")
	|| sameString(s, "lateral_left")
	|| sameString(s, "lateral_right")
	|| sameString(s, "lateral_l")
	|| sameString(s, "lateral_r")
	|| sameString(s, "lateral_l")
	|| sameString(s, "lateral_r")
	|| sameString(s, "all_lateral")
	|| startsWith("lateral_trans", s)
	|| startsWith("lateral2_trans", s)
	;
}

boolean isPosterior(char *s)
/* Return TRUE if name looks like posterior or a misspelling thereof. */
{
return sameConsonants(s, "posterior")
	|| sameString(s, "posteriorl")
	|| sameString(s, "poterior")
	|| sameString(s, "posteior")
	|| sameString(s, "postrerior")
	|| sameString(s, "poaterior")
	|| sameString(s, "popsterior")
	|| sameString(s, "posterrior")
	;
}

boolean isSection(char *s)
/* Return TRUE if name looks like section of a misspelling thereof. */
{
return sameConsonants(s, "section")
	|| sameString(s, "_AVsection")
	|| sameString(s, "_AVsection2")
	|| startsWith("lateral_section", s)
	;
}

boolean isTail(char *s)
/* Return TRUE if name looks like tail or a misspelling thereof. */
{
return sameString(s, "tail")
	| sameString(s, "dorsaltail")
	| sameString(s, "dorsal_tail")
	| sameString(s, "ventraltail");
}

boolean isVentral(char *s)
/* Return TRUE if name looks like ventral or a misspelling thereof. */
{
return sameConsonants(s, "ventral") 
	|| sameString(s, "vetntral")
	|| sameString(s, "venral")
	|| sameString(s, "vetral")
	|| sameString(s, "ventra")
	|| sameString(s, "vental")
	|| sameString(s, "ventrtal")
	|| sameString(s, "ventralposterior")
	|| sameString(s, "ventral_posterior")
	|| sameString(s, "ventralanterior")
	;
}

boolean isVegetal(char *s)
/* Return TRUE if name looks like vegetal or a misspelling thereof */
{
return sameConsonants(s, "vegetal") 
	|| sameString(s, "vegetall")
	|| sameString(s, "vegitall")
	|| sameString(s, "vegittal")
	|| sameString(s, "vegial")
	|| sameString(s, "all_vegetal")
	|| startsWith("vegetal_trans", s)
	;
}

void setView(struct imageInfo *image, char *flakyClone, char *flakyStage,
	char *flakyView)
/* Set image->view from view name if it looks good. */
{
if (startsWith("2_", flakyView))
    flakyView += 2;
if (isAllish(flakyView))
    image->view = "mixed";
else if (isAnimal(flakyView))
    image->view = "animal";
else if (isAnterior(flakyView))
    image->view = "anterior";
else if (isBody(flakyView))
    image->view = "whole";
else if (isDorsal(flakyView))
    image->view = "dorsal";
else if (isTail(flakyView))
    image->view = "tail";
else if (isLateral(flakyView))
    image->view = "lateral";
else if (isPosterior(flakyView))
    image->view = "posterior";
else if (isSection(flakyView))
    image->view = "section";
else if (isVentral(flakyView))
    image->view = "ventral";
else if (isVegetal(flakyView))
    image->view = "vegetal";
else if (isHead(flakyView))
    image->view = "head";
}

struct imageInfo *getImageInfo(struct hash *fixHash,
	char *cloneName, char *dir, char *subDir, char *file,
	struct hash *stageHash, struct hash *viewHash,
	struct hash *otherHash, struct hash *probeHash)
/* Work on a single image file. */
{
char *fixedFile;
char *dupeFileName;
char *s, *e;
struct imageInfo *image;

/* Allocate imageInfo and save stuff that requires no
 * correction. */
AllocVar(image);
image->clone = cloneName;
image->dir = dir;
image->subDir = subDir;
image->file = file;

/* Do the straight lookup type correction. */
fixedFile = hashFindVal(fixHash, file);
if (fixedFile != NULL)
    file = fixedFile;

/* Get a copy of file name to work on as we chew
 * through the correction process. */
dupeFileName = cloneString(file);
s = dupeFileName;

/* Get rid of .jpg suffix, we don't care. */
chopSuffix(s);

/* A lot of them end with trans or sence_trans, which
 * refers I think to some varients in the preparation.
 * We'll just skip this part if it's there. */
stripSuffix(s, "_trans");
stripSuffix(s, "_sence");

/* Generally the rest of the file name is broken up into
 * parts by underbars.  We'll use the e and s variables
 * to point to the beginning and end of the current underbar
 * separated word. */
e = strchr(s, '_');

if (e == NULL)
    {
    /* Here there is just a single word.  Usually it
     * is "all" which we'll interpret as meaning the photo
     * is a mixture of developmental stages and views. 
     * Sometimes it will be the clone name followed by all. */
    hashStore(otherHash, s);
    if (startsWith("XL", s) && sameString(s+8, "all"))
        s += 8;
    setAllishStages(s, image);
    }
else
    {
    /* The clone name from the file name is less reliable
     * than the clone name from the directory, so mostly we'll
     * just skip it.  Sometimes we'll have to use it to dig out
     * the stage name when they forgot the underbar. */
    char *flakyClone = s;
    char *flakyStage = NULL, *flakyView = NULL;

    *e++ = 0;
    if (startsWith("XL178i11St", flakyClone))
	{
	/* This guy forgot the underbar. */
	s += 8;
	}
    else
	s = e;
    if (*s == '_')	/* Sometimes they mess up and put in 2 underbars. */
       ++s;
    else if (startsWith("S_", s))	/* Or put in a gratuitious S_ */
       s += 2;
    else if (startsWith("AS_", s))	/* Or put in a gratuitious AS_ */
       s += 3;
    flakyStage = s;

    /* At this point flakyStage (and s) point to a part of the
     * file name that contains the developmental stage, which
     * is normally in the format "St" followed by a number.
     * However there's a chance there will just be the word
     * all here, which again we interpret as the photo being
     * mixed stages. */
    e = strchr(s, '_');
    if (e == NULL)
	{
        hashStore(otherHash, s);
	setAllishStages(s, image);
	if (image->stage == NULL)
	    {
	    setStage(image, flakyClone, flakyStage);
	    image->view = "mixed";
	    }
	}
    else
        {
	*e++ = 0;
	hashStore(stageHash, s);
	setStage(image, flakyClone, flakyStage);

	s = e;
	flakyView = s;
	hashStore(viewHash, s);
	setView(image, flakyClone, flakyStage, flakyView);

	}
    }
return image;
}

void printHash(char *label, struct hash *hash)
/* Print out keys in hash alphabetically. */
{
struct hashEl *list, *el;
list = hashElListHash(hash);
slSort(&list, hashElCmp);
printf("%s:\n", label);
for (el = list; el != NULL; el = el->next)
    printf("    %s\n", el->name);
hashElFreeList(&list);
}

char *skipDir[] = 
/* List of directories to skip, where we can't find
 * real probe name. */
    {
    "XL23o20_RA030h23_r98_16",
    "XL23o21_RA025p11_r98_17",
    "XL25o04_RA025h17_r98_29",
    "XL36o14_RA025_4b07_r123_3",
    };

char *skipFile[] =
/* List of files to skip, where it is hard to parse and
 * image looks dubious. */
    {
    "XL019m15_St25_somite.jpg",
    };

char *fixPairs[] =
/* List of fixed file names.  Things that are rare enough that we
 * handle them individually. */
    {
    "XL023m20St12_animal.jpg", "XL023m20_St12_animal.jpg",
    "XL033n19_St15_2.jpg", "XL033n19_St15_all.jpg",
    "XL036j02_St2-20_lateral2.jpg", "XL036j02_St20_lateral2.jpg",
    "XL047l17_St121_lateral.jpg", "XL047l17_St21_lateral.jpg",
    "XL048f17_St120_ventral.jpg", "XL048f17_St12_ventral.jpg",
    "XL053o21_St30_trans1.jpg", "XL053o21_St30_all1.jpg",
    "XL053o21_St30_trans2.jpg", "XL053o21_St30_all2.jpg",
    "XL056f17_St27dorsal.jpg", "XL056f17_St27_dorsal.jpg",
    "XL058o01_St12 _vegetal.jpg", "XL058o01_St12_vegetal.jpg",
    "XL059k16_all_trans.jpg", "XL059k16_all.jpg",
    "XL085d10_St20_all_lateral.jpg", "XL085d10_St20_lateral.jpg",
    };

struct hash *hashFixers()
/* Make hash of file names keyed by name that needs
 * fixing and with value of fixed names. */
{
int i;
struct hash *hash = hashNew(0);
for (i=0; i<ArraySize(fixPairs); i += 2)
    {
    hashAdd(hash, fixPairs[i], fixPairs[i+1]);
    }
return hash;
}

void imageInfoOut(struct imageInfo *image, FILE *f)
/* Write out image info. */
{
fprintf(f, "%s\t%s\t%s\t%s\t%s\t%s\n", image->clone, image->stage,
	image->view, image->dir, image->subDir, image->file);
}

void nibbParseImageDir(char *sourceDir, char *goodTab, char *badTab)
/* nibbParseImageDir - Look through nibb image directory and allowing for 
 * typos and the like create a table that maps a file name to clone name, 
 * developmental stage, and view of body part. */
{
struct fileInfo *l1List, *l1, *l2List, *l2, *l3List, *l3;
struct hash *stageHash = hashNew(0);
struct hash *viewHash = hashNew(0);
struct hash *otherHash = hashNew(0);
struct hash *probeHash = hashNew(0);
struct hash *fixHash = hashFixers();
struct imageInfo *imageList = NULL, *image;
FILE *good = mustOpen(goodTab, "w");
FILE *bad = mustOpen(badTab, "w");
int goodCount = 0, badCount = 0;
int jpgCount = 0, jpgDir = 0;


l1List = listDirX(sourceDir, "XL*", FALSE);
for (l1 = l1List; l1 != NULL; l1 = l1->next)
    {
    char l1Path[PATH_LEN];
    safef(l1Path, sizeof(l1Path), "%s/%s", sourceDir, l1->name);
    l2List = listDirX(l1Path, "XL*", FALSE);
    for (l2 = l2List; l2 != NULL; l2 = l2->next)
        {
	char l2Path[PATH_LEN];
	char cloneName[64], *permanentCloneName;
	char *cloneDir = l2->name;
	char *cloneEnd;
	int cloneNameSize = 0;

	if (stringIx(cloneDir, skipDir) >= 0)
	    continue;

	/* Figure out clone name, whish is directory component up to
	 * first underbar. */
	cloneEnd = strchr(cloneDir, '_');
	if (cloneEnd != NULL)
	    cloneNameSize = cloneEnd - cloneDir;
	else
	    errAbort("Strangely formatted image dir %s, no underbar", cloneDir);
	if (cloneNameSize >= sizeof(cloneName))
	    errAbort("Clone name too long in dir %s", cloneDir);
	if (cloneNameSize < 8 || cloneNameSize > 12)
	    errAbort("Clone name wrong size %s", cloneDir);
	memcpy(cloneName, cloneDir, cloneNameSize);
	cloneName[cloneNameSize] = 0;
	/* Check format is XL###L##.  We already checked the XL. */
	if (!isdigit(cloneName[2]) || !isdigit(cloneName[3]) 
		 || !isdigit(cloneName[4]) || isdigit(cloneName[5]) 
		 || !isdigit(cloneName[6]) || !isdigit(cloneName[7]))
	    errAbort("Strangely formatted clone name %s", cloneDir);

	permanentCloneName = hashStoreName(probeHash, cloneName);


	/* Get all files in dir. */
	safef(l2Path, sizeof(l2Path), 
		"%s/%s/%s", sourceDir, l1->name, l2->name);
	l3List = listDirX(l2Path, "*.jpg", FALSE);
	for (l3 = l3List; l3 != NULL; l3 = l3->next)
	    {
	    char *fileName = l3->name;

	    if (stringIx(l3->name, skipFile) >= 0)
		continue;
	    image = getImageInfo(fixHash, permanentCloneName, 
	    	l1->name, cloneDir, fileName,
	    	stageHash, viewHash, otherHash, probeHash);
	    slAddHead(&imageList, image);
	    ++jpgCount;
	    }
	++jpgDir;
	}
    }
slReverse(&imageList);

verbose(1, "%d jpg images in %d directories\n", jpgCount, jpgDir);

#ifdef OLD
verbose(1, "%d probes, %d stages, %d views, %d other\n", 
	probeHash->elCount, stageHash->elCount, 
	viewHash->elCount, otherHash->elCount);
printHash("stages", stageHash);
printHash("views", viewHash);
printHash("other", otherHash);
#endif /* OLD */

for (image = imageList; image != NULL; image = image->next)
    {
    if (image->clone != NULL && image->stage != NULL && image->view != NULL)
        {
	imageInfoOut(image, good);
	++goodCount;
	}
    else
	{
	imageInfoOut(image, bad);
	++badCount;
	}
    }
verbose(1, "%d (%4.1f%%) parsed ok, %d (%4.2f%%) didn't\n", 
	goodCount, 100.0 * goodCount/(goodCount + badCount), 
	badCount, 100.0 * badCount/(goodCount + badCount));
carefulClose(&good);
carefulClose(&bad);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
nibbParseImageDir(argv[1], argv[2], argv[3]);
return 0;
}
