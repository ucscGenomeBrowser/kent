/* Things to do with ENCODE 3 validation. */

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


