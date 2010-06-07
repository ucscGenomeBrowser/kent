/* Parse a gbRecord.  Module serves as a global singleton with state of last
 * record read.  Also has some parsing utility function.  */
#include "common.h"
#include "dystring.h"
#include "dnaseq.h"
#include "hash.h"
#include "keys.h"
#include "gbDefs.h"
#include "gbParse.h"
#include "gbFileOps.h"
#include "linefile.h"

static char const rcsid[] = "$Id: gbParse.c,v 1.21 2008/10/14 17:35:16 markd Exp $";


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
struct gbField *gbSourceOrganism;
struct gbField *gbPrtField;
struct gbField *gbGeneDbxField;
struct gbField *gbGeneGeneField;
struct gbField *gbCdsDbxField;
struct gbField *gbCdsGeneField;
struct gbField *gbProteinIdField;
struct gbField *gbTranslationField;
struct gbField *gbCloneLibField;
struct gbField *gbMiscDiffField;
boolean isInvitrogenEvilEntry;
boolean isAthersysRageEntry;
boolean isOrestesEntry;

/* RefSeq specific data */
struct gbField *gbRefSeqRoot = NULL;
struct gbField *gbRefSeqStatusField = NULL;
struct gbField *gbRefSeqSummaryField = NULL;
struct gbField *gbRefSeqCompletenessField = NULL;
struct gbField *gbRefSeqDerivedField = NULL;


/* list of misc diffs in current record */
struct gbMiscDiff *gbMiscDiffVals = NULL;;

/* State flag, indicates end-of-record reached on current entry; needed to
 * detect entries that don't have sequences */
static boolean gReachedEOR = FALSE;


static char *getCurAcc()
/* get current accession field, or "unknown" if no defined yet */
{
if (gbAccessionField->val->stringSize == 0)
    return "unknown accession";
else
    return gbAccessionField->val->string;
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
    unsigned flags, int indent, int initValSize)
/* Make a new gbField struct. */
{
struct gbField *gbf;
AllocVar(gbf);
gbf->readName = readName;
gbf->writeName = writeName;
gbf->val = dyStringNew(initValSize);
if (flags & GBF_TRACK_VALS)
    gbf->valHash = newHash(0);
gbf->indent = indent;
gbf->flags = flags;
return gbf;
}

void gbfInit()
/* Create a heirarchical structure for parsing genBank fields. */
{
struct gbField *gbs = NULL;
struct gbField *c0, *c1, *c2;

gbLocusField = c0 = newField("LOCUS", NULL, GBF_NONE, 0, 16);
slAddTail(&gbs, c0);

gbAccessionField = c0 = newField("ACCESSION", "acc", GBF_NONE, 0, 32);
slAddTail(&gbs, c0);

gbVersionField = c0 = newField("VERSION", NULL, GBF_NONE, 0, 32);
slAddTail(&gbs, c0);

gbDefinitionField = c0 = newField("DEFINITION", "def", GBF_MULTI_LINE, 0, 1024);
slAddTail(&gbs, c0);

gbKeywordsField = c0 = newField("KEYWORDS", "key", GBF_TRACK_VALS|GBF_MULTI_LINE, 0, 1024);
slAddTail(&gbs, c0);

c0 = newField("SOURCE", "src", GBF_TRACK_VALS, 0, 128);
slAddTail(&gbs, c0);
gbOrganismField = c1 = newField("ORGANISM", "org", GBF_TRACK_VALS, 2, 64);
slAddTail(&c0->children, c1);

c0 = newField("REFERENCE", NULL, GBF_NONE, 0, 64);
slAddTail(&gbs, c0);
gbAuthorsField = c1 = newField("AUTHORS", "aut", GBF_TRACK_VALS|GBF_MULTI_LINE, 2, 4*1024);
slAddTail(&c0->children, c1);

gbJournalField = c1 = newField("JOURNAL", NULL, GBF_MULTI_LINE, 2, 256);
slAddTail(&c0->children, c1);

gbCommentField = c0 = newField("COMMENT", "com", GBF_MULTI_LINE, 0, 8*1024);
slAddTail(&gbs, c0);

/* FEATURES */
c0 = newField("FEATURES", NULL, GBF_NONE, 0, 128);
slAddTail(&gbs, c0);

/* FEATURES source */
c1 = newField("source", NULL, GBF_NONE, 5, 128);
slAddTail(&c0->children, c1);
gbCloneLibField = c2 = newField("/clone_lib", "lib", GBF_TRACK_VALS|GBF_MULTI_LINE, 21, 128);
slAddTail(&c1->children, c2);

gbCloneField = c2 = newField("/clone", "clo", GBF_NONE, 21, 128);
slAddTail(&c1->children, c2);
        
gbSexField = c2 = newField("/sex", "sex", GBF_TRACK_VALS, 21, 128);         
slAddTail(&c1->children, c2);

gbTissueField = c2 = newField("/tissue_type", "tis", GBF_TRACK_VALS|GBF_MULTI_LINE, 21, 128);
slAddTail(&c1->children, c2);

gbDevStageField = c2 = newField("/dev_stage", "dev", GBF_TRACK_VALS|GBF_MULTI_LINE, 21, 128);
slAddTail(&c1->children, c2);
    
c2 = newField("/cell_line", "cel", GBF_TRACK_VALS|GBF_MULTI_LINE, 21, 128);
slAddTail(&c1->children, c2);
    
gbChromosomeField = c2 = newField("/chromosome", "chr", GBF_TRACK_VALS, 21, 16);
slAddTail(&c1->children, c2);

gbMapField = c2 = newField("/map", "map", GBF_TRACK_VALS, 21, 128);
slAddTail(&c1->children, c2);

gbSourceOrganism = c2 = newField("/organism", NULL, GBF_MULTI_VAL|GBF_MULTI_SEMI, 21, 128);
slAddTail(&c1->children, c2);

/* FEATURES gene */
c1 = newField("gene", NULL, GBF_NONE, 5, 128);
slAddTail(&c0->children, c1);
c2 = newField("/gene", NULL, GBF_NONE, 21, 128);
slAddTail(&c1->children, c2);
gbGeneGeneField = c2;
c2 = newField("/db_xref", NULL, GBF_MULTI_VAL, 21, 128); 
slAddTail(&c1->children, c2);
gbGeneDbxField = c2;

/* FEATURES CDS */
c1 = newField("CDS", "cds", GBF_MULTI_LINE, 5, 128);
slAddTail(&c0->children, c1);
c2 = newField("/gene", NULL, GBF_NONE, 21, 128);
slAddTail(&c1->children, c2);
gbCdsGeneField = c2;
c2 = newField("/locus_tag", "lot", GBF_TRACK_VALS, 21, 128);
slAddTail(&c1->children, c2);

c2 = newField("/product", "pro", GBF_NONE, 21, 128);
slAddTail(&c1->children, c2);

gbProteinIdField = newField("/protein_id", "prt", GBF_NONE, 21, 32);
slAddTail(&c1->children, gbProteinIdField);

gbTranslationField = newField("/translation", NULL, GBF_MULTI_LINE|GBF_CONCAT_VAL, 21, 128);
slAddTail(&c1->children, gbTranslationField);

c2 = newField("/note", "cno", GBF_MULTI_LINE, 21, 128);
slAddTail(&c1->children, c2);
gbStruct = gbs;
gbKeywordsField->flags |= GBF_TO_LOWER;
gbTissueField->flags |= GBF_TO_LOWER;
gbSexField->flags |= GBF_TO_LOWER;
gbDevStageField->flags |= GBF_TO_LOWER;

c2 = newField("/protein_id", "prt", GBF_NONE, 21, 128); 
slAddTail(&c1->children, c2);
gbPrtField = c2;

c2 = newField("/db_xref", NULL, GBF_MULTI_VAL, 21, 128); 
slAddTail(&c1->children, c2);
gbCdsDbxField = c2;

c2 = newField("/transl_except", "translExcept", GBF_MULTI_VAL, 21, 128); 
slAddTail(&c1->children, c2);

c2 = newField("/exception", "exception", GBF_MULTI_VAL|GBF_SUB_SPACE, 21, 128); 
slAddTail(&c1->children, c2);

c2 = newField("/selenocysteine", "selenocysteine", GBF_BOOLEAN, 21, 32); 
slAddTail(&c1->children, c2);

/* FEATURES misc_difference */
gbMiscDiffField = c1 = newField("misc_difference", NULL, GBF_MULTI_LINE, 5, 128);
slAddTail(&c0->children, c1);
c2 = newField("/gene", NULL, GBF_NONE, 21, 128);
slAddTail(&c1->children, c2);
c2 = newField("/note", NULL, GBF_MULTI_LINE, 21, 128);
slAddTail(&c1->children, c2);
c2 = newField("/replace", NULL, GBF_NONE, 21, 128);
slAddTail(&c1->children, c2);

/* for refseq, we parse data stuff into comment. */
gbRefSeqStatusField = newField("refSeqStatus", "rss", GBF_NONE, -1, 128);
gbRefSeqRoot = gbRefSeqStatusField;
gbRefSeqSummaryField = newField("refSeqSummary", "rsu", GBF_NONE, -1, 1024);
slAddTail(&gbRefSeqRoot, gbRefSeqSummaryField);
gbRefSeqCompletenessField = newField("refSeqCompleteness", "rsc", GBF_NONE, -1, 128);
slAddTail(&gbRefSeqRoot, gbRefSeqCompletenessField);
gbRefSeqDerivedField = newField("refSeqDerived", "rsd", GBF_NONE, -1, 256);
slAddTail(&gbRefSeqRoot, gbRefSeqDerivedField);
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

static struct gbMiscDiff *gbMiscDiffNew()
/* allocate a new gbMiscDiff object */
{
struct gbMiscDiff *gmd;
AllocVar(gmd);
return gmd;
}

static void gbMiscDiffFree(struct gbMiscDiff **gmdPtr)
/* free a gbMiscDiff struct */
{
struct gbMiscDiff *gmd = *gmdPtr;
if (gmd != NULL)
    {
    freeMem(gmd->loc);
    freeMem(gmd->note);
    freeMem(gmd->gene);
    freeMem(gmd->replace);
    freeMem(gmd);
    }
}

static void gbMiscDiffFreeList(struct gbMiscDiff **gmdList)
/* free a list of gbMiscDiff struct */
{
struct gbMiscDiff *gmd;
while ((gmd = slPopHead(gmdList)) != NULL)
    gbMiscDiffFree(&gmd);
}

static void processMiscDiff()
/* process a misc diff field that was just parsed, moving values to list of
 * gbMiscDiffVals */
{
struct gbMiscDiff *gmd = gbMiscDiffNew();
gmd->loc = cloneString(gbMiscDiffField->val->string);
struct gbField *gbf;
for (gbf = gbMiscDiffField->children; gbf != NULL; gbf = gbf->next)
    {
    if (gbf->val->stringSize > 0)
        {
        if (sameString(gbf->readName,"/gene"))
            gmd->gene = cloneString(gbf->val->string);
        else if (sameString(gbf->readName,"/note"))
            gmd->note = cloneString(gbf->val->string);
        else if (sameString(gbf->readName,"/replace"))
            gmd->replace = cloneString(gbf->val->string);
        }
    }
slAddHead(&gbMiscDiffVals, gmd);
gbfClearVals(gbMiscDiffField);
}

static boolean checkForInvitrogenEvil(char *line)
/* check if clone is from the invitrogen `fulllength' libraries, some of which
 * have apparently been aligned to pseudogenes and then modified to match the
 * genome.  These have wasted a lot of time, so we toss all of them.  This is determined
 * by the URL for the (now dead) library in the entry.  Since it can occur in either a
 * COMMENT or a REMARK, remarks are not parsed, we just check each line.
 */
{
return (containsStringNoCase(line, "http://fulllength.invitrogen.com/") != NULL);
}

static boolean checkForOrestes(char *cloneLib)
/* check for ORESTES library */
{
char *orestes = stringIn("ORESTES", cloneLib);
if (orestes == NULL)
    return FALSE; // fast path
else
    {
    // make sure ORESTES is a word,  BgORESTES is something different
    // start and end of string, or non-alphanum are word seps
    char *end = orestes+7;
    return ((orestes == cloneLib) || !isalnum(*(orestes-1)))
        && ((*end == '\0') || !isalnum(*end));
    }
}

static boolean checkForAthersysRage(char *cloneLib)
/* check for Athersys RAGE library */
{
//  /clone_lib="Athersys RAGE Library"
return sameString(cloneLib, "Athersys RAGE Library");
}

static void readOneField(char *line, struct lineFile *lf, struct gbField *gbf, int subIndent)
/* Read in a single field. */
{
int lineLen;
boolean inSlashSub = (gbf->readName[0] == '/');
int indentCount;
struct hash *valHash;
char closeQuote = '"';  /* can be "..." or (...) */

/* special handling for field that occurs multiple times */
if (gbf->val->stringSize > 0)
    {
    if (!(gbf->flags & GBF_MULTI_VAL))
        return; /* not supported, skip */

    /* space-separate or semi-colon separate values */
    dyStringAppendC(gbf->val, ((gbf->flags & GBF_MULTI_SEMI) ? ';' : ' '));
    }

if (inSlashSub)
    {
    line++; /* skip = */
    /* skip quote or paren */
    if (*line == '"')
        {
        closeQuote = '"';
        line++;
        }
    else if (*line == '(')
        {
        closeQuote = ')';
        line++;
        }
    }
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
        if (line[lineLen-1] == closeQuote)
            {
            --lineLen;
            line[lineLen] = 0;
            }
        }

    /* change spaces to _ if requested */
    if (gbf->flags & GBF_SUB_SPACE)
        {
        subChar(line, ' ', '_');
        subChar(line, '\t', '_');
        }
    
    /* Copy in the line */
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
    if (checkForInvitrogenEvil(line))
        isInvitrogenEvilEntry = TRUE;
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
    if (checkForInvitrogenEvil(line))
        isInvitrogenEvilEntry = TRUE;
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
        if (startsWith(gbf->readName, firstWord) && (gbf->indent == indent))
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
            if (s == NULL)
                {
                if (gbf->flags & GBF_BOOLEAN)
                    dyStringAppend(gbf->val, "yes");
                }
            else
                readOneField(s, lf, gbf, subIndent);
            if (gbf->children)
                recurseReadFields(lf, gbf->children, indent);
            if (gbf == gbMiscDiffField)
                processMiscDiff();
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

static char* findNextInList(char* start, char** list)
/* find the first occurance of one of the strings in the list. */
{
char* first = NULL;
int i;

for (i = 0; list[i] != NULL; i++)
    {
    char* next = strstr(start, list[i]);
    if ((next != NULL) && ((first == NULL) || (next < first)))
        first = next;
    }
return first;
}

static char* nextRefSeqCommentField(char** startPtr, char** valuePtr)
/* Parse next field value out of comment.  Return null terminated
 * field name with null terminated value.  Advance start for next
 * call.  Return null if no more. */
{
static char* FIELD_NAMES[] = {
    "Summary:", "COMPLETENESS:", "Transcript Variant:", NULL
};
char *value, *next;
char* name;
if (*startPtr == NULL)
    return NULL;

name = findNextInList(*startPtr, FIELD_NAMES);

if (name == NULL)
    {
    /* no more, advance startPtr to end of string */
    *startPtr += strlen(*startPtr);
    return NULL;
    }
/* find : at end on name */
value = strchr(name, ':');
*value = '\0';
value++;

/* find either next field or end of string */
next = findNextInList(value, FIELD_NAMES);
if (next == NULL)
    *startPtr += strlen(value);  /* set to end of string */
else
    {
    next[-1] = '\0';
    *startPtr = next;
    }
*valuePtr = trimSpaces(value);
return name;
}

static void parseRefSeqStatus()
/* Parse refseq status out of comment field */
{
char *stat = NULL;
if (startsWith("REVIEWED REFSEQ:", gbCommentField->val->string))
    stat = "rev";
else if (startsWith("VALIDATED REFSEQ:", gbCommentField->val->string))
    stat = "val";
else if (startsWith("PROVISIONAL REFSEQ:", gbCommentField->val->string))
    stat = "pro";
else if (startsWith("PREDICTED REFSEQ:", gbCommentField->val->string))
    stat = "pre";
else if (startsWith("INFERRED REFSEQ:", gbCommentField->val->string))
    stat = "inf";
else if (startsWith("MODEL REFSEQ:", gbCommentField->val->string))
    stat = "mod";
if (stat == NULL)
    {
    stat = "unk";
    warn("Warning: refseq %s has unknown status in \"%s\"", 
         getCurAcc(), gbCommentField->val->string);
    }
dyStringAppend(gbRefSeqStatusField->val, stat);
}

static void parseRefSeqDerivedAcc(char *next, char *prefix)
/* Parse list sequences a refSeq was derived from when specified as
 * accessions.  Prefix is used by genomic, null otherwise */
{
char *word;
int numDerived = 0;

/* parse out referenced accessions */
while ((word = nextWord(&next)) != NULL)
    {
    int wlen = strlen(word);
    char lchr = word[wlen-1];
    
    if (!sameString(word, "and"))
        {
        if (!isdigit(lchr))
            word[wlen-1] = '\0';
        if (numDerived > 0)
            dyStringAppend(gbRefSeqDerivedField->val, " ");
        if (prefix != NULL)
            {
            dyStringAppend(gbRefSeqDerivedField->val, prefix);
            dyStringAppend(gbRefSeqDerivedField->val, "/");
            }
        dyStringAppend(gbRefSeqDerivedField->val, word);
        }
    if (lchr == '.')
        break; /* end of sentence */
    numDerived++;
    }
if (word == NULL)
    errAbort("%s: failed to find '.' terminating \"derived from5B\" comment",
             getCurAcc());
}

static void parseRefSeqDerivedGenomic(char *next)
/* Parse list sequences a refSeq was derived from annotated genomic */
{
static char *derivedStr = "derived from";
char genomicAcc[64];
char *start = NULL, *end = NULL;

/* parse: (NC_003071). */
if (next[0] == '(')
    {
    start = next+1;
    end = strstr(next, ").");
    }
if (end == NULL)
    {
    warn("Warning: %s: can't parse derived from genomic acc in RefSeq comment", getCurAcc());
    return;
    }
safef(genomicAcc, sizeof(genomicAcc), "%1.*s", (int)(end-start), start);

/* now parse associated accessions */
next = strstr(next, derivedStr);
if (next == NULL)
    {
    /* no mrna accs */
    dyStringAppend(gbRefSeqDerivedField->val, genomicAcc);
    }
else
    {
    next += strlen(derivedStr);
    next = skipLeadingSpaces(next);
    if (next == NULL)
        {
        warn("Warning: %s: can't find \"%s\" for annotatied genomic in RefSeq comment", getCurAcc(), derivedStr);
        return;
        }
    parseRefSeqDerivedAcc(next, genomicAcc);
    }
}

static void parseRefSeqDerived()
/* Parse list sequences a refSeq was derived from.
 *
 * Comment lines are like:
 * - NCBI review. The reference sequence was derived from D86960.1.
 *
 * - reference sequence was derived from AY146652.1, BC015368.2 and
 *   AW204088.1.
 *
 * - reference sequence was derived from BC012479.1 and D31968.1.
 *
 * - NCBI review. This record is derived from an annotated genomic
 *   sequence (NC_003071). The reference sequence was derived from
 *   mrna.At2g16920.1.
 *
 * - NCBI review. This record is derived from an annotated genomic
 *   sequence (NC_005782).
 */

{
static char *derivedStr = "derived from";
static char *derivedGenomicStr = "derived from an annotated genomic sequence";
static struct dyString *comBuf = NULL;
char *next;
boolean isGenomic = FALSE;

if (comBuf == NULL)
    comBuf = dyStringNew(1024);
dyStringClear(comBuf);
dyStringAppend(comBuf, gbCommentField->val->string);

/* scan up past the two types of derived from strings */
next = strstr(comBuf->string, derivedGenomicStr);
if (next != NULL)
    isGenomic = TRUE;
else
    next = strstr(comBuf->string, derivedStr);
if (next == NULL)
    {
    warn("Warning: %s: can't find \"%s\" in RefSeq comment", getCurAcc(), derivedStr);
    return;
    }
next += isGenomic ? strlen(derivedGenomicStr) : strlen(derivedStr);
next = skipLeadingSpaces(next);

if (isGenomic)
    parseRefSeqDerivedGenomic(next);
else
    parseRefSeqDerivedAcc(next, NULL);
}

static void parseRefSeqCompleteness(char* completeness)
/* Parse the refseq completeness value. The following values were observed
 * in the refseq data files:
 *     complete on the 5' end.
 *     complete on the 3' end.
 *     full length.
 *     incomplete on both ends.
 *     incomplete on the 5' end.
 *     incomplete on the 3' end.
 *     not full length.
 */
{
char* cmpl;
/* strstr is used to allow for stray spaces, etc */
if (strstr(completeness, "complete on the 5' end") != NULL)
    cmpl = "cmpl5";
else if (strstr(completeness, "complete on the 3' end") != NULL)
    cmpl = "cmpl3";
else if (strstr(completeness, "full length") != NULL)
    cmpl = "full";
else if (strstr(completeness, "incomplete on both ends") != NULL)
    cmpl = "incmpl";
else if (strstr(completeness, "incomplete on the 5' end") != NULL)
    cmpl = "incmpl5";
else if (strstr(completeness, "incomplete on the 3' end") != NULL)
    cmpl = "incmpl3";
else if (strstr(completeness, "not full length") != NULL)
    cmpl = "part";
else
    cmpl = "unk";
dyStringAppend(gbRefSeqCompletenessField->val, cmpl);
}

static void refSeqParse()
/* do special parsing of RefSeq data that was stuck in a comment */
{
char *next, *name, *value;
if (startsWith("NM_", getCurAcc()) || startsWith("NR_", getCurAcc()))
    {
    parseRefSeqStatus();
    parseRefSeqDerived();
    }
    
/* start searching for fields past the end of the status.
 * this destroys comment string */
next = gbCommentField->val->string;
while ((name = nextRefSeqCommentField(&next, &value)) != NULL)
    {
    if (sameString(name, "Summary"))
        dyStringAppend(gbRefSeqSummaryField->val, value);
    else if (sameString(name, "COMPLETENESS"))
        parseRefSeqCompleteness(value);
    }
}

boolean gbfReadFields(struct lineFile *lf)
/* Read in a single Gb record up to the ORIGIN. */
{
boolean gotRecord;
gbfClearVals(gbStruct);
gbfClearVals(gbRefSeqRoot);
gbMiscDiffFreeList(&gbMiscDiffVals);
gReachedEOR = FALSE;
isInvitrogenEvilEntry = FALSE;
isAthersysRageEntry = FALSE;
isOrestesEntry = FALSE;
gotRecord = recurseReadFields(lf, gbStruct, -1);
if (gotRecord)
    {
    isOrestesEntry = checkForOrestes(gbCloneLibField->val->string);
    isAthersysRageEntry = checkForAthersysRage(gbCloneLibField->val->string);
    if (gbGuessSrcDb(getCurAcc()) == GB_REFSEQ)
        refSeqParse();
    }
slReverse(&gbMiscDiffVals);
return gotRecord;
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
    warn("no mRNA sequence for %s in %s", getCurAcc(), lf->fileName);
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
    warn("invalid mRNA bases for %s in %s", getCurAcc(), lf->fileName);
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

void gbfFlatten(struct kvt *kvt)
/* Flatten out parsed genebank data into keyVal array. */
{
kvtClear(kvt);
recurseFlatten(gbStruct, kvt);
recurseFlatten(gbRefSeqRoot, kvt);
}



/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

