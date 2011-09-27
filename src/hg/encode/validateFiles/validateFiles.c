#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "chromInfo.h"
#include "jksql.h"
#include "twoBit.h"
#include "dnaseq.h"
#include "hgBam.h"
#include "bbiFile.h"
#include "bigWig.h"
#include "errCatch.h"
#include "obscure.h"
#include "hmmstats.h"

char *version = "4.3";

#define MAX_ERRORS 10
#define PEAK_WORDS 16
#define TAG_WORDS 9
#define QUICK_DEFAULT 1000

enum bedType {BED_GRAPH = 0, BROAD_PEAK, NARROW_PEAK, GAPPED_PEAK};

int maxErrors;
boolean colorSpace;
boolean zeroSizeOk;
boolean printOkLines;
boolean printFailLines;
boolean mmPerPair;
boolean nMatch;
boolean isSort;
boolean privateData;
boolean allowOther;
boolean allowBadLength;
boolean complementMinus;
int quick;
struct hash *chrHash = NULL;
char dnaChars[256];
char qualChars[256];
char csQualChars[256];
char seqName[256];
char digits[256];
char alpha[256];
char csSeqName[256];
char bedTypeCols[10];
struct twoBitFile *genome = NULL;
int mismatchTotalQuality;
int mismatches;
int matchFirst=0;
int mmCheckOneInN;
int allowErrors = 0;
boolean doReport;
double bamPercent = 0.0;
boolean showBadAlign;

void usage()
/* Explain usage and exit. */
{
  errAbort(
  "validateFiles - Validate format of different track input files\n"
  "                Program exits with non-zero status if any errors detected\n"
  "                  otherwise exits with zero status\n"
  "                Use filename 'stdin' to read from stdin\n"
  "                Files can be in .gz, .bz2, .zip, .Z format and are \n"
  "                  automatically decompressed\n"
  "                Multiple input files of the same type can be listed\n"
  "                Error messages are written to stderr\n"
  "                OK or failing file lines can be optionally written to stdout\n"
  "usage:\n"
  "   validateFiles -type=FILE_TYPE file1 [file2 [...]]\n"
  "options:\n"
  "   -type=(a value from the list below)\n"
  "         tagAlign|pairedTagAlign|broadPeak|narrowPeak|gappedPeak|bedGraph\n"
  "                   : see http://genomewiki.cse.ucsc.edu/EncodeDCC/index.php/File_Formats\n"
  "         fasta     : Fasta files (only one line of sequence, and no quality scores)\n"
  "         fastq     : Fasta with quality scores (see http://maq.sourceforge.net/fastq.shtml)\n"
  "         csfasta   : Colorspace fasta (implies -colorSpace) (see link below)\n"
  "         csqual    : Colorspace quality (see link below)\n"
  "                     (see http://marketing.appliedbiosystems.com/mk/submit/SOLID_KNOWLEDGE_RD?_JS=T&rd=dm)\n"
#ifdef USE_BAM
  "         BAM       : Binary Alignment/Map\n"
  "                     (see http://samtools.sourceforge.net/SAM1.pdf)\n"
#endif
  "         bigWig    : Big Wig\n"
  "                     (see http://genome.ucsc.edu/goldenPath/help/bigWig.html\n"
  "\n"
  "   -chromDb=db                  Specify DB containing chromInfo table to validate chrom names\n"
  "                                  and sizes\n"
  "   -chromInfo=file.txt          Specify chromInfo file to validate chrom names and sizes\n"
  "   -colorSpace                  Sequences include colorspace values [0-3] (can be used \n"
  "                                  with formats such as tagAlign and pairedTagAlign)\n"
  "   -zeroSizeOk                  For BED-type positional data, allow rows with start==end\n"
  "                                  otherwise require strictly start < end\n"
  "   -genome=path/to/hg18.2bit    Validate tagAlign or pairedTagAlign sequences match genome\n"
  "                                  in .2bit file\n"
  "   -mismatches=n                Maximum number of mismatches in sequence (or read pair) if \n"
  "                                  validating tagAlign or pairedTagAlign files\n"
  "   -mismatchTotalQuality=n      Maximum total quality score at mismatching positions\n"
  "   -matchFirst=n                only check the first N bases of the sequence\n"
  "   -mmPerPair                   Check either pair dont exceed mismatch count if validating\n"
  "                                  pairedTagAlign files (default is the total for the pair)\n"
  "   -mmCheckOneInN=n             Check mismatches in only one in 'n' lines (default=1, all)\n"
  "   -nMatch                      N's do not count as a mismatch\n"
  "   -privateData                 Private data so empty sequence is tolerated\n"
  "   -printOkLines                Print lines which pass validation to stdout\n"
  "   -quick[=N]                   Just test the first N lines of each file (default 1000)\n"
  "   -printFailLines              Print lines which fail validation to stdout\n"
  "   -isSort                      input is sorted by chrom\n"
//"   -acceptDot                   Accept '.' as 'N' in DNA sequence\n"
  "   -version                     Print version\n"
  "   -allowOther                  allow chromosomes that aren't native in BAM's\n"
  "   -allowBadLength              allow chromosomes that have the wrong length\n in BAM\n"
  "   -complementMinus             complement the query sequence on the minus strand (for testing BAM)\n"
  "   -doReport                    output report in filename.report\n"
  "   -showBadAlign                show non-compliant alignments\n"
  "   -bamPercent=N.N              percentage of BAM alignments that must be compliant\n"
  "   -allowErrors=N               number of errors allowed to still pass (default 0)\n"
  "   -maxErrors=N                 Maximum lines with errors to report in one file before \n"
  "                                  stopping (default %d)\n"
  , MAX_ERRORS);
}

static struct optionSpec options[] = {
   {"type", OPTION_STRING},
   {"chromDb", OPTION_STRING},
   {"chromInfo", OPTION_STRING},
   {"maxErrors", OPTION_INT},
   {"colorSpace", OPTION_BOOLEAN},
   {"zeroSizeOk", OPTION_BOOLEAN},
   {"mismatchTotalQuality", OPTION_INT},
   {"printOkLines", OPTION_BOOLEAN},
   {"printFailLines", OPTION_BOOLEAN},
   {"genome", OPTION_STRING},
   {"mismatches", OPTION_INT},
   {"matchFirst", OPTION_INT},
   {"mmPerPair", OPTION_BOOLEAN},
   {"mmCheckOneInN", OPTION_INT},
   {"quick", OPTION_INT},
   {"nMatch", OPTION_BOOLEAN},
   {"privateData", OPTION_BOOLEAN},
// {"acceptDot", OPTION_BOOLEAN},
   {"isSort", OPTION_BOOLEAN},
   {"version", OPTION_BOOLEAN},
   {"allowOther", OPTION_BOOLEAN},
   {"allowBadLength", OPTION_BOOLEAN},
   {"allowErrors", OPTION_INT},
   {"complementMinus", OPTION_BOOLEAN},
   {"bamPercent", OPTION_FLOAT},
   {"showBadAlign", OPTION_BOOLEAN},
   {"doReport", OPTION_BOOLEAN},
   {NULL, 0},
};

FILE *reportF;

void vaErrAbort(char *format, va_list args);

void reportErrAbort(char *format, ...)
/* Abort function, with optional (printf formatted) error message. */
{
va_list args;

if (reportF != NULL)
    {
    va_start(args, format);
    vfprintf(reportF, format, args);
    va_end(args);
    }

va_start(args, format);
vaErrAbort(format, args);

va_end(args);
}

void vaWarn(char *format, va_list args);

void reportWarn(char *format, ...)
/* Issue a warning message. */
{
va_list args;
va_start(args, format);
vaWarn(format, args);
va_end(args);

va_start(args, format);
if (reportF != NULL)
    vfprintf(reportF, format, args);
va_end(args);
}

void report(char *format, ...)
/* Issue a warning message. */
{
va_list args;
va_start(args, format);
if (reportF != NULL)
    vfprintf(reportF, format, args);
va_end(args);
}

void reportLongWithCommas(long long l)
/* Print out a long number with commas at thousands, millions, etc. */
{
char ascii[32];
sprintLongWithCommas(ascii, l);
report("%s", ascii);
}

void reportLabelAndLongNumber(char *label, long long l)
/* Print label: 1,234,567 format number */
{
report("%s: ", label);
reportLongWithCommas( l);
report("\n");
}

boolean checkMismatch(int ch1, int ch2)
// checkMismatch -- if the sequence has an N, we call this a mismatch
//   by default unless nMatch is set, in which case we don't call
//   it a mismatch
{
if ((ch1 != 'n') && (ch2 != 'n'))
    return ch1 != ch2;

return !nMatch;
}

void initArrays()
// Set up array of chars
// dnaChars:  DNA chars ACGTNacgtn, and optionally include colorspace 0-3
// qualChars: fastq quality scores as ascii [!-~] (ord(!)=33, ord(~)=126)
// csQualChars: csfasta quality scores are decimals separated by spaces
// seqName:   fastq sequence name chars [A-Za-z0-9_.:/-]
{
int i;
// number of columns to expect in bedType files
bedTypeCols[BED_GRAPH] = 4;
bedTypeCols[BROAD_PEAK] = 9;
bedTypeCols[NARROW_PEAK] = 10;
bedTypeCols[GAPPED_PEAK] = 15;

for (i=0 ; i < 256 ; ++i)
    dnaChars[i] = qualChars[i] = csQualChars[i] = seqName[i] = csSeqName[i] = digits[i] = alpha[i] = 0;
dnaChars['a'] = dnaChars['c'] = dnaChars['g'] = dnaChars['t'] = dnaChars['n'] = 1;
dnaChars['A'] = dnaChars['C'] = dnaChars['G'] = dnaChars['T'] = dnaChars['N'] = 1;
if (colorSpace)
    {
    dnaChars['0'] = dnaChars['1'] = dnaChars['2'] = dnaChars['3'] = 1;
    }
for (i= (int)'A' ; i <= (int)'Z' ; ++i)
    seqName[i] = seqName[i+(int)('a'-'A')] = alpha[i] = alpha[i+(int)('a'-'A')] = 1;
for (i= (int)'0' ; i <= (int)'9' ; ++i)
    seqName[i] = digits[i] = csSeqName[i] = csQualChars[i] = 1;
seqName['_'] = seqName['.'] = seqName[':'] = seqName['/'] = seqName['-'] = seqName['#'] = 1;
csSeqName[','] = csSeqName['.'] = csSeqName['-'] = csSeqName['#'] = 1;
csQualChars[' '] = csQualChars['-'] = 1;
for (i= (int)'!' ; i <= (int)'~' ; ++i)
    qualChars[i] = 1;
}

struct hash *chromHash(struct chromInfo *ci)
// Return a hash table of chrom name to chrom size
{
unsigned *size;
struct hash *h = newHash(0);
for ( ; ci ; ci = ci->next )
    {
    AllocVar(size);
    *size = ci->size;
    verbose(3,"[%s %3d] hashAdd(%s -> %p = %u)\n", __func__, __LINE__, ci->chrom, size, *size);
    hashAdd(h, ci->chrom, size);
    }
return h;
}

boolean checkUnsigned(char *file, int line, char *row, char *s, unsigned *val, char *name)
/* Convert series of digits to unsigned integer about
 * twice as fast as atoi (by not having to skip white
 * space or stop except at the null byte.)
 * Returns true if conversion possible, and value is returned in 'val'
 * Otherwise prints warning and returns false */
{
unsigned res = 0;
char *p = s;
char c;

while (((c = *(p++)) >= '0') && (c <= '9'))
    {
    res *= 10;
    res += c - '0';
    }
if (c != '\0')
    {
    reportWarn("Error [file=%s, line=%d]: %s field invalid unsigned number (%s) [%s]", file, line, name, s, row);
    return FALSE;
    }
*val = res;
return TRUE;
}

boolean checkSigned(char *file, int line, char *row, char *s, int *val, char *name)
/* Convert string to signed integer.  Unlike atol assumes
 * all of string is number.
 * Returns true if conversion possible, and value is returned in 'val'
 * Otherwise prints warning and returns false */
{
int res = 0;
char *p, *p0 = s;

if (*p0 == '-')
    p0++;
p = p0;
while ((*p >= '0') && (*p <= '9'))
    {
    res *= 10;
    res += *p - '0';
    p++;
    }
/* test for invalid character, empty, or just a minus */
if ((*p != '\0') || (p == p0))
    {
    reportWarn("Error [file=%s, line=%d]: %s field invalid signed number (%s) [%s]", file, line, name, s, row);
    return FALSE;
    }
if (*s == '-')
    *val = -res;
else
    *val = res;
return TRUE;
}

boolean checkString(char *file, int line, char *row, char *s, char *name)
// Return TRUE if string has non-zero length
// Othewise print warning that name column is empty and return FALSE
{
verbose(3,"[%s %3d] %s(%s)\n", __func__, __LINE__, name, s);
if (strlen(s) > 0)
    return TRUE;
reportWarn("Error [file=%s, line=%d]: %s column empty [%s]", file, line, name, row);
return FALSE;
}

boolean checkChrom(char *file, int line, char *row, char *s, unsigned *chromSize)
// Return TRUE if string has non-zero length
// Othewise print warning that name column is empty and return FALSE
{
unsigned *size;
*chromSize = 0;
if (strlen(s) > 0)
    {
    if (chrHash)
	{
	if ( (size = hashFindVal(chrHash, s)) != NULL)
	    {
	    *chromSize = *size;
	    verbose(2,"[%s %3d] hashFindVal(%s -> %p = %u)\n", __func__, __LINE__, s, size, *size);
	    return TRUE; // found chrom
	    }
	else
	    {
	    reportWarn("Error [file=%s, line=%d]: chrom %s not found [%s]", file, line, s, row);
	    return FALSE; // chrom not found
	    }
	}
    else
	{
	verbose(2,"[%s %3d] chrom(%s) \n", __func__, __LINE__, s);
	return TRUE; // chrom name not blank, and not validating against chromInfo
	}
    }
reportWarn("Error [file=%s, line=%d]: chrom column empty [%s]", file, line, row);
return FALSE;
}

int checkSeq(char *file, int line, char *row, char *s, char *name)
// Return TRUE if string has non-zero length and contains only chars [ACGTNacgtn0-3]
// Othewise print warning that name column is empty and return FALSE
{
verbose(3,"[%s %3d] inputLine=%d %s seq(%s) [%s]\n", __func__, __LINE__, line, name, s, row);
int i;
int len = 0;
for ( i = 0; s[i] ; ++i)
    {
    if (!dnaChars[(int)s[i]])
	{
	if (s==row)
	    reportWarn("Error [file=%s, line=%d]: invalid DNA chars in %s(%s)", file, line, name, s);
	else
	    reportWarn("Error [file=%s, line=%d]: invalid DNA chars in %s(%s) [%s]", file, line, name, s, row);
	return 0;
	}
    len++;
    }
if (i == 0)
    {
    if(privateData)  // PrivateData means sequence should be empty
        return 1;
    if (s==row)
	reportWarn("Error [file=%s, line=%d]: %s empty", file, line, name);
    else
	reportWarn("Error [file=%s, line=%d]: %s empty in line [%s]", file, line, name, row);
    return 0;
    }
else if(privateData) { // PrivateData means sequence should be empty
    if (s==row)
        reportWarn("Error [file=%s, line=%d]: %s is not empty but this should be private data", file, line, name);
    else
        reportWarn("Error [file=%s, line=%d]: %s  is not empty but this should be private data in line [%s]", file, line, name, row);
    return 0;
    }
return len;
}

boolean checkSeqName(char *file, int line, char *s, char firstChar, char *name)
// Return TRUE if string has non-zero length and contains only seqName[] chars
// Othewise print warning that seqName is empty and return FALSE
{
int i;
if (s[0] == 0)
    {
    reportWarn("Error [file=%s, line=%d]: %s empty [%s]", file, line, name, s);
    return FALSE;
    }
else if (s[0] != firstChar)
    {
    reportWarn("Error [file=%s, line=%d]: %s first char invalid (got '%c', wanted '%c') [%s]",
	file, line, name, s[0], firstChar, s);
    return FALSE;
    }
for ( i = 1; s[i] ; ++i)
    {
    if (s[i] == ' ')
	break;
    if (!seqName[(int)s[i]])
	{
	reportWarn("Error [file=%s, line=%d]: invalid %s chars in [%s]", file, line, name, s);
	return FALSE;
	}
    }
return TRUE;
}

char *getDigits(char *s)
// Consume 1 or more digits from s, return pointer to next non-digit
// Return NULL if no digits consumed
{
char *s0 = s;
while (digits[(int) *s])
    ++s;
if (s > s0)
    return s;
else
    return NULL;
}

boolean checkTrailingCsSeqName(char *s)
// Return true if all chars in s (if any) are csSeqName chars
// Return false otherwise
{
while (csSeqName[(int) *s])
    ++s;
if (*s == 0)
    return TRUE;
else
    return FALSE;
}

//     >461_19_209_F3
//     T022213002230311203200200322000
//     >920_22_656_F3,1.-152654094.1.35.35.0###,19.43558664.1.35.35.0###
//     T01301010111200210102321210100112312

boolean checkCsSeqName(char *file, int line, char *s)
// Return TRUE if string has non-zero length, matches CS name pattern contains only csSeqName[] chars
// Othewise print warning that seqName is empty and return FALSE
{
char *s0;
if (s[0] == 0)
    {
    reportWarn("Error [file=%s, line=%d]: sequence name empty [%s]", file, line, s);
    return FALSE;
    }
else if (s[0] != '>')
    {
    reportWarn("Error [file=%s, line=%d]: sequence name first char invalid (got '%c', wanted '>') [%s]",
	file, line, s[0], s);
    return FALSE;
    }
if ( (s0 = getDigits(s+1))
      && (*(s0++) == '_')
      && (s0 = getDigits(s0)) && (*(s0++) == '_')
      && (s0 = getDigits(s0)) && (*(s0++) == '_')
      && alpha[(int) *(s0++)] && digits[(int) *(s0++)]
      && checkTrailingCsSeqName(s0) )
    {
    verbose(2,"[%s %3d] OK [%s] file(%s) line=%d\n", __func__, __LINE__, s, file, line);
    return TRUE;
    }
else
    {
    reportWarn("Error [file=%s, line=%d]: invalid sequence name [%s]", file, line, s);
    return FALSE;
    }
}

boolean checkQual(char *file, int line, char *s, int len)
// Return TRUE if string has non-zero length and contains only qualChars[] chars
// Othewise print warning that quality is empty and return FALSE
{
int i;
for ( i = 0; s[i] ; ++i)
    {
    if (!qualChars[(int)s[i]])
	{
	reportWarn("Error [file=%s, line=%d]: invalid quality chars in [%s]", file, line, s);
	return FALSE;
	}
    }
if (i == 0)
    {
    reportWarn("Error [file=%s, line=%d]: quality empty [%s]", file, line, s);
    return FALSE;
    }

if (i != len)
    {
    reportWarn("Error [file=%s, line=%d]: quality not as long as sequence (%d bases) [%s]", file, line, len, s);
    return FALSE;
    }

return TRUE;
}

boolean checkCsQual(char *file, int line, char *s)
// Return TRUE if string has non-zero length and contains quality scores
// Othewise print warning that quality is empty and return FALSE
{
int i;
for ( i = 0; s[i] ; ++i)
    {
    if (!csQualChars[(int)s[i]])
	{
	reportWarn("Error [file=%s, line=%d]: invalid colorspace quality chars in [%s]", file, line, s);
	return FALSE;
	}
    }
if (i == 0)
    {
    reportWarn("Error [file=%s, line=%d]: colorspace quality empty [%s]", file, line, s);
    return FALSE;
    }
return TRUE;
}

boolean checkStartEnd(char *file, int line, char *row, char *start, char *end, char *chrom, unsigned chromSize, unsigned *sVal, unsigned *eVal)
// Return TRUE if start and end are both >= 0,
// and if zeroSizeOk then start <= end
//        otherwise  then start < end
// Also check end <= chromSize (special case circular chrM start <= chromSize)
// start and end values are returned in sVal and eVal
// Othewise print warning and return FALSE
{
verbose(3,"[%s %3d] inputLine=%d [%s..%s] (chrom=%s,size=%u) [%s]\n", __func__, __LINE__, line, start, end, chrom, chromSize, row);
unsigned s, e;
if (   !checkUnsigned(file, line, row, start, &s, "chromStart")
    || !checkUnsigned(file, line, row, end, &e, "chromEnd"))
    return FALSE;
*sVal = s;
*eVal = e;
if (chromSize > 0)
    {
    if (e > chromSize && (differentString(chrom, "chrM") || s > chromSize)) // passes test if end < chromSize or chrM and start < chromSize
	{
        reportWarn("Error [file=%s, line=%d]: end(%u) > chromSize(%s=%u) [%s]", file, line, e, chrom, chromSize, row);
        return FALSE;
        }
    else
	verbose(2,"[%s %3d] end <= chromSize (%u <= %u)\n", __func__, __LINE__, e, chromSize);
    }
if (zeroSizeOk)
    {
    if (s <= e)
	{
	verbose(2,"[%s %3d] start <= end (%u <= %u)\n", __func__, __LINE__, s, e);
	return TRUE;
	}
    else
	reportWarn("Error [file=%s, line=%d]: start(%u) > end(%u) [%s]", file, line, s, e, row);
    }
else
    {
    if (s < e)
	{
	verbose(2,"[%s %3d] start < end (%u < %u)\n", __func__, __LINE__, s, e);
	return TRUE;
	}
    else
	reportWarn("Error [file=%s, line=%d]: start(%u) >= end(%u) [%s]", file, line, s, e, row);
    }
return FALSE;
}

boolean checkPeak(char *file, int line, char *row, char *peak, char *start, char *end)
// Return TRUE if peak is >= 0 and <= (end-start)
// Othewise print warning and return FALSE
{
verbose(3,"[%s %3d] inputLine=%d peak(%s) (%s,%s) [%s]\n", __func__, __LINE__, line, peak, start, end, row);
unsigned p, s, e;
int i;
if (!checkSigned(file, line, row, peak, &i, "peak"))
    return FALSE;
if (i == -1)
    return TRUE;

if (   !checkUnsigned(file, line, row, peak, &p, "peak")
    || !checkUnsigned(file, line, row, start, &s, "chromStart")
    || !checkUnsigned(file, line, row, end, &e, "chromEnd"))
    return FALSE;
if (p > e - s)
    {
    reportWarn("Error [file=%s, line=%d]: peak(%u) past block length (%u) [%s]", file, line, p, e - s, row);
    return FALSE;
    }
return TRUE;
}

boolean checkIntBetween(char *file, int line, char *row, char *val, char *name, int min, int max)
// Return TRUE if val is integer between min and max
// Othewise print warning and return FALSE
{
int i;
if (!checkSigned(file, line, row, val, &i, name))
    return FALSE;
verbose(2,"[%s %3d] inputLine=%d [%s] -> [%d] [%s,%d..%d]\n", __func__, __LINE__, line, val, i, name, min, max);
if (i >= min && i <= max)
    {
    verbose(2,"[%s %3d] min <= value <= max (%d <= %d <= %d)\n", __func__, __LINE__, min, i, max);
    return TRUE;
    }
reportWarn("Error [file=%s, line=%d]: %s %d outside bounds (%d, %d) [%s]", file, line, name, i, min, max, row);
return FALSE;
}

boolean checkFloat(char *file, int line, char *row, char *val, char *name)
// Return TRUE if val is floating point number
// Othewise print warning and return FALSE
// taken from sqlNum.c
{
char* end;
double discardMe = strtod(val, &end);
if ((end == val) || (*end != '\0'))
    {
    reportWarn("Error [file=%s, line=%d]: invalid %s '%s' [%s]", file, line, name, val, row);
    discardMe = 0.0;
    return FALSE;
    }
return TRUE;
}

boolean checkStrand(char *file, int line, char *row, char *strand)
// Return TRUE if strand == '+' or '-' or '.',
// Othewise print warning and return FALSE
{
if (strlen(strand) == 1 && (*strand == '+' || *strand == '-' || *strand == '.'))
    {
    verbose(2,"[%s %3d] strand(%s)\n", __func__, __LINE__, strand);
    return TRUE;
    }
reportWarn("Error [file=%s, line=%d]: invalid strand '%s' (want '+','-','.') [%s]", file, line, strand, row);
return FALSE;
}

boolean wantNewLine(struct lineFile *lf, char *file, int line, char **row, char *msg)
{
boolean res = lineFileNext(lf, row, NULL);
if (!res)
    reportWarn("Error [file=%s, line=%d]: %s not found", file, line, msg);
return res;
}

boolean checkColumns(char *file, int line, char *row, char *buf, char *words[], int wordSize, int expected)
// Split buf into wordSize columns in words[] array
// Return TRUE if number of columns == expected, otherwise FALSE
{
int n = chopByChar(buf, '\t', words, wordSize);
if ( n != expected)
    {
    reportWarn("Error [file=%s, line=%d]: found %d columns, expected %d [%s]", file, line, n, expected, row);
    return FALSE;
    }
return TRUE;
}



boolean checkMismatchesSeq(char *file, int line, char *chrom, unsigned chromStart, unsigned chromEnd, char strand, char *seq)
{
int i, mm = 0;
struct dnaSeq *g;
static struct dnaSeq *cacheSeq = NULL;
static char cacheChrom[1024];
static char bigArr[100 * 1024]; // 100K limit on tagAlign seqLen
struct dnaSeq ourSeq;
boolean chrMSizeAjustment=FALSE;

if(privateData)  // No way to check private data
    return TRUE;

if (!genome)
    return TRUE; // only check if 2bit file specified
if (line % mmCheckOneInN != 0)
    return TRUE; // dont check if this is not one in N
if (!isSort)
    {
    //unsigned end = chromEnd;
    if(sameString(chrom,"chrM"))
        {
        unsigned size =  twoBitSeqSize(genome, chrom);
        if( chromEnd>size)
            {
            chrMSizeAjustment=TRUE;
            chromEnd=size;
            }
        }
    g = twoBitReadSeqFragLower(genome, chrom, chromStart, chromEnd);
    }
else
    {
    // read the whole chrom
    if ((cacheChrom == NULL) || !sameString(chrom, cacheChrom))
	{
	freeDnaSeq(&cacheSeq);
	int size =  twoBitSeqSize(genome, chrom);
	cacheSeq = twoBitReadSeqFragLower(genome, chrom, 0, size);
	strcpy(cacheChrom, chrom);
	verbose(2, "read in chrom %s size %d\n",cacheChrom, size);
	}
    int len = chromEnd - chromStart;
    if (len > sizeof(bigArr))
	reportErrAbort("static array not big enough for sequence len %d on line %d\n",
	    len, line);
    g = &ourSeq;
    g->dna = bigArr;
    g->size = len;
    memcpy(g->dna, &cacheSeq->dna[chromStart], len);
    }

if (strand == '-')
    reverseComplement(g->dna, g->size);

if ((g->size != strlen(seq) || g->size != chromEnd-chromStart) && !chrMSizeAjustment)
    {
    reportWarn("Error [file=%s, line=%d]: sequence (%s) length (%d) does not match genomic coords (%d / %d - %s %d %d %c)",
        file, line, seq, (int)strlen(seq), chromEnd-chromStart, g->size,
        chrom, chromStart, chromEnd, strand);
    return FALSE;
    }

int length = g->size;
if (matchFirst && (matchFirst < length))
    length = matchFirst;

for (i=0 ; i < length; ++i)
    {
    char c = tolower(seq[i]);
    if (checkMismatch(c,  g->dna[i]))
        ++mm;
    }
if (mm > mismatches)
    {
    reportWarn("Error [file=%s, line=%d]: too many mismatches (found %d/%d, maximum is %d) (%s %d %d %c)\nseq=[%s]\ngen=[%s]\n",
         file, line, mm, g->size, mismatches, chrom, chromStart, chromEnd, strand, seq, g->dna);
    return FALSE;
    }
if (!isSort)
    freeDnaSeq(&g);
return TRUE;
}

boolean checkMismatchesSeq1Seq2(char *file, int line, char *chrom, unsigned chromStart, unsigned chromEnd, char strand, char *seq1, char *seq2)
{
int i, mm1, mm2, len1, len2;
struct dnaSeq *g1, *g2;
if(privateData)  // No way to check private data
    return TRUE;
if (!genome)
    return TRUE; // dont check unless 2bit file specified
if (line % mmCheckOneInN != 0)
    return TRUE; // dont check if this is not one in N
len1 = strlen(seq1);
len2 = strlen(seq2);
if (strand == '-')
    {
    g1 = twoBitReadSeqFragLower(genome, chrom, chromEnd-len1, chromEnd);
    g2 = twoBitReadSeqFragLower(genome, chrom, chromStart, chromStart+len2);
    reverseComplement(g1->dna, g1->size);
    reverseComplement(g2->dna, g2->size);
    }
else
    {
    g1 = twoBitReadSeqFragLower(genome, chrom, chromStart, chromStart+len1);
    g2 = twoBitReadSeqFragLower(genome, chrom, chromEnd-len2, chromEnd);
    }
if (g1->size != len1 || g2->size != len2)
    {
    reportWarn("Error [file=%s, line=%d]: sequence lengths (%d, %d) do not match genomic ones (%d, %d)",
         file, line, len1, len2, g1->size, g2->size);
    return FALSE;
    }
mm1 = 0;
for (i=0 ; i < g1->size ; ++i)
    {
    char c = tolower(seq1[i]);
    if (checkMismatch(c,  g1->dna[i]))
        ++mm1;
    }
mm2 = 0;
for (i=0 ; i < g2->size ; ++i)
    {
    char c = tolower(seq2[i]);
    if (checkMismatch(c,  g2->dna[i]))
        ++mm2;
    }
if (mmPerPair)
    {
    if (mm1 > mismatches || mm2 > mismatches)
        {
        reportWarn("Error [file=%s, line=%d]: too many mismatches in one or both (seq1=%d/%d, seq2=%d/%d, maximum is %d) (%s %d %d %c)\nseq1=[%s] seq2=[%s]\ngen1=[%s] gen2=[%s]\n",
             file, line, mm1, len1, mm2, len2, mismatches, chrom, chromStart, chromEnd, strand, seq1, seq2, g1->dna, g2->dna);
        return FALSE;
        }
    }
else
    {
    if (mm1+mm2 > mismatches)
        {
        reportWarn("Error [file=%s, line=%d]: too many mismatches in pair (seq1=%d/%d, seq2=%d/%d, maximum is %d) (%s %d %d %c)\nseq1=[%s] seq2=[%s]\ngen1=[%s] gen2=[%s]\n",
             file, line, mm1, len1, mm2, len2, mismatches, chrom, chromStart, chromEnd, strand, seq1, seq2, g1->dna, g2->dna);
        return FALSE;
        }
    }
freeDnaSeq(&g1);
freeDnaSeq(&g2);
return TRUE;
}

int validateTagOrPairedTagAlign(struct lineFile *lf, char *file, boolean paired)
{
char *row;
char buf[1024];
char *words[TAG_WORDS];
int line = 0;
int errs = 0;
unsigned chromSize;
int size;
verbose(2,"[%s %3d] paired=%d file(%s)\n", __func__, __LINE__, paired, file);
while (lineFileNext(lf, &row, &size))
    {
    unsigned start, end;
    ++line;
    if (quick && line > quick)
	break;
    safecpy(buf, sizeof(buf), row);
    if ( checkColumns(file, line, row, buf, words, TAG_WORDS, (paired ? 8 : 6))
	&& checkChrom(file, line, row, words[0], &chromSize)
         && checkStartEnd(file, line, row, words[1], words[2], words[0], chromSize, &start, &end)
	&& checkIntBetween(file, line, row, words[4], "score", 0, 1000)
	&& checkStrand(file, line, row, words[5])
	&& (paired ?
		(checkString(file, line, row, words[3], "name")
		&& checkSeq(file, line, row, words[6], "seq1")
		&& checkSeq(file, line, row, words[7], "seq2")
                 && checkMismatchesSeq1Seq2(file, line, words[0], start, end, *words[5], words[6], words[7]))
	    :
            (checkSeq(file, line, row, words[3], "sequence")
             && checkMismatchesSeq(file, line, words[0], start, end, *words[5], words[3]))
	    ) )
	{
	if (printOkLines)
	    printf("%s\n", row);
	}
    else
	{
	if (printFailLines)
	    printf("%s\n", row);
	if (++errs >= maxErrors)
	    reportErrAbort("Aborting .. found %d errors\n", errs);
	}
    }
return errs;
}

// tagAlign
// chr1     6082    6117    TCTACTGGCTCTGTGTGTACCAGTCTGTCACTGAG     1000    -
// chr1     7334    7369    AGCCAGGGGGTGACGTTGTTAGATTAGATTTCTTA     1000    +

int validateTagAlign(struct lineFile *lf, char *file)
{
return validateTagOrPairedTagAlign(lf, file, FALSE);
}

// pairedTagAlign
// chr10    96316360        96310862        9       1000    +       TCTCACCCGATAACGACCCCCTCCC       TGATCCTTGACTCACTTGCTAATTT
// chr8    126727657       126721865       10      1000    +       AATTCTTCACCTCTCCTGTTCAAAG       TGTGTGAGATCCAAGAATCCTCTCT

int validatePairedTagAlign(struct lineFile *lf, char *file)
{
return validateTagOrPairedTagAlign(lf, file, TRUE);
}

int validateBedVariant(struct lineFile *lf, char *file, enum bedType type)
{
char *row;
char buf[1024];
char *words[PEAK_WORDS];
int line = 0;
int errs = 0;
unsigned chromSize;
int gappedOffset = (type == GAPPED_PEAK ? 6 : 0);
verbose(2,"[%s %3d] file(%s)\n", __func__, __LINE__, file);
while (lineFileNextReal(lf, &row))
    {
    ++line;
    unsigned start, end;
    if (quick && line > quick)
	break;
    safecpy(buf, sizeof(buf), row);
    if ( checkColumns(file, line, row, buf, words, PEAK_WORDS, bedTypeCols[type])
	&& checkChrom(file, line, row, words[0], &chromSize)
         && checkStartEnd(file, line, row, words[1], words[2], words[0], chromSize, &start, &end)
	&& ( type == BED_GRAPH ?
	      (checkFloat(file, line, row, words[3], "value")) // canonical bedGraph has float in 4th column
	   : // otherwise BROAD_, NARROW_, or GAPPED_PEAK
	      (checkString(file, line, row, words[3], "name")
		  && checkIntBetween(file, line, row, words[4], "score", 0, 1000)
		  && checkStrand(file, line, row, words[5])
		  // && ((type != GAPPED_PEAK) || ()) // for now dont check all the BED 12 gapped fields
		  && checkFloat(file, line, row, words[6 + gappedOffset], "signalValue")
		  && checkFloat(file, line, row, words[7 + gappedOffset], "pValue")
		  && checkFloat(file, line, row, words[8 + gappedOffset], "qValue")
		  && ((type != NARROW_PEAK) || (checkPeak(file, line, row, words[9], words[1], words[2])))
	      )
	    )
	)
	{
	if (printOkLines)
	    printf("%s\n", row);
	}
    else
	{
	if (printFailLines)
	    printf("%s\n", row);
	if (++errs >= maxErrors)
	    reportErrAbort("Aborting .. found %d errors\n", errs);
	}
    }
return errs;
}

int validateBroadPeak(struct lineFile *lf, char *file)
{
return validateBedVariant(lf, file, BROAD_PEAK);
}

int validateNarrowPeak(struct lineFile *lf, char *file)
{
return validateBedVariant(lf, file, NARROW_PEAK);
}

int validateGappedPeak(struct lineFile *lf, char *file)
{
return validateBedVariant(lf, file, GAPPED_PEAK);
}

int validateBedGraph(struct lineFile *lf, char *file)
{
return validateBedVariant(lf, file, BED_GRAPH);
}

// fasta:
// >VHE-245683051005-13-1-2-1704
// GTGTTAATTTTCTTGATCTTTCGTTC
// >VHE-245683051005-13-1-2-1704
// CTTGCTTTCTAGTTCTTTTAATTGTG

int validateFasta(struct lineFile *lf, char *file)
{
char *seqName, *seq;
int line = 0;
int errs = 0;
boolean startOfFile = TRUE;
verbose(2,"[%s %3d] file(%s)\n", __func__, __LINE__, file);
while ( lineFileNext(lf, &seqName, NULL))
    {
    ++line;
    if (quick && line > quick)
	break;
    if (startOfFile)
	{
	if (*seqName == '#')
	    continue;
	else
	    startOfFile = FALSE;
	}
    if (checkSeqName(file, line, seqName, '>', "sequence name")
	&& (wantNewLine(lf, file, ++line, &seq, "fastq sequence line"))
	&& checkSeq(file, line, seq, seq, "sequence") )
	{
	if (printOkLines)
	    printf("%s\n%s\n", seqName, seq);
	}
    else
	{
	if (printFailLines)
	    printf("%s\n%s\n", seqName, seq);
	if (++errs >= maxErrors)
	    reportErrAbort("Aborting .. found %d errors\n", errs);
	}
    }
return errs;
}

// fastq:
// @NINA_1_FC30G3VAAXX:5:1:110:908
// ATCGTCAGGTGGGATAATCCTTACCTTTTCCTCCTC
// +NINA_1_FC30G3VAAXX:5:1:110:908
// aa`]`a`XQ^VQQ^`aaaaaaa^[[ZG[aXUX[[[X

int validateFastq(struct lineFile *lf, char *file)
{
char *seqName, *seq, *qName, *qual;
int line = 0;
int errs = 0;
boolean startOfFile = TRUE;
verbose(2,"[%s %3d] file(%s)\n", __func__, __LINE__, file);
while ( lineFileNext(lf, &seqName, NULL))
    {
    ++line;
    if (quick && line > quick)
	break;
    if (startOfFile)
	{
	if (*seqName == '#')
	    continue;
	else
	    startOfFile = FALSE;
	}
    int len = 0;
    if (checkSeqName(file, line, seqName, '@', "sequence name")
	&& (wantNewLine(lf, file, ++line, &seq, "fastq sequence line"))
	&& (len = checkSeq(file, line, seq, seq, "sequence"))
	&& (wantNewLine(lf, file, ++line, &qName, "fastq sequence name (quality line)"))
	&& checkSeqName(file, line, qName, '+', "quality name")
	&& (wantNewLine(lf, file, ++line, &qual, "quality line"))
	&& checkQual(file, line, qual, len) )
	{
	if (printOkLines)
	    printf("%s\n%s\n%s\n%s\n", seqName, seq, qName, qual);
	}
    else
	{
	if (printFailLines)
	    printf("%s\n%s\n%s\n%s\n", seqName, seq, qName, qual);
	if (++errs >= maxErrors)
	    reportErrAbort("Aborting .. found %d errors\n", errs);
	}
    }
reportLabelAndLongNumber("number of sequences", line / 4);
return errs;
}

/*    Syntax per http://marketing.appliedbiosystems.com/mk/submit/SOLID_KNOWLEDGE_RD?_JS=T&rd=dm
CS Fasta:
>461_19_209_F3
T022213002230311203200200322000
>920_22_656_F3,1.-152654094.1.35.35.0###,19.43558664.1.35.35.0###
T01301010111200210102321210100112312
*/

int validateCsfasta(struct lineFile *lf, char *file)
// Validate Colorspace fasta files
{
char *seqName = NULL;
char *seq = NULL;
int line = 0;
int errs = 0;
boolean startOfFile = TRUE;
verbose(2,"[%s %3d] file(%s)\n", __func__, __LINE__, file);
while (lineFileNext(lf, &seqName, NULL))
    {
    ++line;
    if (quick && line > quick)
	break;
    if (startOfFile)
	{
	if (*seqName == '#')
	    continue;
	else
	    startOfFile = FALSE;
	}
    if (checkCsSeqName(file, line, seqName)
	&& (wantNewLine(lf, file, ++line, &seq, "colorspace sequence name"))
	&& checkSeq(file, line, seq, seq, "colorspace sequence") )
	{
	if (printOkLines)
	    printf("%s\n%s\n", seqName, seq);
	}
    else
	{
	if (printFailLines)
	    printf("%s\n%s\n", seqName, seq);
	if (++errs >= maxErrors)
	    reportErrAbort("Aborting .. found %d errors\n", errs);
	}
    }
return errs;
}


/*    Syntax per http://marketing.appliedbiosystems.com/mk/submit/SOLID_KNOWLEDGE_RD?_JS=T&rd=dm
    Sample:-

# Cwd: /home/pipeline
# Title: S0033_20080723_2_I22_EA_
>461_19_90_F3
20 10 8 13 8 10 20 7 7 24 15 22 21 14 14 8 11 15 5 20 6 5 8 22 6 24 3 16 7 11
>461_19_209_F3
16 8 5 12 20 24 19 8 13 17 11 23 8 24 8 7 17 4 20 8 29 7 3 16 3 4 8 20 17 9
*/

int validateCsqual(struct lineFile *lf, char *file)
// Validate Colorspace quality files
{
char *seqName = NULL;
char *qual = NULL;
int line = 0;
int errs = 0;
boolean startOfFile = TRUE;
verbose(2,"[%s %3d] file(%s)\n", __func__, __LINE__, file);
while (lineFileNext(lf, &seqName, NULL))
    {
    ++line;
    if (quick && line > quick)
	break;
    if (startOfFile)
	{
	if (*seqName == '#')
	    continue;
	else
	    startOfFile = FALSE;
	}
    if (checkCsSeqName(file, line, seqName)
	&& (wantNewLine(lf, file, ++line, &qual, "colorspace quality line"))
	&& checkCsQual(file, line, qual) )
	{
	if (printOkLines)
	    printf("%s\n%s\n", seqName, qual);
	}
    else
	{
	if (printFailLines)
	    printf("%s\n%s\n", seqName, qual);
	if (++errs >= maxErrors)
	    reportErrAbort("Aborting .. found %d errors\n", errs);
	}
    }
return errs;
}

int validateBigWig(struct lineFile *lf, char *file)
{
if (chrHash == NULL)
    reportErrAbort("bigWig validation requires the -chromInfo or -chromDb option\n");

int errs = 0;
struct bbiFile *bbiFile;

if (!bigWigFileCheckSigs(file))
    reportErrAbort("bad signatures in file %s\n", file);

bbiFile = bigWigFileOpen(file);

if (bbiFile == NULL)
    reportErrAbort("Aborting... Cannot open bigWig file: %s\n", file);


struct bbiChromInfo *bbiChroms = bbiChromList(bbiFile);

if (bbiChroms == NULL)
    reportErrAbort("Aborting... cannot get bigWig chromosome list in file: %s\n", file);

struct bbiChromInfo *chroms = bbiChroms;
for(; chroms; chroms = chroms->next)
    {
    unsigned *size;

    if ( (size = hashFindVal(chrHash, chroms->name)) == NULL)
	{
	reportWarn("bigWig contains invalid chromosome name: %s\n", 
	    chroms->name);
	errs++;
	}
    else
	{
	if (*size != chroms->size)
	    {
	    reportWarn("bigWig contains chromosome with wrong length: %s should be %d bases, not %d bases\n", 
		chroms->name,
		*size, chroms->size);
	    errs++;
	    }
	}
    }


if (errs)
    reportErrAbort("Aborting... %d errors found in bigWig file\n", errs);

report("version: %d\n", bbiFile->version);
report("isCompressed: %s\n", (bbiFile->uncompressBufSize > 0 ? "yes" : "no"));
report("isSwapped: %d\n", bbiFile->isSwapped);
reportLabelAndLongNumber("primaryDataSize", bbiFile->unzoomedIndexOffset - bbiFile->unzoomedDataOffset);
if (bbiFile->levelList != NULL)
    {
    long long indexEnd = bbiFile->levelList->dataOffset;
    reportLabelAndLongNumber("primaryIndexSize", indexEnd - bbiFile->unzoomedIndexOffset);
    }
report("zoomLevels: %d\n", bbiFile->zoomLevels);

struct bbiZoomLevel *zoom;
for (zoom = bbiFile->levelList; zoom != NULL; zoom = zoom->next)
    report("\t%d\t%d\n", zoom->reductionLevel, (int)(zoom->indexOffset - zoom->dataOffset));

struct bbiChromInfo *chrom, *chromList = bbiChroms;
report("chromCount: %d\n", slCount(chromList));
for (chrom=chromList; chrom != NULL; chrom = chrom->next)
    report("\t%s %d %d\n", chrom->name, chrom->id, chrom->size);
struct bbiSummaryElement sum = bbiTotalSummary(bbiFile);
reportLabelAndLongNumber("basesCovered", sum.validCount);
report("mean: %f\n", sum.sumData/sum.validCount);
report("min: %f\n", sum.minVal);
report("max: %f\n", sum.maxVal);
report("std: %f\n", calcStdFromSums(sum.sumData, sum.sumSquares, sum.validCount));

return errs;
}

#ifdef USE_BAM
struct bamCallbackData
    {
    char *file;
    char *chrom;
    int *errs;
    int numAligns;
    int numNeg;
    int numPos;
    struct hash *tagHash;
    };

boolean checkCigarMismatches(char *file, int line, char *chrom, unsigned chromStart, char strand, char *seq, UBYTE* quals, unsigned int *cigarPacked, int nCigar)
{
static char cacheChrom[1024];
static struct dnaSeq *cacheSeq = NULL;

if (!genome)
    reportErrAbort("Need to specify genome if checking BAM files");

if ((line % mmCheckOneInN) != 0)
    return TRUE; // dont check if this is not one in N

// read the whole chrom
if ((cacheChrom == NULL) || !sameString(chrom, cacheChrom))
    {
    freeDnaSeq(&cacheSeq);
    int size =  twoBitSeqSize(genome, chrom);
    cacheSeq = twoBitReadSeqFragLower(genome, chrom, 0, size);
    strcpy(cacheChrom, chrom);
    verbose(2, "read in chrom %s size %d: aligns to this point %d\n",
        cacheChrom, size, line);
    }


int curPosGenome = chromStart;
int i, j;
int mmTotalQual = 0;
int mm = 0;
char dna[10000], *dnaPtr = dna;
for (i = 0;   (i < nCigar);  i++)
    {
    char op;
    int size = bamUnpackCigarElement(cigarPacked[i], &op);
    switch (op)
	{
	case 'M': // match or mismatch (gapless aligned block)
            //printf("grabbing %d from %d\n",size,curPosGenome);
            for (j=0 ; j < size; ++j)
                {
                *dnaPtr++ = cacheSeq->dna[curPosGenome];
                curPosGenome++;
                }
	    break;
	case 'I': // inserted in query
	case 'S': // skipped query bases at beginning or end ("soft clipping")
            for(j=0; j < size; j++)
                *dnaPtr++ = '-';
	    break;
	case 'D': // deleted from query
	case 'N': // long deletion from query (intron as opposed to small del)
	    curPosGenome += size;
	    break;
	case 'H': // skipped query bases not stored in record's query sequence ("hard clipping")
	case 'P': // P="silent deletion from padded reference sequence" -- ignore these.
	    break;
	default:
	    reportErrAbort("unrecognized CIGAR op %c -- update me", op);
	}
    }

*dnaPtr = 0;

if (complementMinus && (strand == '-'))
    reverseComplement(seq, strlen(seq));

int length = strlen(seq);
if (matchFirst && (matchFirst < length))
    length = matchFirst;
int checkLength = length;

int start, incr;
if (strand == '-')
    {
    start = strlen(seq) - 1;
    incr = -1;
    }
else
    {
    start = 0;
    incr = 1;
    }

for (i = start; (strand == '-') ? i >= 0 : i < strlen(seq); i += incr)
    {
    char c = tolower(seq[i]);
    if ((dna[i] != '-') && (checkMismatch(c,  dna[i]))) 
        {
        ++mm;
        if (quals)
            mmTotalQual += min( round( quals[i] / 10 ) * 10, 30 );
        }

    if (--length == 0)
        break;
    }


if (mm > mismatches || ((quals != NULL) && (mmTotalQual > mismatchTotalQuality)))
    {
    if (showBadAlign)
        {
        assert(checkLength < 10000);
        char match[10000];

        for(i = 0; i < strlen(seq); i++)
            {
            if ((dna[i] == '-') || (tolower(seq[i]) == dna[i]))
                match[i] = ' ';
            else
                match[i] = 'x';
            }
        match[i] = 0;

        if (mm > mismatches)
            {
            reportWarn("Error [file=%s, line=%d]: too many mismatches (found %d/%d, maximum is %d) (%s: %d\nquery %s\nmatch %s\ndna   %s )\n",
                file, line, mm, checkLength, mismatches, chrom, chromStart, seq, match, dna);
            }

        if ((quals != NULL) && (mmTotalQual > mismatchTotalQuality))
            {
            char squal[10000];
            for (i = 0; i < checkLength; i++)
                squal[i] = '0' + min( round( quals[i] / 10 ), 3 );

            reportWarn("Error [file=%s, line=%d]: total quality at mismatches too high (found %d, maximum is %d) (%s: %d\nquery %s\nmatch %s\ndna   %s\nqual  %s )\n",
                file, line, mmTotalQual, mismatchTotalQuality, chrom, chromStart, seq, match, dna, squal);
            }        
        }
    return FALSE;
    }
return TRUE;
}

unsigned long long numQuals[256];
unsigned long quals[256];
unsigned int flagOr = 0;

int parseBamRecord(const bam1_t *bam, void *data)
/* bam_fetch() calls this on each bam alignment retrieved. */
{
struct bamCallbackData *bd = data;
char *chrom = bd->chrom;
char *file = bd->file;
int *errs = bd->errs;
const bam1_core_t *core = &bam->core;
unsigned int *cigarPacked = bam1_cigar(bam);
char *query = bamGetQuerySequence(bam, FALSE);
char strand = bamIsRc(bam) ? '-' : '+';
UBYTE *queryQuals = NULL;
quals[core->qual]++;

bd->numAligns++;

unsigned char *s = bam1_aux(bam);
char str[3];
str[2] = 0;
while(s < bam->data + bam->data_len)
    {
    str[0] = s[0];
    str[1] = s[1];
    hashStore(bd->tagHash, str);
    uint8_t type;
    s += 2; type = *s; ++s;
    if (type == 'A') {  ++s; }
    else if (type == 'C') {  ++s; }
    else if (type == 'c') {  ++s; }
    else if (type == 'S') {  s += 2; }
    else if (type == 's') {  s += 2; }
    else if (type == 'I') {  s += 4; }
    else if (type == 'i') {  s += 4; }
    else if (type == 'f') {  s += 4; }
    else if (type == 'd') {  s += 8; }
    else if (type == 'Z' || type == 'H')
        {
        while (*s) s++;
        ++s;
        }
    }



if (core->flag &  BAM_FUNMAP)
    // read is unmapped... ignore
    return 0;

flagOr |= core->flag;

if ( mismatchTotalQuality)
    {
    queryQuals = bamGetQueryQuals(bam, FALSE);
    int ii;
    int len = strlen(query);
    for(ii=0; ii < len; ii++)
        numQuals[queryQuals[ii]]++;
    }

if (bam->core.l_qseq == 0)
    {
    reportWarn("zero length sequence on line %d\n", bd->numAligns);
    ++(*errs);
    }
else if (! checkCigarMismatches(file, bd->numAligns, chrom, bam->core.pos, 
            strand, query, queryQuals, cigarPacked, core->n_cigar))
    {
    char *cigar = bamGetCigar(bam);
    if (showBadAlign)
        reportWarn("align: ciglen %d cigar %s qlen %d pos %d length %d strand %c\n",bam->core.n_cigar, cigar, bam->core.l_qname, bam->core.pos,  bam->core.l_qseq, bamIsRc(bam) ? '-' : '+');

    ++(*errs);
    }
    
if ((bamPercent == 0.0) && (*errs) >= maxErrors)
    reportErrAbort("Aborting .. found %d errors\n", *errs);

if (strand == '+')
    bd->numPos++;
else
    bd->numNeg++;
return 0;
}

int validateBAM(struct lineFile *lf, char *file)
{
if (chrHash == NULL)
    reportErrAbort("BAM validation requires the -chromInfo or -chromDb option\n");

int errs = 0;
samfile_t *fh = samopen(file, "rb", NULL);

if (fh == NULL)
    reportErrAbort("Aborting... Cannot open BAM file: %s\n", file);

bam_header_t *head = fh->header;

if (head == NULL)
    reportErrAbort("Aborting... Bad BAM header in file: %s\n", file);

int ii;

for(ii=0; ii < head->n_targets; ii++)
    {
    unsigned *size;

    verbose(2,"has chrom %s\n", head->target_name[ii]);
    if ( (size = hashFindVal(chrHash, head->target_name[ii])) == NULL)
	{
        reportWarn("BAM contains invalid chromosome name: %s\n", 
            head->target_name[ii]);
	if (!allowOther)
            errs++;
	}
    else
	{
	if (*size != head->target_len[ii])
	    {
            reportWarn("BAM contains chromosome with wrong length: %s should be %d bases, not %d bases\n", 
                head->target_name[ii],
                *size, head->target_len[ii]);
            if (!allowBadLength)
                errs++;
	    }
	}
    }

if (errs > allowErrors)
    reportErrAbort("Aborting... %d errors found in BAM file\n", errs);

if (!genome)
    return errs; // only check sequence if 2bit file specified

bam_index_t *idx = bam_index_load(file);

if (idx == NULL)
    reportErrAbort("couldn't find index file for %s\n", file);
struct bamCallbackData *bd;
AllocVar(bd);

bd->file = file;
if (bamPercent != 0.0)
    errs = 0;  // if we're testing on percent compliant, don't count header errs
bd->errs = &errs;
bd->numPos = bd->numNeg = 0;
struct hash *tagHash = newHash(5);
bd->tagHash = tagHash;

for(ii=0; ii < head->n_targets; ii++)
    {
    char position[256];
    safef(position, sizeof position, "%s:%d-%d",head->target_name[ii], 0, head->target_len[ii]);
    int chromId, start, end;
    int ret = bam_parse_region(fh->header, position, &chromId, &start, &end);

    bd->chrom = head->target_name[ii];
    verbose(2,"asking for alignments on %s\n",bd->chrom);
    ret = bam_fetch(fh->x.bam, idx, chromId, start, end, bd, parseBamRecord);

    }

report("number of BAM alignments %d pos %d neg %d errors %d\n",
    bd->numAligns, bd->numPos, bd->numNeg, errs );
if (doReport)
    {
    struct hashCookie cook = hashFirst(tagHash);
    struct hashEl *hel;
    report("tags: ");
    while((hel = hashNext(&cook)) != NULL)
        {
        report("%s ",hel->name);
        }
    report("\n");

    report( "flagOr: %x\n", flagOr);
    }

double percentCorrect = 100.0 * (bd->numAligns - errs)/(double)bd->numAligns;
report( "BAM alignment compliancy rate: %g\n", percentCorrect);
if (bamPercent != 0.0) 
    {
    if (percentCorrect < bamPercent)
        reportErrAbort("Aborting: too many non-compliant BAM alignments (%%%g compliancy rate)", percentCorrect);
    errs = 0;
    }

report("alignQuals ");
for(ii=0; ii < 256; ii++)
    {
    report("%ld ",quals[ii]);
    }
report("\n");

report("seqQuals ");
for(ii=0; ii < 256; ii++)
    {
    report("%lld ",numQuals[ii]);
    }
report("\n");

return errs;
}
#endif

void validateFiles(int (*validate)(struct lineFile *lf, char *file), int numFiles, char *files[])
/* validateFile - validate format of different track input files. */
{
int i;
int errs = 0;
verbose(2,"[%s %3d] numFiles=%d \n", __func__, __LINE__, numFiles);
for (i = 0; i < numFiles ; ++i)
    {
    struct lineFile *lf = lineFileOpen(files[i], TRUE);

    char buffer[10 * 1024];
    if (doReport)
        {
        safef(buffer, sizeof buffer, "%s.report", files[i]);
        verbose(2, "opening report %s\n", buffer);
        reportF = mustOpen(buffer, "w");
        }
    else 
        reportF = NULL;

    report("validateFiles %s %s\n", version, files[i]);
    errs += validate(lf, files[i]);

    if (quick)
	{
	/* set up error catch to allow SIGPIPE errors from gzip */
	struct errCatch *errCatch = errCatchNew();
	if (errCatchStart(errCatch))
	    lineFileClose(&lf);
	errCatchEnd(errCatch);
	if (errCatch->gotError)
	   warn("Ignoring: %s", errCatch->message->string);
	errCatchFree(&errCatch);
	}
    else
	lineFileClose(&lf);

    report("endReport\n");
    if (reportF != NULL)
        {
        fclose(reportF);
        reportF = NULL;
        }
    }
verbose(2,"[%s %3d] done loop\n", __func__, __LINE__);
if (errs > allowErrors)
    errAbort("Aborting ... found %d errors in total\n", errs);
else
    printf( "Error count %d\n",errs);

verbose(2,"[%s %3d] done\n", __func__, __LINE__);

}

int testFunc(char *f)
{
char *row;
int size;
struct lineFile *lf = lineFileOpen(f, TRUE);
while (lineFileNext(lf, &row, &size))
    printf("size=%d [%s]\n", size, row);
printf("done.\n");
return 0;
}

struct chromInfo *ourChromInfoLoad(char **row)
/* Load a chromInfo from row fetched with select * from chromInfo
 * from database.  Dispose of this with chromInfoFree(). */
{
struct chromInfo *ret;

AllocVar(ret);
ret->chrom = cloneString(row[0]);
ret->size = sqlUnsigned(row[1]);
ret->fileName = NULL;
return ret;
}

static struct chromInfo *chromInfoLoadFile(char *fileName) 
{
struct chromInfo *list = NULL, *el;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[2];

while (lineFileRow(lf, row))
    {
    el = ourChromInfoLoad(row);
    slAddHead(&list, el);
    }
lineFileClose(&lf);
slReverse(&list);
return list;
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *type;
void *func;
struct chromInfo *ci = NULL;
struct hash *funcs = newHash(0);
char *chromDb, *chromInfo;
optionInit(&argc, argv, options);
++argv;
--argc;
if (optionExists("version"))
    reportErrAbort("%s", version);
if (argc==0)
    usage();
type = optionVal("type", "");
if (strlen(type) == 0)
    reportErrAbort("please specify type");
maxErrors      = optionInt("maxErrors", MAX_ERRORS);
allowErrors    = optionInt("allowErrors", allowErrors);
zeroSizeOk     = optionExists("zeroSizeOk");
printOkLines   = optionExists("printOkLines");
printFailLines = optionExists("printFailLines");
genome         = optionExists("genome") ? twoBitOpen(optionVal("genome",NULL)) : NULL;
mismatches     = optionInt("mismatches",0);
matchFirst     = optionInt("matchFirst",0);
mmPerPair      = optionExists("mmPerPair");
mismatchTotalQuality = optionInt("mismatchTotalQuality",0);
nMatch         = optionExists("nMatch");
privateData    = optionExists("privateData");
isSort         = optionExists("isSort");
mmCheckOneInN  = optionInt("mmCheckOneInN", 1);
quick          = optionExists("quick") ? optionInt("quick",QUICK_DEFAULT) : 0;
colorSpace     = optionExists("colorSpace") || sameString(type, "csfasta");
allowOther     = optionExists("allowOther");
allowBadLength = optionExists("allowBadLength");
complementMinus = optionExists("complementMinus");
bamPercent     = optionFloat("bamPercent", bamPercent);
showBadAlign   = optionExists("showBadAlign");
doReport       = optionExists("doReport");

if ((bamPercent != 0.0) && (mmCheckOneInN != 1 ))
    reportErrAbort("can't specify bamPercent and mmCheckOneInN");

initArrays();
dnaChars[(int)'.'] = 1;//optionExists("acceptDot");   // I don't think this is worth adding another option.  But it could be done.

// Get chromInfo from DB or file
if ( (chromDb = optionVal("chromDb", NULL)) != NULL)
    {
    if (!(ci = createChromInfoList(NULL, chromDb)))
        reportErrAbort("could not load chromInfo from DB %s\n", chromDb);
    chrHash = chromHash(ci);
    chromInfoFree(&ci);
    }
else if ( (chromInfo=optionVal("chromInfo", NULL)) != NULL)
    {
    if (!(ci = chromInfoLoadFile(chromInfo)))
	reportErrAbort("could not load chromInfo file %s\n", chromInfo);
    chrHash = chromHash(ci);
    chromInfoFree(&ci);
    }
verbose(2,"[%s %3d] type=%s\n", __func__, __LINE__, type);
// Setup the function hash keyed by type
hashAdd(funcs, "tagAlign",       &validateTagAlign);
hashAdd(funcs, "pairedTagAlign", &validatePairedTagAlign);
hashAdd(funcs, "fasta",          &validateFasta);
hashAdd(funcs, "fastq",          &validateFastq);
hashAdd(funcs, "csfasta",        &validateCsfasta);
hashAdd(funcs, "csqual",         &validateCsqual);
hashAdd(funcs, "broadPeak",      &validateBroadPeak);
hashAdd(funcs, "narrowPeak",     &validateNarrowPeak);
hashAdd(funcs, "gappedPeak",     &validateGappedPeak);
hashAdd(funcs, "bedGraph",       &validateBedGraph);
#ifdef USE_BAM
hashAdd(funcs, "BAM",            &validateBAM);
#endif
hashAdd(funcs, "bigWig",         &validateBigWig);
//hashAdd(funcs, "test", &testFunc);
if (!(func = hashFindVal(funcs, type)))
    reportErrAbort("Cannot validate %s type files\n", type);
validateFiles(func, argc, argv);
return 0;
}
