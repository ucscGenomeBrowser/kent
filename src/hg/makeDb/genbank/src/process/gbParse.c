/* Parse a gbRecord.  Module serves as a global singleton with state of last
 * record read.  Also has some parsing utility function.  */
#include "common.h"
#include "dystring.h"
#include "dnaseq.h"
#include "hash.h"
#include "keys.h"
#include "gbParse.h"
#include "gbFileOps.h"
#include "linefile.h"

static char const rcsid[] = "$Id: gbParse.c,v 1.4 2003/07/14 19:53:25 markd Exp $";


/* Some fields we'll want to use directly. */
struct gbField *gbStruct;               /* Root of field tree. */
struct gbField *gbLocusField;     
struct gbField *gbDefinitionField;
struct gbField *gbAccessionField;
struct gbField *gbVersionField;
struct gbField *gbCommentField;
struct gbField *gbAuthorsField;
struct gbField *gbJournalField;
struct gbField *gbKeywordsField;
struct gbField *gbOrganismField;
struct gbField *gbTissueField;
struct gbField *gbSexField;
struct gbField *gbDevStageField;
struct gbField *gbCloneField;
struct gbField *gbChromosomeField;
struct gbField *gbMapField;
struct gbField *gbPrtField;
struct gbField *gbGeneDbxField;
struct gbField *gbCdsDbxField;
struct gbField *gbProteinIdField;
struct gbField *gbTranslationField;

/* State flag, indicates end-of-record reached on current entry; needed to
 * detect entries that don't have sequences */
static boolean gReachedEOR = FALSE;

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

static struct gbField *newField(char *readName, char *writeName, 
    unsigned flags, int initValSize)
/* Make a new gbField struct. */
{
struct gbField *gbf;
AllocVar(gbf);
gbf->readName = readName;
gbf->writeName = writeName;
gbf->val = dyStringNew(initValSize);
if (flags & GBF_TRACK_VALS)
    gbf->valHash = newHash(0);
gbf->flags = flags;
return gbf;
}

void gbfInit()
/* Create a heirarchical structure for parsing genBank fields. */
{
struct gbField *gbs = NULL;
struct gbField *c0, *c1, *c2;

gbLocusField = c0 = newField("LOCUS", NULL, GBF_NONE, 16);
slAddTail(&gbs, c0);

gbAccessionField = c0 = newField("ACCESSION", "acc", GBF_NONE, 32);
slAddTail(&gbs, c0);

gbVersionField = c0 = newField("VERSION", NULL, GBF_NONE, 32);
slAddTail(&gbs, c0);

gbDefinitionField = c0 = newField("DEFINITION", "def", GBF_MULTI_LINE, 1024);
slAddTail(&gbs, c0);

gbKeywordsField = c0 = newField("KEYWORDS", "key", GBF_TRACK_VALS, 1024);
slAddTail(&gbs, c0);

c0 = newField("SOURCE", "src", GBF_TRACK_VALS, 128);
slAddTail(&gbs, c0);
gbOrganismField = c1 = newField("ORGANISM", "org", GBF_TRACK_VALS, 64);
slAddTail(&c0->children, c1);

c0 = newField("REFERENCE", NULL, GBF_NONE, 64);
slAddTail(&gbs, c0);
gbAuthorsField = c1 = newField("AUTHORS", "aut", GBF_TRACK_VALS|GBF_MULTI_LINE, 4*1024);
slAddTail(&c0->children, c1);

gbJournalField = c1 = newField("JOURNAL", NULL, GBF_MULTI_LINE, 256);
slAddTail(&c0->children, c1);

gbCommentField = c0 = newField("COMMENT", "com", GBF_MULTI_LINE, 8*1024);
slAddTail(&gbs, c0);

/* FEATURES */
c0 = newField("FEATURES", NULL, GBF_NONE, 128);
slAddTail(&gbs, c0);

/* FEATURES source */
c1 = newField("source", NULL, GBF_NONE, 128);
slAddTail(&c0->children, c1);
c2 = newField("/clone_lib", "lib", GBF_TRACK_VALS, 128);
slAddTail(&c1->children, c2);

gbCloneField = c2 = newField("/clone", "clo", GBF_NONE, 128);
slAddTail(&c1->children, c2);
        
gbSexField = c2 = newField("/sex", "sex", GBF_TRACK_VALS, 128);         
slAddTail(&c1->children, c2);

gbTissueField = c2 = newField("/tissue_type", "tis", GBF_TRACK_VALS|GBF_MULTI_LINE, 128);
slAddTail(&c1->children, c2);

gbDevStageField = c2 = newField("/dev_stage", "dev", GBF_TRACK_VALS|GBF_MULTI_LINE, 128);
slAddTail(&c1->children, c2);
    
c2 = newField("/cell_line", "cel", GBF_TRACK_VALS, 128);
slAddTail(&c1->children, c2);
    
gbChromosomeField = c2 = newField("/chromosome", "chr", GBF_TRACK_VALS, 16);
slAddTail(&c1->children, c2);

gbMapField = c2 = newField("/map", "map", GBF_TRACK_VALS, 128);         
slAddTail(&c1->children, c2);

/* FEATURES gene */
c1 = newField("gene", NULL, GBF_NONE, 128);
slAddTail(&c0->children, c1);
c2 = newField("/db_xref", NULL, GBF_MULTI_VAL, 128); 
slAddTail(&c1->children, c2);
gbGeneDbxField = c2;

/* FEATURES CDS */
c1 = newField("CDS", "cds", GBF_MULTI_LINE, 128);
slAddTail(&c0->children, c1);
c2 = newField("/gene", "gen", GBF_TRACK_VALS, 128);
slAddTail(&c1->children, c2);

c2 = newField("/product", "pro", GBF_NONE, 128);
slAddTail(&c1->children, c2);

gbProteinIdField = newField("/protein_id", "prt", GBF_NONE, 32);
slAddTail(&c1->children, gbProteinIdField);

gbTranslationField = newField("/translation", NULL, GBF_MULTI_LINE|GBF_CONCAT_VAL, 128);
slAddTail(&c1->children, gbTranslationField);

c2 = newField("/note", "cno", GBF_MULTI_LINE, 128);
slAddTail(&c1->children, c2);
gbStruct = gbs;
gbKeywordsField->flags |= GBF_TO_LOWER;
gbTissueField->flags |= GBF_TO_LOWER;
gbSexField->flags |= GBF_TO_LOWER;
gbDevStageField->flags |= GBF_TO_LOWER;

c2 = newField("/protein_id", "prt", GBF_NONE, 128); 
slAddTail(&c1->children, c2);
gbPrtField = c2;

c2 = newField("/db_xref", NULL, GBF_MULTI_VAL, 128); 
slAddTail(&c1->children, c2);
gbCdsDbxField = c2;
}

void gbfClearVals(struct gbField *gbf)
/* Set all values to zero. */
{
for (; gbf != NULL; gbf = gbf->next)
    {
    dyStringClear(gbf->val);
    if (gbf->children != NULL)
        gbfClearVals(gbf->children);
    }
}

static void readOneField(char *line, struct lineFile *lf, struct gbField *gbf, int subIndent)
/* Read in a single field. */
{
int lineLen;
boolean inSlashSub = (gbf->readName[0] == '/');
int indentCount;
struct hash *valHash;

/* special handling for field that occurs multiple times */
if (gbf->val->stringSize > 0)
    {
    if (!(gbf->flags & GBF_MULTI_VAL))
        return; /* not supported, skip */

    /* space-separate values */
    dyStringAppendC(gbf->val, ' ');
    }

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
    
    /* Copy in the line. */
    dyStringAppendN(gbf->val, line, lineLen);

    /* All done if single line field. */
    if (!(gbf->flags & GBF_MULTI_LINE))
        break;

    /* Get next line. */
    if (!lineFileNext(lf, &line, &lineLen))
        break;    
    
    /* Count up indentation and break if need to return to parent. */
    for (indentCount = 0; isspace(*line); ++indentCount)
        {
        line += 1;
        }
    if (inSlashSub && line[0] == '/')
        {
        lineFileReuse(lf);
        break;
        }
    if ((indentCount < subIndent)
        || (!inSlashSub && (*line == '/') && (indentCount == subIndent)))
        {
        lineFileReuse(lf);
        break;
        }
    if ((gbf->flags & GBF_CONCAT_VAL) == 0)
        dyStringAppendC(gbf->val, ' ');
    }

/* Return if value is just '.' */
if (sameString(gbf->val->string, "."))
    return;

/* If tracking all values put it on hash table and list. */
if (gbf->flags & GBF_TO_LOWER)
    tolowers(gbf->val->string);
if ((valHash = gbf->valHash) != NULL)
    {
    struct gbFieldUseCounter *uses;
    struct hashEl *hel;

    if ((hel = hashLookup(valHash, gbf->val->string)) != NULL)
        {
        uses = hel->val;
        uses->useCount += 1;
        }
    else
        {
        AllocVar(uses);
        hel = hashAdd(valHash, gbf->val->string, uses);
        uses->val = hel->name;
        uses->useCount = 1;
        slAddHead(&gbf->valList, uses);
        }
    }
}

static boolean recurseReadFields(struct lineFile *lf, struct gbField *first, int parentIndent)
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
        if (startsWith(gbf->readName, firstWord))
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
                readOneField(s, lf, gbf, subIndent);
            if (gbf->children)
                recurseReadFields(lf, gbf->children, indent);        
            break;
            }
        }
    if (gbf == NULL && parentIndent < 0)
        {
        if (startsWith("ORIGIN", firstWord))
            return TRUE;
        if (startsWith("//", firstWord))
            {
            gReachedEOR = TRUE;
            return TRUE;
            }
        }
    }
return FALSE;
}

boolean gbfReadFields(struct lineFile *lf)
/* Read in a single Gb record up to the ORIGIN. */
{
gbfClearVals(gbStruct);
gReachedEOR = FALSE;
return recurseReadFields(lf, gbStruct, -1);
}

DNA* gbfReadSequence(struct lineFile *lf, int *retSize)
/* Read in sequence part of genBank file */
{
static int dnaAlloc = 0;
static DNA *dna = NULL;
int dnaCount = 0;

*retSize = 0;
if (gReachedEOR)
    {
    /* some gbcon.seq don't have origin, they reference contigs, just skip */
    warn("no mRNA sequence for %s in %s", gbAccessionField->val->string,
         lf->fileName);
    return NULL;  /* no sequence */
    }

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
*retSize = dnaCount;

/* FIXME: make abort */
if (numAllowedRNABases(dna) > 0)
    warn("invalid mRNA bases for %s in %s", gbAccessionField->val->string,
         lf->fileName);
return dna;
}

void gbfSkipSequence(struct lineFile *lf)
/* Skip to '//' if sequence not read */
{
char *line;
int lineSize;
while (lineFileNext(lf, &line, &lineSize))
    if (startsWith("//", line))
        break;
gReachedEOR = TRUE;
}

static void recurseFlatten(struct gbField *gbf, struct kvt *kvt);
/* forward */

static void flattenField(struct gbField *gbf, struct kvt *kvt)
/* Recursively flatten a filed into keyVal array. */
{
if ((gbf->writeName != NULL) && (gbf->val->stringSize > 0))
    kvtAdd(kvt, gbf->writeName, gbf->val->string);
if (gbf->children != NULL)
    recurseFlatten(gbf->children, kvt);
}

static void recurseFlatten(struct gbField *gbf, struct kvt *kvt)
/* Recursively flatten out gbf into keyVal array. */
{
for (; gbf != NULL; gbf = gbf->next)
    flattenField(gbf, kvt);
}

void gbfFlatten(struct gbField *gbf, struct kvt *kvt)
/* Flatten out gbf into keyVal array. */
{
kvtClear(kvt);
recurseFlatten(gbf, kvt);
}



/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

