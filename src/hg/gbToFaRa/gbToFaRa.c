/* GbToFaRa - Takes GenBank flat format (.seq) files and extracts only
 * those that pass a user defined filter.  It puts the sequence in a .fa
 * file and annotations to a .ra (rna annotation) file.  It
 * also keeps some statistics on the things found in the
 * genBank files and puts them in a .ta file.
 *
 * This can also be induced to extract genomic sequences and
 * parse through the genBank comment field to figure out where
 * the contigs are in unfinished sequence submissions.
 *
 * The basic flow of the program is to read in a GenBank record
 * into a recursive data structure (gbField), process the
 * data structure a bit, and then write out the data structure.
 */

#include "common.h"
#include "portable.h"
#include "hash.h"
#include "linefile.h"
#include "dnautil.h"
#include "dystring.h"
#include "dnaseq.h"
#include "fa.h"
#include "keys.h"
#include "options.h"


/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"byOrganism", OPTION_STRING},
    {"faInclVer", OPTION_BOOLEAN},
    {NULL, 0}
};

enum formatType
/* Are we working on genomic sequence or mRNA?  Need to write
 * one big .fa file, or a separate one for each sequence. */
    {
    ftUnknown,	/* Default.  Useful for mRNA. Extracts sequence into one big file. */
    ftBac,	/* For genomic. Extracts sequence into separate files. */
    ftEst,      /* Sequence into one big file.  Some special EST oriented processing. */
    };

enum formatType formatType = ftUnknown;		/* Current format type - set at beginning. */

struct gbFieldUseCounter
/* For some fields we use this to keep track of all
 * values in that field and how often they are used. */
    {
    struct gbFieldUseCounter *next;	/* Next in list. */
    char *val;				/* A value seen in this field. */
    int useCount;			/* How many times it's been seen */
    };

struct gbField
/* Structure that helps parse one field of a genBank file. */    
    {
    struct gbField *next;       /* Next field. */
    struct gbField *children;   /* First subfield if any. */

    struct hash *valHash;       /* Hash of all vals seen (may be null) */
    struct gbFieldUseCounter *valList;  /* List of all vals (may be null) */
    int valAlloc;               /* Size of space allocated for value. */
    char *valBuf;               /* Buffer for value. */
    char *val;                  /* Value itself (may be null if not read). */
    boolean multiLine;          /* True if can be multiple line. */
    boolean endsAtSlash;	/* True if multiple line query broken by '/' */
    boolean toLower;			/* Force to lower case. */

    char *readName;               /* GenBank's name for field. */
    char *writeName;               /* Jim's name for field.  If null field not saved. */
    };

struct filter
/* Controls which records are chosen and which fields are written. */
    {
    struct keyExp *keyExp;	/* If this expression evaluates true, the record is included. */
    struct slName *hideKeys;    /* A list of keys to not write out. */
    };

struct kvt *kvt;	/* Key/Value Table - Gets filled in as we parse through the
                         * genBank record.  Sent to expression evaluator along with
			 * filter.keyExp after genBank record is parsed. */

static boolean gFaInclVer;  // include version in fasta

struct gbField *newGbField(char *readName, char *writeName, 
    boolean trackVal, boolean multiLine, int initValSize)
/* Make a new gbField struct. */
{
struct gbField *gbf;
AllocVar(gbf);
gbf->readName = readName;
gbf->writeName = writeName;
if (initValSize <= 0)
    initValSize = 32;
gbf->valAlloc = initValSize;
gbf->valBuf = needMem(initValSize);
if (trackVal)
    gbf->valHash = newHash(0);
gbf->multiLine = multiLine;
return gbf;
}

/* Some fields we'll want to use directly. */
struct gbField *gbStruct;		/* Root of field tree. */
struct gbField *locusField;	
struct gbField *definitionField;
struct gbField *accessionField;
struct gbField *versionField;
struct gbField *commentField;
struct gbField *authorsField;
struct gbField *journalField;
struct gbField *keywordsField;
struct gbField *organismField;
struct gbField *tissueField;
struct gbField *sexField;
struct gbField *devStageField;
struct gbField *cloneField;
struct gbField *chromosomeField;
struct gbField *mapField;

/* Flag indicating whether we are separating the output by organism */
static boolean gByOrganism = FALSE;

/* If the above flag is TRUE then this is our output dir */
static char *gOutputDir = NULL;

void makeGbStruct()
/* Create a heirarchical structure for parsing genBank fields. */
{
struct gbField *gbs = NULL;
struct gbField *c0, *c1, *c2;

locusField = c0 = newGbField("LOCUS", NULL, FALSE, FALSE, 16);
slAddTail(&gbs, c0);

accessionField = c0 = newGbField("ACCESSION", "acc", FALSE, FALSE, 32);
slAddTail(&gbs, c0);

versionField = c0 = newGbField("VERSION", NULL, FALSE, FALSE, 32);
slAddTail(&gbs, c0);

definitionField = c0 = newGbField("DEFINITION", "def", FALSE, TRUE, 1024);
slAddTail(&gbs, c0);

keywordsField = c0 = newGbField("KEYWORDS", "key", TRUE, FALSE, 1024);
slAddTail(&gbs, c0);

c0 = newGbField("SOURCE", "src", TRUE, FALSE, 128);
slAddTail(&gbs, c0);
    organismField = c1 = newGbField("ORGANISM", "org", TRUE, FALSE, 64);
    slAddTail(&c0->children, c1);

c0 = newGbField("REFERENCE", NULL, FALSE, FALSE, 64);
slAddTail(&gbs, c0);
    authorsField = c1 = newGbField("AUTHORS", "aut", TRUE, TRUE, 4*1024);
    slAddTail(&c0->children, c1);

    journalField = c1 = newGbField("JOURNAL", NULL, FALSE, TRUE, 256);
    slAddTail(&c0->children, c1);

commentField = c0 = newGbField("COMMENT", "com", FALSE, TRUE, 8*1024);
slAddTail(&gbs, c0);

c0 = newGbField("FEATURES", NULL, FALSE, FALSE, 128);
slAddTail(&gbs, c0);
    c1 = newGbField("source", NULL, FALSE, FALSE, 128);
    slAddTail(&c0->children, c1);
        c2 = newGbField("/clone_lib", "lib", TRUE, FALSE, 128);
        slAddTail(&c1->children, c2);

        cloneField = c2 = newGbField("/clone", "clo", FALSE, FALSE, 128);
        slAddTail(&c1->children, c2);
        
        sexField = c2 = newGbField("/sex", "sex", TRUE, FALSE, 128);         
        slAddTail(&c1->children, c2);

        tissueField = c2 = newGbField("/tissue_type", "tis", TRUE, TRUE, 128);
        slAddTail(&c1->children, c2);

        devStageField = c2 = newGbField("/dev_stage", "dev", TRUE, TRUE, 128);
        slAddTail(&c1->children, c2);
    
        c2 = newGbField("/cell_line", "cel", TRUE, FALSE, 128);
        slAddTail(&c1->children, c2);
    
        chromosomeField = c2 = newGbField("/chromosome", "chr", TRUE, FALSE, 16);
        slAddTail(&c1->children, c2);

        mapField = c2 = newGbField("/map", "map", TRUE, FALSE, 128);         
        slAddTail(&c1->children, c2);

    c1 = newGbField("CDS", "cds", FALSE, TRUE, 128);
    c1->endsAtSlash = TRUE;
    slAddTail(&c0->children, c1);
        c2 = newGbField("/gene", "gen", TRUE, FALSE, 128);
        slAddTail(&c1->children, c2);

        c2 = newGbField("/product", "pro", FALSE, FALSE, 128);
        slAddTail(&c1->children, c2);

        c2 = newGbField("/note", "cno", FALSE, TRUE, 128);
        slAddTail(&c1->children, c2);
gbStruct = gbs;
keywordsField->toLower = TRUE;
tissueField->toLower = TRUE;
sexField->toLower = TRUE;
devStageField->toLower = TRUE;
}

struct keyVal *commentKey;	/* The comment field needs lots of further processing
                                 * in unfinished genomic sequences, so we keep a
				 * pointer to it. */

void recurseFlatten(struct gbField *gbf)
/* Recursively flatten out gbf into keyVal array. */
{
struct keyVal *kv;
for (; gbf != NULL; gbf = gbf->next)
    {
    if (gbf->writeName && gbf->val)
        {
        kv = kvtAdd(kvt, gbf->writeName, gbf->val);
        if (gbf == commentField)
            commentKey = kv;
        }
    if (gbf->children)
        {
        recurseFlatten(gbf->children);
        }
    }
}

void flatten(struct gbField *gbf)
/* Flatten out gbf into keyVal array. */
{
commentKey = NULL;
kvtClear(kvt);
recurseFlatten(gbf);
}


char *skipLeadingNonSpaces(char *s)
/* Return first non-white space or NULL. */
{
char c;
/* Skip over non-white space */

for (;;)
    {
    c = *s;
    if (c == 0)
        return NULL;
    if (isspace(c))
        return s;
    ++s;
    }
}

char *skipToNextWord(char *s)
/* Returns start of next (white space separated) word.
 * Returns NULL if not another word. */
{
char c;
/* Skip over non-white space */

for (;;)
    {
    c = *s++;
    if (c == 0)
        return NULL;
    if (isspace(c))
        break;
    }
/* Skip over white space. */
for (;;)
    {
    c = *s;
    if (isspace(c))
        ++s;
    else if (c == 0)
        return NULL;
    else
        return s;
    }
}

char *skipThrough(char *s, char *through)
/* Skip to next character in s that's not part of through. */
{
char c, t;
char *th;
boolean anyMatch;
for (;;)
    {
    c = *s;
    if (c == 0)
        return NULL;
    anyMatch = FALSE;
    for (th = through; (t = *th) != 0; ++th)
        {
        if (c == t)
            {
            anyMatch = TRUE;
            break;
            }
        }
    if (!anyMatch)
        return s;
    ++s;
    }
}



char *skipTo(char *s, char *target)
/* Skip to next character in s that is part of target. */
{
char c, t;
char *ta;
for (;;)
    {
    c = *s;
    if (c == 0)
        return NULL;
    for (ta = target; (t = *ta) != 0; ++ta)
        {
        if (c == t)
            {
            return s;
            }
        }
    ++s;
    }
}


void readOneField(char *line, struct lineFile *lf, struct gbField *gbf, int subIndent)
/* Read in a single field. */
{
int valPos = 0;
int lineLen;
boolean inSlashSub = (gbf->readName[0] == '/');
int nextValPos;
int indentCount;
struct hash *valHash;
char *valBuf = gbf->valBuf;

if (inSlashSub)
    line += 2;  /* Skip over =" */
for (;;)
    {
    /* Erase trailing spaces and quotes. */
    lineLen = strlen(line);
    while (lineLen > 0)
        {
        char c = line[lineLen-1];
        if (!isspace(c))
            break;
        --lineLen;
        line[lineLen] = 0;
        }
    if (lineLen > 0 && inSlashSub)
        {
        if (line[lineLen-1] == '"')
            {
            --lineLen;
            line[lineLen] = 0;
            }
        }
    
    /* Figure out how much space we use now, and if necessary expand
     * buffer space. */
    nextValPos = valPos + lineLen + 1;
    if (nextValPos > gbf->valAlloc)
        {
        gbf->valAlloc = nextValPos*2;
        valBuf = gbf->valBuf = needMoreMem(valBuf, valPos, gbf->valAlloc);
        }
    
    /* Copy in the line. */
    memcpy(valBuf + valPos, line, lineLen);

    /* All done if single line field. */
    if (!gbf->multiLine)
        break;

    /* Get next line. */
    if (!lineFileNext(lf, &line, &lineLen))
        break;    
    
    /* Count up indentation and break if need to return to parent. */
    for (indentCount = 0; ; ++indentCount)
        {
        if (!isspace(*line))
            break;
        line += 1;
        indentCount += 1;
        }
    if (line[0] == '/')
        {
	if (gbf->endsAtSlash || inSlashSub)
	    {
	    lineFileReuse(lf);
	    break;
	    }
	}
    if (indentCount < subIndent)
        {
        lineFileReuse(lf);
        break;
        }
    
    valBuf[nextValPos-1] = ' ';
    valPos = nextValPos;
    }
/* Add zero tag to string and indicate that we've got a value. 
 * Return if value is just '.' */
valBuf[nextValPos-1] = 0;
if (valBuf[1] == 0 && valBuf[0] == '.')
	return;

gbf->val = valBuf;

/* If tracking all values put it on hash table and list. */
if (gbf->toLower)
	tolowers(gbf->val);
if ((valHash = gbf->valHash) != NULL)
    {
    struct gbFieldUseCounter *uses;
    struct hashEl *hel;

    if ((hel = hashLookup(valHash, gbf->val)) != NULL)
        {
        uses = hel->val;
        uses->useCount += 1;
        }
    else
        {
        AllocVar(uses);
        hel = hashAdd(valHash, gbf->val, uses);
        uses->val = hel->name;
        uses->useCount = 1;
        slAddHead(&gbf->valList, uses);
        }
    }
}

boolean recurseReadFields(struct lineFile *lf, struct gbField *first, int parentIndent)
/* Recursively read in RA. */
{
char *line;
int lineSize;
int indent;
char *firstWord;
char *s;
struct gbField *gbf;

while (lineFileNext(lf, &line, &lineSize))
    {
    for (indent = 0; ; ++indent)
        {
        if (line[indent] != ' ')
            break;
        }
    if (indent <= parentIndent)
        {
        lineFileReuse(lf);
        return TRUE;
        }
    firstWord = line+indent;
    for (gbf = first; gbf != NULL; gbf = gbf->next)
        {
        if (gbf->val == NULL && startsWith(gbf->readName, firstWord))
            {
            int subIndent;
            if (gbf->readName[0] == '/')
                {
                s = strchr(firstWord, '=');
                subIndent = firstWord-line;
                }
            else
                {
                s = skipToNextWord(firstWord);
		if (s == NULL)
		    s = firstWord + strlen(firstWord);
                subIndent = s - line;
                }
            if (s != NULL)
                {
                readOneField(s, lf, gbf, subIndent);
                }            
            if (gbf->children)
                recurseReadFields(lf, gbf->children, indent);        
            break;
            }
        }
    if (gbf == NULL && parentIndent < 0)
        {
        if (startsWith("ORIGIN", firstWord) || startsWith("//", firstWord))
            return TRUE;
        }
    }
return FALSE;
}

void gbfClearVals(struct gbField *gbf)
/* Set all values to zero. */
{
for (; gbf != NULL; gbf = gbf->next)
    {
    gbf->val = NULL;
    if (gbf->children)
        gbfClearVals(gbf->children);
    }
}

boolean readGbInfo(struct lineFile *lf)
/* Read in a single Gb record up to the ORIGIN. */
{
gbfClearVals(gbStruct);
return recurseReadFields(lf, gbStruct, -1);
}

void readSequence(struct lineFile *lf, DNA **retDna, int *retSize)
/* Read in sequence part of genBank file */
{
static int dnaAlloc = 0;
static DNA *dna = NULL;
int dnaCount = 0;

if (dnaAlloc == 0)
    {
    dnaAlloc = 512*1024;
    dna = needMem(dnaAlloc * sizeof(*dna));
    }

for (;;)
    {
    char *line;
    int lineSize;
    char c;
    
    /* Process lines until //. */
    if (!lineFileNext(lf, &line, &lineSize))
        break;
    if (line[0] == '/')
        {
        break;
        }
    while ((c = *line++) != 0)
        {
        if (!isspace(c) && !isdigit(c))
            {
            dna[dnaCount++] = c;
            }
        if (dnaCount >= dnaAlloc)
            {
            dnaAlloc *= 2;
            dna = needMoreMem(dna, dnaCount, dnaAlloc);
            }
        }
    }
dna[dnaCount] = 0;
*retDna = dna;
*retSize = dnaCount;
}

int sizeHistogram[5000];	/* Keep track of sequence sizes. */
int histChunkSize = 100;

struct authorExample
/* We keep one of these for each author, the better
 * to check each style of EST. */
    {
    struct authorExample *next;	/* Next in list. */
    char *name;			/* Name of author (or group of authors) */
    int count;			/* Number of ESTs they've submitted. */
    char accession[32];		/* Accession of one of their ESTs */
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


/* Thes three variables track the contigs in the current sequence. */
int contigCount;            /* Count of contigs in sequence. */
int contigStarts[1024*2];   /* Starts of contigs. */
int contigEnds[1024*2];     /* Ends of contigs. */

void addContig(int start, int end)
/* Add a new contig. */
{
if (contigCount >= ArraySize(contigStarts) )
    errAbort("Too many contigs, can only handle %d\n", (int)ArraySize(contigStarts));
contigStarts[contigCount] = start;
contigEnds[contigCount] = end;
++contigCount;
}


char *nextWordStart(char *s)
/* Return start of next (white space separated) word. */
{
char c;
/* Skip over current word. */
for (;;)
    {
    if ((c = *s++) == 0)
        return NULL;
    if (isspace(c))
        break;
    }
/* Skip over white space. */
for (;;)
    {
    if ((c = *s) == 0)
        return NULL;
    if (!isspace(c))
        return s;
    ++s;
    }
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
        errAbort("Too many contigs, can only handle %d\n", (int)ArraySize(contigStarts));
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
    errAbort("Can't find preserved marker in %s\n%s\n", accession, s);
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
        slen = sprintf(sbuf, "contig %d:", cix++);
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

void bacWrite(char *faDir, char *accession, int version, DNA *dna, int dnaSize)
/* Write all the contigs of one BAC to a file. */
{
char fileName[512];
FILE *f;
char id[48];
int i;
int start, end, size;

sprintf(fileName, "%s/%s.fa", faDir, accession);
f = mustOpen(fileName, "w");
if (contigCount <= 1)
    {
    faWriteNext(f, accession, dna, dnaSize);
    }
else
    {
    for (i=0; i<contigCount; ++i)
	{
	start = contigStarts[i];
	end = contigEnds[i];
	size = end - start;
	assert(size > 0);
	sprintf(id, "%s.%d", accession, i+1);
	faWriteNext(f, id, dna+start, size);
	}
    }
fclose(f);
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
boolean gotThreePrime, gotFivePrime;

gotThreePrime = isThreePrime(def);
gotFivePrime = isFivePrime(def);
if (gotThreePrime ^ gotFivePrime)
    dir = (gotThreePrime ? three : five);
if (dir == NULL)
    {
    gotThreePrime = isThreePrime(com);
    gotFivePrime = isFivePrime(com);
    dir = (gotThreePrime ? three : five);
    }
return dir;
}

static char * replaceMultipleChars(char *src, char *origList, char *newList)
/*
Utility function to replace all occurrences of the 
original chars with their respective new ones in a string.
Each character at index X in the origList gets replaced by the
corresponding one at the same index in newList.

param src - The string to operate on.
param origList - The set of chars in the string to replace.
param newList - The replacement set of char to place in the string.

return - A newly allocated string containing the 
         chars that have been morphed.
 */
{
char *retStr = NULL;
int repCount = strlen(origList);
int repIndex = 0;

assert(repCount == strlen(newList));
if (src == NULL) 
    {
    return NULL;
    }

retStr = cloneString(src);

for (repIndex = 0; repIndex < repCount; ++repIndex) 
    {
    subChar(retStr, origList[repIndex], newList[repIndex]);
    }
return retStr;
}

void procOneGbFile(char *inName, 
                   FILE *faFile, char *faDir, 
                   FILE *raFile, char *raName, 
                   struct hash *uniqueNameHash, struct hash *estAuthorHash,
                   struct hash *orgHash, 
                   struct filter *filter)
/* Process one genBank file into fa and ra files. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
char *words[16];
int wordCount;
static int gbCount = 0;
DNA *dna;
int dnaSize;
char sizeString[16];
char *oldOrg = NULL;
struct dyString *ctgDs = newDyString(512);
int modder = 100;  /* How often to print an I'm still alive dot. */
char *origFaDir = faDir;

if (formatType == ftBac)
    modder = 100;
gbCount = 0;
while (readGbInfo(lf))
    {
    char *locus = locusField->val;
    char *accession = accessionField->val;
    int version = 0;
    char *gi = NULL;
    char *verChar = versionField->val;
    char *s;
    char *org = organismField->val;
    struct keyVal *seqKey;
    boolean doneSequence = FALSE;

    if (++gbCount % modder == 0)
        {
        printf(".");
        if (gbCount%70000 == 0)
            printf("\n");
        fflush(stdout);
        }
    if (locus == NULL || accession == NULL)
        errAbort("No LOCUS or no ACCESSION line near %d of %s", lf->lineIx, inName);
    
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
    if (!hashLookup(uniqueNameHash, accession))
        {
        boolean isEst = FALSE;
        boolean isFragmented = FALSE;
        boolean isNoted = FALSE;
        int nSeparateFrags = 0;
        char frg[16];
	char verNum[8];
        char *com = commentField->val;
        char *center = NULL;
        char *journal = journalField->val;
        boolean hattoriStyle = FALSE;
        boolean beutenStyle = FALSE;

        hashAdd(uniqueNameHash, accession, NULL);
        flatten(gbStruct);
                
        /* Get additional keys. */
	if (com != NULL)
	    {
	    if (startsWith("REVIEWED", com))
	        kvtAdd(kvt, "cur", "yes");
	    }
	sprintf(verNum, "%d", version);
	kvtAdd(kvt, "ver", verNum);
	if (gi != NULL)
	    kvtAdd(kvt, "ngi", gi);
        wordCount = chopLine(locus, words);
        if (wordCount >= 6)
            {
            kvtAdd(kvt, "mol", words[3]);
            kvtAdd(kvt, "cat", words[wordCount-2]);
            kvtAdd(kvt, "dat", words[wordCount-1]);
            }
        else if (wordCount == 5 && sameString(words[2], "bp") && isdigit(words[1][0]))
            {
            /* Check carefully.  Probably it's just missing the molecule type... */
            char *date = words[4];
            if (!isNcbiDate(date))
                {
                errAbort("Strange LOCUS line in %s accession %s", inName, accession);
                }
            kvtAdd(kvt, "cat", words[3]);
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
            errAbort("Short LOCUS line in %s accession %s", inName, accession);
            }
        if ((wordCount >= 5 && sameString(words[4], "EST")) || 
	    (wordCount >= 6 && sameString(words[5], "EST")))
            {
            /* Try and figure out if it's a 3' or 5' EST */
	    char *dir = getEstDir(definitionField->val, com);
	    if (dir != NULL)
                kvtAdd(kvt, "dir", dir);
            isEst = TRUE;
            }

        contigCount = 0;
        if (formatType == ftBac && org != NULL && sameString(org, "Homo sapiens"))
            {
            char *keys = keywordsField->val;
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
                char *authors = authorsField->val;
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
            }
        
        /* Handle sequence part of read. */
        readSequence(lf, &dna, &dnaSize);
        doneSequence = TRUE;
        seqKey = kvtAdd(kvt, "seq", dna);
        sprintf(sizeString, "%d", dnaSize);
        kvtAdd(kvt, "siz", sizeString);
        
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
            sprintf(frg, "%d", contigCount);
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

        if ((filter->keyExp == NULL) || keyExpEval(filter->keyExp, kvt))
            {
            int chunkIx;
            /* Store size in histogram for statistical analysis. */
            chunkIx = dnaSize/histChunkSize;
            if (chunkIx >= ArraySize(sizeHistogram))
                chunkIx = ArraySize(sizeHistogram)-1;
            sizeHistogram[chunkIx] += 1;

            if (isEst)
                {
                char *author = authorsField->val;
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
            if (commentKey != NULL)
                commentKey->val = NULL;  /* Don't write out comment either. */

            if (gByOrganism) 
                {
                char *orgDir = NULL;
                char fullOrgPath[PATH_LEN];
                char raPath[PATH_LEN];
                char faPath[PATH_LEN];
                
                /* If we are on a new organism, open a new file,
                   otherwise just reuse our old file handles. */
                if (NULL == oldOrg || !sameString(oldOrg, org)) 
                    {
                    freez(&oldOrg);
                    oldOrg = cloneString(org);
                    strcpy(oldOrg, org);
                    fflush(raFile);
                    fflush(faFile);
                    carefulClose(&raFile);
                    carefulClose(&faFile);
                    
                    if (NULL == org)
                        {
                        orgDir = "Unspecified";
                        }
                    else
                        {
                        /* Replace illegal directory chars */
                        orgDir = replaceMultipleChars(org, " ()'/", "_###~");
                        }
            
                    if (!hashLookup(orgHash, org))
                        {
                        sprintf(fullOrgPath, "%s/%s", gOutputDir, orgDir);
                        makeDir(fullOrgPath);
                        hashAdd(orgHash, org, orgDir);
                        }

                    sprintf(raPath, "%s/%s/%s", gOutputDir, orgDir, raName); 
                    /*printf ("RA PATH: %s\n", raPath);*/
                    raFile = mustOpen(raPath, "ab");

                    sprintf(faPath, "%s/%s/%s", gOutputDir, orgDir, origFaDir); 
                    /*printf("FA PATH: %s\n", path);*/
                    faFile = mustOpen(faPath, "ab");

                    faDir = faPath;
                    freez(&orgDir);
                    }
                }

            kvtWriteAll(kvt, raFile, filter->hideKeys);
            if (formatType == ftBac)
                {
                printf("bacWrite()\n");
                bacWrite(faDir, accession, version, dna, dnaSize);
                }
            else
		{
                faWriteNext(faFile, (gFaInclVer ? versionField->val : accession) , dna, dnaSize);
		}
            }
        }
    if (!doneSequence)
        {
        /* Just skip to '//' */
        char *line;
	int lineSize;
	while (lineFileNext(lf, &line, &lineSize))
            if (startsWith("//", line))
                break;
        }
    } /* End of outermost while loop */
    

freez(&oldOrg);
freeDyString(&ctgDs);
lineFileClose(&lf);
printf(" %d\n", gbCount);
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

void printGbfStats(FILE *f, struct gbField *gbf)
/* Print statistics on recursive data structure. */
{
for (;gbf != NULL; gbf = gbf->next)
    {
    if (gbf->valHash != NULL)
	{
	struct gbFieldUseCounter *fc;
	slSort(&gbf->valList, cmpUse);
	slReverse(&gbf->valList);
	fc = gbf->valList;
	fprintf(f, "%s (%d different values)\n", gbf->writeName, slCount(fc));
	for (;fc != NULL; fc = fc->next)
		{
		fprintf(f, "  %d %s\n", fc->useCount, fc->val);
		}
	}
    if (gbf->children != NULL)
	 printGbfStats(f, gbf->children);
    }
}

void printStats(FILE *f, struct gbField *gbf)
/* Print stats  on one field - this is stats of all genBank records,
 * not just the ones that have passed the filter. */
{
int total = 0;
int longCount = 0;
int longIx = 5;
int longSize = longIx * histChunkSize;
int i;               

for (i=0; i<ArraySize(sizeHistogram); ++i)
    {
    int h = sizeHistogram[i];
    if (i >= longIx)
        longCount += h;
    total += h;
    }    
fprintf(f, "Total of %d sequences passed filter\n", total);
fprintf(f, "%d sequences longer than %d\n\n", longCount, longSize);

fprintf(f, "Size histogram\n");
for (i=0; i<ArraySize(sizeHistogram); ++i)
    {
    int h = sizeHistogram[i];
    if (h > 0)
        {
        fprintf(f, "%5d+ %5d\n", i*histChunkSize, h);
        }
    }
fprintf(f, "\n");

if (estAuthorList != NULL)
    {
    struct authorExample *ae;
    fprintf(f, "Samples of each EST author that passed filter\n");
    slReverse(&estAuthorList);
    slSort(&estAuthorList, cmpAuthorExample);
    for (ae = estAuthorList; ae != NULL; ae = ae->next)
        {
        fprintf(f, "%5d %s %s\n", ae->count, ae->accession, ae->name);
        }
    fprintf(f, "\n");
    }
printGbfStats(f, gbf);
}

void usage()
/* Explain usage and exit */
{
errAbort("gbToFaRa - Convert GenBank flat format file to an fa file containing\n"
         "the sequence data, an ra file containing other relevant info and\n"
         "a ta file containing summary statistics.\n"
         "usage:\n"
         "   gbToFaRa filterFile faFile raFile taFile genBankFile(s)\n"
         "where filterFile is definition of which records and fields\n"
         "use /dev/null if you want no filtering.\n"
	 "options:\n"
	 "    -byOrganism=outputDir - Make separate files for each organism\n"
	 "    -faInclVer - include version in fasta\n");
}

struct filter *makeFilter(char *fileName)
/* Create filter from filter specification file. */
{
struct filter *filt = (struct filter *)NULL;
FILE *f;
char line[1024];
int lineCount=0;
char *words[128];
int wordCount;
int i;

AllocVar(filt);
f = mustOpen(fileName, "r");
while (fgets(line, sizeof(line), f))
    {
    ++lineCount;
    if (startsWith("//", line) )
        continue;
    if (startsWith("restrict", line))
        {
        char *s = skipToNextWord(line);
        filt->keyExp = keyExpParse(s);
        }
    else if (startsWith("hide", line))
        {
        wordCount = chopLine(line, words);
        for (i=1; i<wordCount; ++i)
            {
            struct slName *name = newSlName(words[i]);
            slAddHead(&filt->hideKeys, name);
            }
        }
    else if (startsWith("type", line))
        {
        char *type;
        wordCount = chopLine(line, words);
        if (wordCount < 2)
            errAbort("Expecting at least two words in type line of %s\n", fileName);
        type = words[1];
        if (sameWord(type, "BAC"))
            {
            formatType = ftBac;
            }
        else
            errAbort("Unrecognized type %s in %s\n", type, fileName);
        }
    else if (skipLeadingSpaces(line) != NULL)
        {
        errAbort("Can't understand line %d of %s:\n%s", lineCount, fileName,  line);
        }
    }
fclose(f);
return filt;
}

int main(int argc, char *argv[])
/* Check parameters, set up, loop through each GenBank file. */
{
char *filterName;
struct filter *filter;
char *faName, *raName, *taName, *gbName;
FILE *fa = NULL;
FILE *ra = NULL;
FILE *ta = NULL;
int i = 0;
int startIndex = 5;
struct hash *uniqHash = NULL;
struct hash *estAuthorHash = NULL;
struct hash *orgHash = NULL;

optionInit(&argc, argv, optionSpecs);
if (argc < 6)
    usage();

gOutputDir = optionVal("byOrganism", NULL);
gByOrganism = (gOutputDir != NULL);
gFaInclVer = optionExists("faInclVer");

filterName = argv[1];
faName = argv[2];
raName = argv[3];
taName = argv[4];

filter  = makeFilter(filterName);
uniqHash = newHash(16);
estAuthorHash = newHash(10);
kvt = newKvt(128);
makeGbStruct();

orgHash = newHash(20);
ta = mustOpen(taName, "wb");

if (gByOrganism) 
    {
    printf("Processing output by organism into directory: %s\n", gOutputDir);
    makeDir(gOutputDir);
    }
else
    {
    ra = mustOpen(raName, "wb");
    if (formatType != ftBac)
        {
        fa = mustOpen(faName, "wb");
        }
    }

for (i = startIndex; i < argc; ++i)
    {
    gbName = argv[i];
    printf("Processing %s into %s and %s\n", gbName, faName, raName);
    procOneGbFile(gbName, fa, faName, ra, raName, 
                  uniqHash, estAuthorHash, orgHash, filter);
    }

printStats(ta, gbStruct);
return 0;
}
