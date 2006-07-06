/* gbProcess - Takes GenBank flat format (.seq) files and extracts only
 * those that pass a user defined filter.  It puts the sequence in a .fa
 * file and annotations to a .ra (rna annotation) file.
 *
 * Note: This program started out life as gbToFaRa.  It was significant
 * modified to support incremental genbank update and renamed to
 * gbProcess.  While it maintains many of the original functionality
 * not directly used by the incremental update, a lot of this has
 * not been tested.
 *
 * This can also be induced to extract genomic sequences and
 * parse through the genBank comment field to figure out where
 * the contigs are in unfinished sequence submissions.
 *
 * The basic flow of the program is to read in a GenBank record
 * into a recursive data structure (gbField), process the
 * data structure a bit, and then write out the data structure.
 *
 * Previously this program discarded duplicated accessions, however
 * with the automated genbank update, it's desirable to keep all
 * in case one is duplicated.
 *
 * This records certain fields specified to refSeq if they are found.
 * The loc2ref and mim2loc infomation is now extracted from this this
 * file rather than the external files.
 *
 */
/* FIXME: the output file selection is very convoluted.
 * FIXME: doesn't use gbOpenOutput mechanism for ra files; this is
 * due to the reopening of the files for by-prefix output.
 * FIXME: BAC, contig stuff is untested, should probably just move it to
 * another program.
 */
#include "common.h"
#include "portable.h"
#include "hash.h"
#include "linefile.h"
#include "errabort.h"
#include "dnautil.h"
#include "dystring.h"
#include "dnaseq.h"
#include "gbFa.h"
#include "keys.h"
#include "options.h"
#include "gbFilter.h"
#include "gbParse.h"
#include "gbDefs.h"
#include "gbFileOps.h"
#include "gbProcessed.h"

static char const rcsid[] = "$Id: gbProcess.c,v 1.15 2006/07/06 19:20:00 markd Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"filterFile", OPTION_STRING},
    {"filter", OPTION_STRING},
    {"byAccPrefix", OPTION_INT},
    {"gbidx", OPTION_STRING},
    {"pepFa", OPTION_STRING},
    {"inclXMs", OPTION_BOOLEAN},
    {NULL, 0}
};

/* Key/Value Table - Gets filled in as we parse through the
 * genBank record.  Sent to expression evaluator along with
 * filter.keyExp after genBank record is parsed. */
static struct kvt *kvt;

/* Buffers for information that was collected that was parsed and saved
 * in the kvt. Needed because kvt doesn't own memory of values.  Most
 * of kvt memory is in tables in gbParse. (all yuk) */
static struct dyString *dbXrefBuf = NULL;
static struct dyString *omimIdBuf = NULL;
static struct dyString *srcOrgBuf = NULL;
static struct dyString *synOrgBuf = NULL;
static char locusLinkId[64];
static char geneId[64];
static char faOffStr[128], faSizeStr[64];
static char pepSizeStr[64], pepFaOffStr[128], pepFaSizeStr[64];

/* Base names for output files */
static char *faName, *raName, *gbIdxName;
static char faBacDir[PATH_LEN]; /* directory for BACs */

/* If > 0, seperate into files by this many accession characters */
static int gByAccPrefixSize = 0;
static char gCurAccPrefix[32];  /* current prefix characters */

/* Open output files.  If output by organism or acc prefix, a table is
 * kept of the files that have already been created so that they can be
 * appended to all but the first open. */
static struct gbFa *faFile = NULL;
static FILE *raFile = NULL;
static FILE *gbIdxFile = NULL;
static struct hash *openedFiles = NULL;
static struct gbFa *gPepFa = NULL;

static boolean inclXMs = FALSE; /* Should XM_ refseqs be included? */

struct authorExample
/* We keep one of these for each author, the better
 * to check each style of EST. */
    {
    struct authorExample *next; /* Next in list. */
    char *name;                 /* Name of author (or group of authors) */
    int count;                  /* Number of ESTs they've submitted. */
    char accession[32];         /* Accession of one of their ESTs */
    };
struct authorExample *estAuthorList = NULL;

int cmpAuthorExample(const void *va, const void *vb)
/* Compare two authorExamples to sort most prolific ones to top of list. */
{
const struct authorExample *a = *((struct authorExample **)va);
const struct authorExample *b = *((struct authorExample **)vb);
return b->count - a->count;
}

boolean isNcbiDate(char *date)
/* Check if date string is plausibly something like 28-OCT-1999 */
{
char buf[24];
char *words[4];
int wordCount;
int len = strlen(date);

if (len >= sizeof(buf))
    return FALSE;
strcpy(buf, date);
wordCount = chopString(date, "-", words, ArraySize(words));
if (wordCount != 3)
    return FALSE;
return isdigit(words[0][0]) && isalpha(words[1][0]) && isdigit(words[0][0]);
}


void ncbiDateToSqlDate(char *gbDate, char *sqlDate, int sqlDateSize,
                       char *acc)
/* Convert genbank date to a SQL date. */
{
static char *months[] = { "JAN", "FEB", "MAR", "APR", "MAY", "JUN", 
                          "JUL", "AUG", "SEP", "OCT", "NOV", "DEC", };
int monthIx;
char *day = gbDate;
char *month = gbDate+3;
char *year = gbDate+7;

gbDate[2] = gbDate[6] = 0;
for (monthIx = 0; monthIx<12; ++monthIx)
    {
    if (sameString(months[monthIx], month))
	break;
    }
if (monthIx == 12)
    errAbort("Unrecognized month %s for %s", month, acc);
safef(sqlDate, sqlDateSize, "%s-%02d-%s", year, monthIx, day);
}

/* Thes three variables track the contigs in the current sequence. */
int contigCount;            /* Count of contigs in sequence. */
int contigStarts[1024*2];   /* Starts of contigs. */
int contigEnds[1024*2];     /* Ends of contigs. */

void addContig(int start, int end)
/* Add a new contig. */
{
if (contigCount >= ArraySize(contigStarts) )
    errAbort("Too many contigs, can only handle %lld\n", (long long)ArraySize(contigStarts));
contigStarts[contigCount] = start;
contigEnds[contigCount] = end;
++contigCount;
}


void sepByN(DNA *dna, int dnaSize, int nMinCount)
/* Separate into fragments based on runs of at least
 * nMinCount N's. */
{
int i;
DNA b;
int nCount = 0;
boolean isN, lastIsN = TRUE;
boolean open = FALSE;
int nFirstIx = 0;
int firstIx = 0;

for (i=0; i<dnaSize; ++i)
    {
    b = dna[i];
    isN = (b == 'n' || b == 'N');
    if (isN)
        {
        if (lastIsN)
            {
            if (++nCount == nMinCount)
                {
                if (open)
                    {
                    addContig(firstIx, nFirstIx);
                    open = FALSE;
                    }
                }
            }
        else
            {
            nFirstIx = i;
            nCount = 1;
            }
        }
    else
        {
        if (!open)
            {
            firstIx = i;
            open = TRUE;
            }
        }
    lastIsN = isN;
    }
if (open)
    {
    if (contigCount >= ArraySize(contigStarts) )
        errAbort("Too many contigs, can only handle %lld\n", (long long)ArraySize(contigStarts));
    contigStarts[contigCount] = firstIx;
    contigEnds[contigCount] = i;
    ++contigCount;
    }
}

void contigError(char *accession)
/* Tell user there's an error in this contig and die. */
{
errAbort("Error processing contig fragments in %s", accession);
}

boolean addNoteContigs(char *comLine, char *accession, boolean hattoriStyle)
/* Parse contigs in comments that are after Note:
 * This is the style that most submissions from the USA use, and
 * some of the Japanese.  The other popular style separates contigs
 * by runs of NNNN.  */
{
char *s = comLine;
boolean doAsterisk = FALSE;
if (hattoriStyle)
    {
    s = strstr(comLine, "NOTE:");
    }
else
    {
    if ((s = strstr(s, "* NOTE:")) != NULL)
        {
        doAsterisk = TRUE;
        }
    else
        {
        s = strstr(comLine, "NOTE:");
        }
    }
if (s == NULL)
    {
    warn("No NOTE: in %s", accession);
    return FALSE;
    }
s = strstr(s, "preserved");
if (s == NULL)
    errAbort("Can't find preserved marker in %s\n", accession);
for (;;)
    {
    int start, end;
    int i;

    if (doAsterisk)
        {
        if ((s = nextWordStart(s)) == NULL || s[0] != '*')
            break;
        }
    if ((s = nextWordStart(s)) == NULL)
        break;
    if (startsWith("gap", s))
        {
        if ((s = nextWordStart(s)) == NULL)
            contigError(accession);
        if (!startsWith("of", s))
            errAbort("Expecting 'of' after 'gap' in %s", accession);
        if ((s = nextWordStart(s)) == NULL)
            contigError(accession);
        if (!startsWith("unknown", s))
            errAbort("Expecting 'unknown' after 'gap of' in %s", accession);
        if ((s = nextWordStart(s)) == NULL)
            contigError(accession);
        continue;
        }
    if (!isdigit(s[0]))
        break;
    start = atoi(s);
    if ((s = nextWordStart(s)) == NULL || !isdigit(s[0]))
        errAbort("Expecting contig end in %s", accession);
    end = atoi(s);
    if ((s = nextWordStart(s)) == NULL)
        errAbort("Expecting 'contig' or 'gap' in %s", accession);
    if (startsWith("gap", s) )
        {
        if ((s = nextWordStart(s)) == NULL)
            contigError(accession);
        if (!startsWith("of", s))
            errAbort("Expecting 'of' after 'gap' in %s", accession);
        if ((s = nextWordStart(s)) == NULL)
            contigError(accession);
        if (startsWith("unknown", s))
            {
            if ((s = nextWordStart(s)) == NULL)
                contigError(accession);
            }
        else if (isdigit(s[0]))
            {
            if ((s = nextWordStart(s)) == NULL)
                contigError(accession);
            if (startsWith("bp", s))
                {
                if ((s = nextWordStart(s)) == NULL)
                    contigError(accession);
                }
            }
        else
            {
            errAbort("Expecting 'unknown' or number after 'gap of' in %s", accession);
            }
        continue;
        }
    else if (!startsWith("contig", s))
        {
        errAbort("Expecting 'contig' or 'gap' in %s", accession);
        }
    if ((s = nextWordStart(s)) == NULL || !startsWith("of", s) )
        errAbort("Expecting 'of' in %s", accession);
    for (i=0; i<4; ++i)
        {
        if ((s = nextWordStart(s)) == NULL)
            contigError(accession);
        }
    addContig(start-1, end);
    }
return contigCount > 0;
}

void beutenErr(char *accession)
/* Complain about an error in Beutenbergstrasse style contig. */
{
errAbort("Error in Beutenbergstrasse contig definition for %s", accession);
}

boolean addBeutenContigs(char *comLine, char *accession)
/* Add in the quirky style of the center on Beutenbergstrasse:
   One example of their style is:
    COMMENT     1-54037: contig of 54037 bp; 54037-54038: gap of unknown size;
                54038-63480: contig of 9443 bp (orientation unknown); 63480-63481:
                gap of unknown size; 63481-139931: contig of 76452 bp;
                139931-139932: gap of 189 bp (by alignment to 70I24 = AF064860);
                139932-141643: contig of 1712 bp.
   Unfortunately they pretty clearly enter in the comment field by hand.
   Sometimes they'll throw in a Pos. or pos. before the numbers.  Sometimes they'll
   put a space in between the numbers and the dash.  Sometimes they'll use
   the word "to" instead of the dash.  Sometimes they'll leave out the colon
   after the second number. Sometimes they'll just leave out the contigs
   altogether.
 */
{
char *s = comLine, *x, *y, *z;
char *styleName = "Beutenbergstrasse";

if (strstr(comLine, "* NOTE: This is a 'working"))
    return addNoteContigs(comLine, accession, FALSE);
if ((s = strstr(comLine, "os. 1")) != NULL)
    {
    for (;;)
        {
        int start, end;
        char *t, *u;
        s = strstr(s, "os.");
        if (s == NULL)
            break;
        s = skipLeadingSpaces(s+4);
        if (!isdigit(*s))
            errAbort("Expecting number in %s style %s", styleName, accession);
        start = atoi(s);
        /* It could either be XXX to YYY or XXX-YYY or XXX...YYY */
        t = skipTo(s, "-.");
        u = strstr(s, "to");
        if (t == NULL && u == NULL)
            beutenErr(accession);
        if (t != NULL && u != NULL)
            {
            if (t < u)
                u = NULL;
            else
                t = NULL;
            }
        if (t != NULL)
            {
            s = skipThrough(t+1, "-. \t");
            }
        else
            {
            if ((s = nextWordStart(u)) == NULL)
                beutenErr(accession);
            }
        if (!isdigit(*s))
            errAbort("Expecting number in %s style %s", styleName, accession);
        end = atoi(s);
        if ((s = nextWordStart(s)) == NULL)
            beutenErr(accession);
        if (startsWith("contig", s) || startsWith("Inbetween", s) )
            addContig(start, end);
        else if (startsWith("gap", s))
            ;
        else
            beutenErr(accession);
        }
    return TRUE;
    }
else if ((s = strstr(comLine, ": contig")) != NULL || (s = strstr(comLine, "contig of")) != NULL)
    {
    char *separator;
    int sepLen;
    if (startsWith(": contig", s))
        separator = ": contig";
    else
        separator = "contig of";
    sepLen = strlen(separator);
    for (;;)
        {
        /* Find 'contig' and work backwards to XXX-YYY: or XXX - YYY */
        z = strstr(s, separator);
        if (z == NULL)
            break;
        *z = 0;
        y = z;
        for (;;)
            {
            if (--y == s)
                errAbort("Expecting - in %s style %s", styleName, accession);
            if (y[0] == '-')
                break;
            }
        *y = 0;
        x = y;
        y = skipLeadingSpaces(y + 1);
        while (isspace(x[-1]) && x-1 > comLine)
            --x;
        for (;;)
            {
            if (--x == comLine)
                break;
            if (isspace(*x))
                {
                ++x;
                break;
                }
            }
        if (!isdigit(*x) || !isdigit(*y))
            errAbort("Expecting number in %s style %s", styleName, accession);
        addContig(atoi(x)-1, atoi(y));
        s = z + sepLen;
        }
    return TRUE;
    }
else if ((s = strstr(comLine, "contig 1:")) != NULL)
    {
    char sbuf[16];
    int slen;
    int cix = 1;
    int start, end;
    char *numberSep = ".-\t ";
    for (;;)
        {
        slen = safef(sbuf, sizeof(sbuf), "contig %d:", cix++);
        if ((s = strstr(s, sbuf)) == NULL)
            break;
        s = skipLeadingSpaces(s + slen);
        if (!isdigit(s[0]))
            beutenErr(accession);
        start = atoi(s);
        if ((s = skipTo(s, numberSep)) == NULL)
            beutenErr(accession);
        if ((s = skipThrough(s, numberSep)) == NULL)
            beutenErr(accession);
        if (!isdigit(s[0]))
            beutenErr(accession);
        end = atoi(s);
        addContig(start-1, end);
        }
    }
else if (startsWith("* NOTE:", comLine))
    {
    return FALSE;
    }
else if ((s = strstr(comLine, "* NOTE:")) != NULL && s - comLine < 100)
    {
    return FALSE;
    }
else
    {
    warn("Unexpected %s style on %s", styleName, accession);
    return FALSE;
    }
return FALSE;
}

/* Get a ra file. */


void bacWrite(char *faDir, char *accession, int version, DNA *dna)
/* Write all the contigs of one BAC to a file. */
{
char fileName[PATH_LEN];
struct gbFa *fa;
char accVer[64];
int i;
int start, end, size;

safef(fileName, sizeof(safef), "%s/%s.fa", faDir, accession);
fa = gbFaOpen(fileName, "w");
if (contigCount <= 1)
    {
    safef(accVer, sizeof(accVer), "%s.%d", accession, version);
    gbFaWriteSeq(fa, accVer, NULL, dna, -1);
    }
else
    {
    for (i=0; i<contigCount; ++i)
        {
        start = contigStarts[i];
        end = contigEnds[i];
        size = end - start;
        assert(size > 0);
        safef(accVer, sizeof(accVer), "%s.%d.%d", accession, version, i+1);
        gbFaWriteSeq(fa, accVer, NULL, dna+start, size);
        }
    }
gbFaClose(&fa);
}

boolean isThreePrime(char *s)
/* Return TRUE if s looks to have words three prime in it. */
{
if (s == NULL)
    return FALSE;
return stringIn("3'", s) || stringIn("Three prime", s) 
        || stringIn("three prime", s) || stringIn("3 prime", s);
}

boolean isFivePrime(char *s)
/* Return TRUE if s looks to have words five prime in it. */
{
if (s == NULL)
    return FALSE;
return stringIn("5'", s) || stringIn("Five prime", s) 
        || stringIn("five prime", s) || stringIn("5 prime", s);
}

char *getEstDir(char *def, char *com)
/* Return EST direction as deduced from definition and comment lines. */
{
char *three = "3'";
char *five = "5'";
char *dir = NULL;
boolean gotThreeDef = FALSE, gotFiveDef = FALSE;
boolean gotThreeCom = FALSE, gotFiveCom = FALSE;

gotThreeDef = isThreePrime(def);
gotFiveDef = isFivePrime(def);
if (gotThreeDef ^ gotFiveDef)
    dir = (gotThreeDef ? three : five);
if (dir == NULL)
    {
    gotThreeCom = isThreePrime(com);
    gotFiveCom = isFivePrime(com);
    if (gotThreeCom ^ gotFiveCom)
        dir = (gotThreeCom ? three : five);
    }
/* either didn't get 5' or 3' in def and com, or got both */
return dir;
}

void procHumanBac(DNA *dna, int dnaSize)
/* Special handling for human BAC fields.  This will add values to kvt global
 * and fill in contig* globals. */
{
char *org = gbOrganismField->val->string;
char *keys = gbKeywordsField->val->string;
char *journal = gbJournalField->val->string;
char *com = gbCommentField->val->string;
char *accession = gbAccessionField->val->string;
boolean isFragmented = FALSE;
int nSeparateFrags = 0;
char *center = NULL;
boolean isNoted = FALSE;
boolean hattoriStyle = FALSE;
boolean beutenStyle = FALSE;
struct dyString *ctgDs = newDyString(512);
char frg[16];

if (keys != NULL)
    {
    if (strstr(keys, "htgs_phase1"))
        {
        kvtAdd(kvt, "pha", "1");
        isFragmented = TRUE;
        }
    else if (strstr(keys, "htgs_phase2"))
        {
        kvtAdd(kvt, "pha", "2");
        isFragmented = TRUE;
        }
    else if (strstr(keys, "htgs_phase3"))
        {
        kvtAdd(kvt, "pha", "3");
        }
    else if (strstr(keys, "htgs_phase0"))
        {
        kvtAdd(kvt, "pha", "0");  /* So fragmented we don't bother. */
        }
    }
if (journal != NULL)
    {
    if (strstr(journal, "Sanger"))
        {
        center = "Sanger";
        nSeparateFrags = 100;
        }
    else if (strstr(journal, "Wellcome Trust"))
        {
        center = "Wellcome";
        nSeparateFrags = 100;
        }
    else if (strstr(journal, "Washington University"))
        {
        center = "WashU";
        }
    else if (strstr(journal, "Beutenbergstrasse"))
        {
        center = "Beutenbergstrasse";
        beutenStyle = TRUE;
        }
    }                        
if (center == NULL)
    {
    char *authors = gbAuthorsField->val->string;
    if (authors != NULL)
        {
        if (strstr(authors, "Lander,E"))
            {
            center = "Whitehead";
            }
        else if (strstr(authors, "Waterston,R"))
            {
            center = "WashU";
            }
        else if (strstr(authors, "Muzny,D.M., Adams,C."))
            {
            center = "Baylor";
            }
        else if (strstr(authors, "Genoscope"))
            {
            center = "Genoscope";
            nSeparateFrags = 100;
            }
        else if (strstr(authors, "DOE Joint"))
            {
            center = "DOE";
            }
        else if (strstr(authors, "Hattori,M.") )
            {
            center = "Hattori";
            hattoriStyle = TRUE;
            }
        else if (strstr(authors, "Shimizu,N"))
            {
            center = "Shimizu";
            nSeparateFrags = 10;
            }
        else if (strstr(authors, "Blechschmidt,K.")
                 ||   strstr(authors, "Baumgart,C.")
                 ||   strstr(authors, "Polley,A.") 
                 ||   strstr(authors, "Schudy,A.") )
            {
            center = "Beutenbergstrasse";
            beutenStyle = TRUE;
            }
        }
                
    }
if (center != NULL)
    kvtAdd(kvt, "cen", center);
if (isFragmented && org != NULL )
    {
    if (com != NULL)
        {
        if (nSeparateFrags)
            ;   /* Do nothing yet... */
        else
            {
            if (beutenStyle)
                addBeutenContigs(com, accession); 
            else if (addNoteContigs(com, accession, hattoriStyle))
                isNoted = TRUE;
            else
                nSeparateFrags = 10;
            }   
        }
    }

if (nSeparateFrags)
    sepByN(dna, dnaSize, nSeparateFrags);
if (isFragmented)
    {
    if (contigCount == 0)
        contigCount = 1;
    if (contigCount == 1 && !isNoted)
        {
        if (com == NULL || (!strstr(com, "n's") && !strstr(com, "N's")))
            warn("*** Only one contig in %s ***", accession);
        }
    safef(frg, sizeof(frg), "%d", contigCount);
    kvtAdd(kvt, "frg", frg);
    if (contigCount > 1)
        {
        int i;
        dyStringClear(ctgDs);
        for (i=0; i<contigCount; ++i)
            {
            dyStringPrintf(ctgDs, "%d-%d ", contigStarts[i], contigEnds[i]);
            }
        kvtAdd(kvt, "ctg", ctgDs->string);
        }
    }
freeDyString(&ctgDs);
}

boolean isFirstOpen(char *path)
/* Check if this is the first open, and record that file has been opened */
{

if (openedFiles == NULL)
    openedFiles = hashNew(18);
if (hashLookup(openedFiles, path) != NULL)
    return FALSE;
hashAdd(openedFiles, path, NULL);
return TRUE;
}

void makeAccPrefixedFile(char* accPrefix, char *inFile, char *prefixedFile)
/* generate an output with the prefixed included  */
{
char *sep = strrchr(inFile, '/');
char *fname, *rest;
prefixedFile[0] = '\0';

/* Get directory */
if (sep != NULL)
    {
    *sep = '\0';
    strcpy(prefixedFile, inFile);
    *sep = '/';
    strcat(prefixedFile, "/");
    fname = sep+1;
    }
else
    fname = inFile;  /* no directory */

/* copy first word of file, upto a dot */
sep = strchr(fname, '.');
if (sep != NULL)
    {
    *sep = '\0';
    strcat(prefixedFile, fname);
    *sep = '.';
    rest = sep+1;
    }
else
    rest = "";

/* append the accession prefix */
strcat(prefixedFile, ".");
strcat(prefixedFile, accPrefix);
strcat(prefixedFile, ".");

/* add back in the rest */
strcat(prefixedFile, rest);
}

void openByAccPrefix(char* accPrefix)
/* Open up the by accession prefix */
{
char *mode;
char raPath[PATH_LEN], faPath[PATH_LEN], gbIdxPath[PATH_LEN];

carefulClose(&raFile);
gbFaClose(&faFile);
if (gbIdxName != NULL)
    carefulClose(&gbIdxFile);
                    
makeAccPrefixedFile(accPrefix, raName, raPath);
mode = isFirstOpen(raPath) ? "w" : "a";
raFile = mustOpen(raPath, mode);

makeAccPrefixedFile(accPrefix, faName, faPath);
faFile = gbFaOpen(faPath, mode);

if (gbIdxName != NULL)
    {
    makeAccPrefixedFile(accPrefix, gbIdxName, gbIdxPath);
    gbIdxFile = mustOpen(gbIdxPath, mode);
    }
strcpy(gCurAccPrefix, accPrefix);
}

void setupOutputFiles(char *acc, char *org, struct filter *filter)
/* Get the output files (in globals) for a sequence, opening as needed. */
{
if (gByAccPrefixSize > 0)
    {
    char accPrefix[32];
    strncpy(accPrefix, acc, gByAccPrefixSize);
    accPrefix[gByAccPrefixSize] = '\0';
    tolowers(accPrefix);
    if (!sameString(accPrefix, gCurAccPrefix))
        openByAccPrefix(accPrefix);
    }
else
    {
    /* output to a single set of files */
    if (raFile == NULL)
        {
        raFile = mustOpen(raName, "w");
        if (!filter->isBAC)
            faFile = gbFaOpen(faName, "w");
        if (gbIdxName != NULL)
            gbIdxFile = mustOpen(gbIdxName, "w");
        }
    }
}

struct slName* parseDbXrefStr(char* xrefStr)
/* Split while-space seperated list of db_xref values into a list */
{
struct slName* head = NULL;
char *xref;

while ((xref = nextWord(&xrefStr)) != NULL)
    slSafeAddHead(&head, newSlName(xref));
return head;
}

void updateKvt(struct keyVal **kvPtr, char* name, char *val)
/* add or update at kvt value */
{
if (*kvPtr != NULL)
    (*kvPtr)->val = val;
else 
    *kvPtr = kvtAdd(kvt, name, val);
}

void parseDbXrefs()
/* Parse the db_xref entries for various features to build a single dbx entry
 * in the kvt and to obtain the locus and mim ids for the kvt */
{
static char* LOCUS_ID = "LocusID:";
static char* GENE_ID = "GeneID:";
static char* MIM_ID = "MIM:";
struct slName* head = NULL, *xref, *prevXref;
struct keyVal* dbXrefKv = NULL;
struct keyVal* locusLinkIdKv = NULL;
struct keyVal* geneIdKv = NULL;
struct keyVal* omimIdKv = NULL;
if (dbXrefBuf == NULL)
    dbXrefBuf = dyStringNew(256);
dyStringClear(dbXrefBuf);
if (omimIdBuf == NULL)
    omimIdBuf = dyStringNew(256);
dyStringClear(omimIdBuf);
locusLinkId[0] = '\0';

/* split into a list and sort so we can remove dups */
if (gbCdsDbxField->val->stringSize > 0)
    head = slCat(head, parseDbXrefStr(gbCdsDbxField->val->string));
if (gbGeneDbxField->val->stringSize > 0)
    head = slCat(head, parseDbXrefStr(gbGeneDbxField->val->string));
slNameSort(&head);

xref = head;
prevXref = NULL;
while (xref != NULL)
    {
    /* skip is dup of previous */
    if ((prevXref == NULL) || !sameString(prevXref->name, xref->name))
        {
        if (dbXrefBuf->stringSize > 0)
            dyStringAppendC(dbXrefBuf, ' ');
        dyStringAppend(dbXrefBuf, xref->name);
        updateKvt(&dbXrefKv, "dbx", dbXrefBuf->string);

        /* find number in db_xref like LocusID:27 or GeneID:27 */
        if (startsWith(LOCUS_ID, xref->name))
            {
            safef(locusLinkId, sizeof(locusLinkId), "%s",
                  xref->name+strlen(LOCUS_ID));
            updateKvt(&locusLinkIdKv, "loc", locusLinkId);
            }
        else if (startsWith(GENE_ID, xref->name))
            {
            safef(geneId, sizeof(geneId), "%s",
                  xref->name+strlen(GENE_ID));
            updateKvt(&geneIdKv, "gni", geneId);
            }
        else if (startsWith(MIM_ID, xref->name))
            {
            if (omimIdBuf->stringSize > 0)
                dyStringAppendC(omimIdBuf, ' ');
            dyStringAppend(omimIdBuf, xref->name+strlen(MIM_ID));
            updateKvt(&omimIdKv, "mim", omimIdBuf->string);
            }
        }
    prevXref = xref;
    xref = xref->next;
    }
slFreeList(&head);
}

void parseSourceOrganism()
/* parse source /organism fields, output as srcOrg if different from org */
{
int numOrgs, i;
char **orgs;
if (gbSourceOrganism->val->stringSize == 0)
    return;
if (srcOrgBuf == NULL)
    srcOrgBuf = dyStringNew(256);
dyStringClear(srcOrgBuf);

numOrgs = chopString(gbSourceOrganism->val->string, ";", NULL, 0);
AllocArray(orgs, numOrgs);
chopString(gbSourceOrganism->val->string, ";", orgs, numOrgs);
for (i = 0; i < numOrgs; i++)
    {
    if (!sameString(orgs[i], gbOrganismField->val->string))
        {
        if (srcOrgBuf->stringSize > 0)
            dyStringAppendC(srcOrgBuf, ';');
        dyStringAppend(srcOrgBuf, orgs[i]);
        }
    }
freeMem(orgs);
if (srcOrgBuf->stringSize > 0)
    kvtAdd(kvt, "srcOrg", srcOrgBuf->string);
}

char *findSyntheticTarget()
/* for a synthetic sequence, attempt to find the targete organism.  This was
 * added to support the ORFeome clones.  In general, there is no simple way to
 * determine an organism that a synthenic clone targets. */
{
struct keyVal *kv;
if (synOrgBuf == NULL)
    synOrgBuf = dyStringNew(256);
dyStringClear(synOrgBuf);

kv = kvtGet(kvt, "srcOrg");
if (kv != NULL)
    dyStringAppend(synOrgBuf, kv->val);

if (synOrgBuf->stringSize > 0)
    {
    kvtAdd(kvt, "synOrg", synOrgBuf->string);
    return synOrgBuf->string;
    }
else
    return NULL;
}


void writePepSeq()
/* If information is available, write the peptide sequence and
 * save offset and size in kvt */
{
if ((gPepFa != NULL) && (gbProteinIdField->val->stringSize > 0)
    && (gbTranslationField->val->stringSize > 0))
    {
    int faSize;
    gbFaWriteSeq(gPepFa, gbProteinIdField->val->string, NULL,
                 gbTranslationField->val->string, -1);
    
    safef(pepSizeStr, sizeof(pepSizeStr), "%u", 
          gbTranslationField->val->stringSize);
    kvtAdd(kvt, "prs", pepSizeStr);

    safef(pepFaOffStr, sizeof(pepFaOffStr), "%lld", (long long)gPepFa->recOff);
    kvtAdd(kvt, "pfo", pepFaOffStr);

    faSize = gPepFa->off - gPepFa->recOff;
    safef(pepFaSizeStr, sizeof(pepFaSizeStr), "%d", faSize);
    kvtAdd(kvt, "pfs", pepFaSizeStr);
    }
}
        

void procGbEntry(struct lineFile *lf, struct hash *estAuthorHash,
                 struct filter *filter)
/* process one entry in the genbank file . readGbInfo should be called
 * first */
{
char *words[16];
char date[64];
int wordCount;
DNA *dna = NULL;
int dnaSize;
char sizeString[16];
char accVer[64];
int faSize;
char *locus = gbLocusField->val->string;
char *accession = gbAccessionField->val->string;
int version = 0;
char *gi = NULL;
char *verChar = gbVersionField->val->string;
char *s;
char *org = gbOrganismField->val->string;
char *synOrg = NULL;
struct keyVal *seqKey, *sizeKey, *commentKey;
boolean isEst = FALSE, keepIt;
char verNum[8];
char *com = gbCommentField->val->string;

if (locus == NULL || accession == NULL)
    errAbort("No LOCUS or no ACCESSION line near %d of %s",
             lf->lineIx, lf->fileName);
    
/* Chop off all but first word of accession. */
s = skipLeadingSpaces(accession);
if (s != NULL)
    s = skipLeadingNonSpaces(s);
if (s != NULL)
    *s = 0;

/* Get version field (defaults to zero) */
if (verChar != NULL)
    {
    char *parts[2];
    char *accVer;
    int partCount;

    partCount = chopByWhite(verChar, parts, ArraySize(parts));

    /* Version is number after dot. */
    accVer = parts[0];
    if ((accVer = strchr(accVer, '.')) != NULL)
        version = atoi(accVer+1);
    if (partCount >= 2 && startsWith("GI:", parts[1]))
        gi = parts[1]+3;
    }

gbfFlatten(kvt);
                
/* Get additional keys. */
if (com != NULL)
    {
    if (startsWith("REVIEWED", com))
        kvtAdd(kvt, "cur", "yes");
    }
safef(verNum, sizeof(verNum), "%d", version);
kvtAdd(kvt, "ver", verNum);
if (gi != NULL)
    kvtAdd(kvt, "ngi", gi);
wordCount = chopLine(locus, words);
if (wordCount >= 6)
    {
    kvtAdd(kvt, "mol", words[3]);
    kvtAdd(kvt, "cat", words[wordCount-2]);
    ncbiDateToSqlDate(words[wordCount-1], date, sizeof(date), accession);
    kvtAdd(kvt, "dat", date);
    }
else if (wordCount == 5 && sameString(words[2], "bp") && isdigit(words[1][0]))
    {
    /* Check carefully.  Probably it's just missing the molecule type... */
    if (!isNcbiDate(words[4]))
        {
        errAbort("Strange LOCUS line in %s accession %s",
                 lf->fileName, accession);
        }
    kvtAdd(kvt, "cat", words[3]);
    ncbiDateToSqlDate(words[4], date, sizeof(date), accession);
    kvtAdd(kvt, "dat", date);
    }
else if (wordCount == 5 && sameString(words[2], "bp") && isdigit(words[1][0]))
    {
    char *mol = words[3];
    if (sameString(mol, "mRNA") || sameString(mol, "DNA"))
        kvtAdd(kvt, "mol", mol);
    }
else
    {
    int i;
    uglyf("Arghh!\n ");
    for (i=0; i<wordCount; ++i)
        uglyf("'%s' ", words[i]);
    uglyf("\n");
    errAbort("Short LOCUS line in %s accession %s",
             lf->fileName, accession);
    }
if (((wordCount >= 5) && sameString(words[4], "EST")) || 
    ((wordCount >= 6) && sameString(words[5], "EST")))
    {
    /* Try and figure out if it's a 3' or 5' EST */
    char *dir = getEstDir(gbDefinitionField->val->string, com);
    if (dir != NULL)
        kvtAdd(kvt, "dir", dir);
    isEst = TRUE;
    }

/* Handle other fields */
parseDbXrefs();
parseSourceOrganism();

if (startsWith("synthetic construct", gbOrganismField->val->string))
    synOrg = findSyntheticTarget();

/* for refseqs, we only keep NM_ */
if (gbGuessSrcDb(accession) == GB_REFSEQ)
    {
    keepIt = startsWith("NM_", accession) || (inclXMs && startsWith("XM_", accession));
    }
else
    keepIt = TRUE;

if (keepIt && ((filter->keyExp == NULL) || keyExpEval(filter->keyExp, kvt)))
    {
    /* Handle sequence part of read. */
    contigCount = 0;
    dna = gbfReadSequence(lf, &dnaSize);
    }
/* just discard if no sequence */
if (dna != NULL)
    {
    seqKey = kvtAdd(kvt, "seq", dna);
    safef(sizeString, sizeof(sizeString), "%d", dnaSize);
    sizeKey = kvtAdd(kvt, "siz", sizeString);
    
    if ((filter->isBAC) && (org != NULL) && sameString(org, "Homo sapiens"))
        procHumanBac(dna, dnaSize);

    if (isEst)
        {
        char *author = gbAuthorsField->val->string;
        if (author != NULL)
            {
            struct authorExample *ae;
            struct hashEl *hel;
            if ((hel = hashLookup(estAuthorHash, author)) == NULL)
                {
                AllocVar(ae);
                hel = hashAdd(estAuthorHash, author, ae);
                ae->name = hel->name;
                ae->count = 1;
                strncpy(ae->accession, accession, sizeof(ae->accession));
                slAddHead(&estAuthorList, ae);
                }
            else
                {
                ae = hel->val;
                ae->count += 1;
                }
            }
        }
    seqKey->val = NULL; /* Don't write out sequence here. */
    commentKey = kvtGet(kvt, "com");
    if (commentKey != NULL)
        commentKey->val = NULL;  /* Don't write out comment either. */

    setupOutputFiles(accession, org, filter);

    if (filter->isBAC)
        {
        printf("bacWrite()\n");
        bacWrite(faBacDir, accession, version, dna);
        }
    if (faFile != NULL)
        {
        /* save fasta offset, size in ra */
        safef(accVer, sizeof(accVer), "%s.%d", accession, version);
        gbFaWriteSeq(faFile, accVer, NULL, dna, -1);
        faSize = faFile->off - faFile->recOff;
        safef(faOffStr, sizeof(faOffStr), "%lld", (long long)faFile->recOff);
        kvtAdd(kvt, "fao", faOffStr);
        safef(faSizeStr, sizeof(faSizeStr), "%d", faSize);
        kvtAdd(kvt, "fas", faSizeStr);
        }
    if (gPepFa != NULL)
        {
        /* must write before writing kvt */
        writePepSeq();
        }
    kvtWriteAll(kvt, raFile, filter->hideKeys);
    if (gbIdxFile != NULL)
        {
        /* use synthetic target if it was determined */
        gbProcessedWriteIdxRec(gbIdxFile, accession, version,
                               kvtLookup(kvt, "dat"),
                               ((synOrg != NULL) ? synOrg : org));
        }
    }
else
    gbfSkipSequence(lf);
}

void procOneGbFile(char *inName, struct hash *estAuthorHash,
                   struct filter *filter)
/* Process one genBank file into fa and ra files. */
{
struct lineFile *lf = gzLineFileOpen(inName);

while (gbfReadFields(lf))
    procGbEntry(lf, estAuthorHash, filter);
gzLineFileClose(&lf);
}

int cmpUse(const void *va, const void *vb)
/* Compare two gbFieldUseCounters. */
{
const struct gbFieldUseCounter *a = *((struct gbFieldUseCounter **)va);
const struct gbFieldUseCounter *b = *((struct gbFieldUseCounter **)vb);
int dif = a->useCount - b->useCount;
if (dif != 0)
        return dif;
return strcmp(b->val, a->val);
}

void usage()
/* Explain usage and exit */
{
errAbort("gbProcess - Convert GenBank flat format file to an fa file containing\n"
         "the sequence data, and a ra file containing other relevant info.\n"
         "usage:\n"
         "   gbProcess [options] faFile raFile genBankFile(s)\n"
         "where filterFile is definition of which records and fields\n"
         "to use or the word null if you want no filtering."
         "options:\n"
         "     -filterFile=file - filter file\n"
         "     -filter=filter - filter expressions, lines seperated by semi-colons\n"
         "     -inclXMs - don't drop XM entries\n"
         "     -byAccPrefix=n - separate into files by the first n,\n"
         "      case-insensitive letters of the accession\n"
         "     -gbidx=name - Make an index file byte this name\n"
         "     -pepFa=fa - write peptide products to this fasta\n"
         "      file, massaging the ids and recording the offsets in the\n"
         "      ra file\n"
         "This will read compressed input, but does not write compressed\n"
         "output due to need to append with byAccPrefix.\n");
}


int main(int argc, char *argv[])
/* Check parameters, set up, loop through each GenBank file. */
{
char *filterFile, *filterExpr;
struct filter *filter;
char *gbName;
int argi = 1;
struct hash *estAuthorHash = NULL;
char *pepFa;

optionInit(&argc, argv, optionSpecs);
if (argc < 4)
    usage();

gByAccPrefixSize = optionInt("byAccPrefix", 0);
gbIdxName = optionVal("gbidx", NULL);
pepFa = optionVal("pepFa", NULL);
filterFile = optionVal("filterFile", NULL);
filterExpr = optionVal("filter", NULL);
if ((filterFile != NULL) && (filterExpr != NULL))
    errAbort("can't specify both -filterFile and -filter");
inclXMs = optionExists("inclXMs");

if (gByAccPrefixSize > 4)  /* keep small to avoid tons of open files */
    errAbort("max value of -byAccPrefix is 4");

gCurAccPrefix[0] = '\0';

faName = argv[argi++];
strncpy(faBacDir, faName, sizeof(faBacDir));
raName = argv[argi++];

if (filterFile != NULL)
    filter = makeFilterFromFile(filterFile);
else if (filterExpr != NULL)
    filter = makeFilterFromString(filterExpr);
else
    filter = makeFilterEmpty();
estAuthorHash = newHash(10);
kvt = newKvt(128);
gbfInit();

if (pepFa != NULL)
    gPepFa = gbFaOpen(pepFa,"w");

while (argi < argc)
    {
    gbName = argv[argi++];
    printf("Processing %s into %s and %s\n", gbName, faName, raName);
    procOneGbFile(gbName, estAuthorHash, filter);
    }

gbFaClose(&faFile);
gbFaClose(&gPepFa);
carefulClose(&raFile);
carefulClose(&gbIdxFile);

return 0;
}
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

