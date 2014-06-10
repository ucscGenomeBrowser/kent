/* Things to do with ENCODE 3 validation. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hex.h"
#include "linefile.h"
#include "encode3/encode3Valid.h"


char *encode3CalcValidationKey(char *md5Hex, long long fileSize)
/* calculate validation key to discourage faking of validation.  Do freeMem on 
 *result when done. */
{
if (strlen(md5Hex) != 32)
    errAbort("Invalid md5Hex string: %s\n", md5Hex);
long long sum = 0;
sum += fileSize;
while (*md5Hex)
    {
    unsigned char n = hexToByte(md5Hex);
    sum += n;
    md5Hex += 2;
    }
int vNum = sum % 10000;
char buf[256];
safef(buf, sizeof buf, "V%d", vNum);
return cloneString(buf);
}

static char *fileNameOnly(char *fullName)
/* Return just the fileName part of the path */
{
char *fileName = strrchr(fullName, '/');
if (!fileName)
    fileName = fullName;
return fileName;
}

static void requireStartEndLines(char *fileName, char *startLine, char *endLine)
/* Make sure first real line in file is startLine, and last is endLine.  Tolerate
 * empty lines and white space. */
{
char *reportFileName = fileNameOnly(fileName);
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line;

/* Get first real line and make sure it is not empty and matches start line. */
if (!lineFileNextReal(lf, &line))
    errAbort("%s is empty", reportFileName);
line = trimSpaces(line);
if (!sameString(line, startLine))
    errAbort("%s doesn't start with %s as expected", reportFileName, startLine);

boolean lastSame = FALSE;

for (;;)
    {
    if (!lineFileNextReal(lf, &line))
        break;
    line = trimSpaces(line);
    lastSame = sameString(line, endLine);
    }
if (!lastSame)
    errAbort("%s doesn't end with %s as expected", reportFileName, endLine);

lineFileClose(&lf);
}

void encode3ValidateRcc(char *path)
/* Validate a nanostring rcc file. */
{
requireStartEndLines(path, "<Header>", "</Messages>");
}

boolean fileStartsWithOneOfPair(char *fileName,  char *one, char *two)
/* Return TRUE if file starts with either one of two strings. */
{
/* Figure out size of one and two strings. */
int oneLen = strlen(one);
int twoLen = strlen(two);
int maxLen = max(oneLen, twoLen);
assert(maxLen > 0);
int bufLen = maxLen+1;
char buf[bufLen];

/* Open up file and try to read enough data */
FILE *f = fopen(fileName, "r");
if (f == NULL)
   return FALSE;
int sizeRead = fread(buf, 1, maxLen, f);
carefulClose(&f);

/* Return TRUE if we match one or two, otherwise FALSE. */
if (oneLen >= sizeRead && memcmp(buf, one, oneLen) == 0)
    return TRUE;
else if (twoLen >= sizeRead && memcmp(buf, two, twoLen) == 0)
    return TRUE;
return FALSE;
}

void encode3ValidateIdat(char *path)
/* Validate illumina idat file. */
{
if (!fileStartsWithOneOfPair(path, "IDAT", "DITA"))
    errAbort("%s is not a valid .idat file, it does not start with IDAT or DITA", fileNameOnly(path));
}

boolean encode3IsGzipped(char *path)
/* Return TRUE if file at path starts with GZIP signature */
{
FILE *f = mustOpen(path, "r");
int first = fgetc(f);
int second = fgetc(f);
carefulClose(&f);
return first == 0x1F && second == 0x8B;
}

static char *edwSupportedEnrichedIn[] = {"unknown", "exon", "intron", "promoter", "coding",
    "utr", "utr3", "utr5", "open"};
static int edwSupportedEnrichedInCount = ArraySize(edwSupportedEnrichedIn);

boolean encode3CheckEnrichedIn(char *enriched)
/* return TRUE if value is allowed */
{
return (stringArrayIx(enriched, edwSupportedEnrichedIn, edwSupportedEnrichedInCount) >= 0);
}

struct encode3BedType encode3BedTypeTable[] = {
    {"bedLogR", 9, 1},
    {"bedRnaElements", 6, 3},
    {"bedRrbs", 9, 2},
    {"bedMethyl", 9, 2},
    {"narrowPeak", 6, 4},
    {"broadPeak", 6, 3},
};

int encode3BedTypeCount = ArraySize(encode3BedTypeTable);

struct encode3BedType *encode3BedTypeMayFind(char *name)
/* Return encode3BedType of given name, just return NULL if not found. */
{
int i;
for (i=0; i<encode3BedTypeCount; ++i)
    {
    if (sameString(name, encode3BedTypeTable[i].name))
        return &encode3BedTypeTable[i];
    }
return NULL;
}

struct encode3BedType *encode3BedTypeFind(char *name)
/* Return encode3BedType of given name.  Abort if not found */
{
struct encode3BedType *bedType = encode3BedTypeMayFind(name);
if (bedType == NULL)
    errAbort("Couldn't find bed format %s", name);
return bedType;
}


