/*
 * Object for accessing GFF3 files
 * See GFF3 specification for details of file format:
 *   http://www.sequenceontology.org/gff3.shtml
 */
#include "common.h"
#include "gff3.h"
#include <limits.h>
#include "errabort.h"
#include "localmem.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "fa.h"

// FIXME: spec unclear if attributes can be specified multiple times
// FIXME: spec unclear on attribute of discontinuous features.
// FIXME: should spaces be striped from attributes?

/*
 * Notes:
 *   - a separate feature object that linked discontinuous feature annotations
 *     was not used because it create more complexity with the linking of parents
 *     and the fact that the restriction on discontinguous features attributes is
 *     not clearly defined.
 */

static const int gffNumCols = 9;

/* standard attribute names */
char *gff3AttrID = "ID";
char *gff3AttrName = "Name";
char *gff3AttrAlias = "Alias";
char *gff3AttrParent = "Parent";
char *gff3AttrTarget = "Target";
char *gff3AttrGap = "Gap";
char *gff3AttrDerivesFrom = "Derives_from";
char *gff3AttrNote = "Note";
char *gff3AttrDbxref = "Dbxref";
char *gff3AttrOntologyTerm = "Ontology_term";

/* commonly used features names */
char *gff3FeatGene = "gene";
char *gff3FeatMRna = "mRNA";
char *gff3FeatExon = "exon";
char *gff3FeatCDS = "CDS";
char *gff3FeatThreePrimeUTR = "three_prime_UTR";
char *gff3FeatFivePrimeUTR = "five_prime_UTR";
char *gff3FeatStartCodon = "start_codon";
char *gff3FeatStopCodon = "stop_codon";

static bool gff3FileStopDueToErrors(struct gff3File *g3f)
/* determine if we should stop due to the number of errors */
{
return g3f->errCnt > g3f->maxErr;
}

static void gff3FileErr(struct gff3File *g3f, char *format, ...)
#if defined(__GNUC__)
__attribute__((format(printf, 2, 3)))
#endif
;

static void gff3AnnErr(struct gff3Ann *g3a, char *format, ...)
#if defined(__GNUC__)
__attribute__((format(printf, 2, 3)))
#endif
;

static void vaGff3FileErr(struct gff3File *g3f, char *format, va_list args)
/* Print error message to error file, abort if max errors have been reached */
{
if (g3f->lf != NULL)
    fprintf(g3f->errFh, "%s:%d: ", g3f->lf->fileName, g3f->lf->lineIx);
vfprintf(g3f->errFh, format, args);
fprintf(g3f->errFh, "\n");
g3f->errCnt++;
if (gff3FileStopDueToErrors(g3f))
    errAbort("GFF3: %d parser errors", g3f->errCnt);
}

static void gff3FileErr(struct gff3File *g3f, char *format, ...)
/* Print error message and abort */
{
va_list args;
va_start(args, format);
vaGff3FileErr(g3f, format, args);
va_end(args);
}

static void gff3AnnErr(struct gff3Ann *g3a, char *format, ...)
/* Print error message abort */
{
va_list args;
va_start(args, format);
vaGff3FileErr(g3a->file, format, args);
va_end(args);
}

static int gff3FileStrToInt(struct gff3File *g3f, char *str)
/* convert a string to an integer */
{
char *end;
long val = strtol(str, &end, 0);
if ((end == str) || (*end != '\0'))
    gff3FileErr(g3f, "invalid integer: %s", str);
return (int)val;
}

static float gff3FileStrToFloat(struct gff3File *g3f, char *str)
/* convert a string to a float  */
{
char *end;
double val = strtod(str, &end);
if ((end == str) || (*end != '\0'))
    gff3FileErr(g3f, "invalid float: %s", str);
return (float)val;
}

static void *gff3FileAlloc(struct gff3File *g3f, size_t size)
/* allocate memory from the memory pool */
{
return lmAlloc(g3f->pool->lm, size);
}

static char *gff3FileCloneStr(struct gff3File *g3f, char *str)
/* allocate memory for a string and copy if */
{
return lmCloneString(g3f->pool->lm, str);
}

static char *gff3FilePooledStr(struct gff3File *g3f, char *str)
/* allocate memory for a string from the shared string pool */
{
return hashStore(g3f->pool, str)->name;
}

static struct slName *gff3FileSlNameNew(struct gff3File *g3f, char *name)
/* allocate slName from the memory pool */
{
return lmSlName(g3f->pool->lm, name);
}


static char **dynChopStringWhite(struct gff3File *g3f, char *str, int minWords, int maxWords, int *numWordsRet, char *desc)
/* dynamic chop string without corrupting it, generate error if expected
 * number of words not found. Free return when done. Returns NULL on
 * error. */
{
int numWords = chopByWhite(str, NULL, 0);
if ((numWords < minWords) || (numWords > maxWords))
    {
    gff3FileErr(g3f, "expected %s, got \"%s\"", desc, str);
    return NULL;
    }
// allocate buffer for both array and string
int wordsBytes = ((numWords+1)*sizeof(char**));
char **words = needMem(wordsBytes+strlen(str)+1);
char *strcp = ((char*)words)+wordsBytes;
strcpy(strcp, str);
chopByWhite(strcp, words, numWords);
words[numWords] = NULL;
if (numWordsRet != NULL)
    *numWordsRet = numWords;
return words;
}

struct gff3Attr *gff3AnnFindAttr(struct gff3Ann *g3a, char *tag)
/* find a user attribute, or NULL */
{
struct gff3Attr *attr;
for (attr = g3a->attrs; attr != NULL; attr = attr->next)
    {
    if (sameString(attr->tag, tag))
        return attr;
    }
return NULL;
}

static struct gff3AnnRef *gff3AnnRefAlloc(struct gff3Ann *g3a)
/* construct an annotation reference, allocated in the memory pool*/
{
struct gff3AnnRef *ref = gff3FileAlloc(g3a->file, sizeof(struct gff3AnnRef));
ref->ann = g3a;
return ref;
}

static void raiseInvalidEscape(struct gff3Ann *g3a, char *str)
/* raise an error about an invalid escape in a string */
{
gff3AnnErr(g3a, "invalid GFF escape sequence in string: %s", str);
}

static char convertEscape(struct gff3Ann *g3a, char *esc, char *src)
/* convert character at esc, which should start with a `%' and be a string
 * in the form `%09' */
{
if (!(isxdigit(esc[1]) && isxdigit(esc[1])))
    raiseInvalidEscape(g3a, src);
char num[3], *end;
strncpy(num, esc+1, 2);
num[2] = '\0';
long val = strtol(num, &end, 16);
if ((end == num) || (*end != '\0'))
    raiseInvalidEscape(g3a, src);
return (char)val;
}

static void unescapeStr(struct gff3Ann *g3a, char *dest, char *src)
/* remove URL-style escapes from a string. dest need only have enough
 * memory to hold src, as unescaping will not grow the string */
{
char *s = src, *d = dest;
while (*s != '\0')
    {
    if (*s == '%')
        {
        *d++ = convertEscape(g3a, s, src);
        s += 3;
        }
    else
        *d++ = *s++;
    }
*d = '\0';
}

static struct slName *unescapeSlName(struct gff3Ann *g3a, char *src)
/* unescape the string and put in an slName object, storing it in memory
 * allocated from localmem  */
{
struct slName *dest = gff3FileAlloc(g3a->file, sizeof(struct slName)+strlen(src));
unescapeStr(g3a, dest->name, src);
return dest;
}

static char *unescapeStrTmp(struct gff3Ann *g3a, char *src)
/* unescape the string into a tmp buffer. WARNING: return is a static and
 * maybe invalidated by calling another unescape function */
{
static struct dyString *buf = NULL;  // buffer for tmp copy of unescaped string
if (buf == NULL)
    buf = dyStringNew(256);
dyStringBumpBufSize(buf, strlen(src)+1);
unescapeStr(g3a, buf->string, src);
return buf->string;
}

static char *unescapeStrPooled(struct gff3Ann *g3a, char *src)
/* unescape the string and obtain it from localmem  */
{
return gff3FilePooledStr(g3a->file, unescapeStrTmp(g3a, src));
}

static char *escapeChar(char c)
/* escape a character.  Warning: static return */
{
static char ec[4];
safef(ec, sizeof(ec), "%%%02X", c);
return ec;
}

static boolean isMetaChar(char c)
/* determine if a character is a GFF3 meta character */
{
return ((c == '\t') || (c == '\n') || (c == '\r') || !isprint(c) || (c == ';') || (c == '=') || (c == '&') || (c == ','));
}

static void writeEscaped(char *str, FILE *fh)
/* write a data string to a file, escaping as needed */
{
char *c;
for (c = str; *c != '\0'; c++)
    {
    if (isMetaChar(*c))
        fputs(escapeChar(*c), fh);
    else
        fputc(*c, fh);
    }
}

static char *parseStrand(struct gff3Ann *g3a, char *strand)
/* parse strand into static string, validating it */
{
if (sameString(strand, "."))
    return NULL;
else if (sameString(strand, "+"))
    return "+";
else if (sameString(strand, "-"))
    return "-";
else if (sameString(strand, "?"))
    return "?";
else
    gff3AnnErr(g3a, "invalid strand: '%s'", strand);
return NULL;
}

static int parsePhase(struct gff3Ann *g3a, char *str)
/* parse phase into a number, validating it */
{
if (sameString(str, "."))
    return -1;
int phase = gff3FileStrToInt(g3a->file, str);
if ((phase < 0) || (phase  > 2))
    gff3AnnErr(g3a, "invalid phase: %d", phase);
return phase;
}

static void parseFields(struct gff3Ann *g3a, char **words)
/* parse the field in an annotation record */
{
g3a->seqid = unescapeStrPooled(g3a, words[0]);
g3a->source = unescapeStrPooled(g3a, words[1]);
g3a->type = unescapeStrPooled(g3a, words[2]);
g3a->start = gff3FileStrToInt(g3a->file, words[3])-1;
g3a->end = gff3FileStrToInt(g3a->file, words[4]);
if (!sameString(words[5], "."))
    {
    g3a->score = gff3FileStrToFloat(g3a->file, words[5]);
    g3a->haveScore = TRUE;
    }
g3a->strand = parseStrand(g3a, words[6]);
g3a->phase = parsePhase(g3a, words[7]);
if (sameString(g3a->type, "CDS"))
    {
    if (g3a->phase < 0)
        gff3AnnErr(g3a, "CDS feature must have phase");
    }
else
    {
#if 0 // spec unclear; bug report filed
    // spec currently doesn't restrict phase, unclear if it's allowed on start/stop codon features
    if (g3a->phase >= 0)
        gff3AnnErr(g3a, "phase only allowed on CDS features");
#endif
    }
}

/* check that an attribute tag name is valid. */
static boolean checkAttrTag(struct gff3Ann *g3a, char *tag)
{
// FIXME: spec is not clear on what is a valid tag.
char *tc = tag;
boolean isOk = isalpha(*tc);
for (tc++; isOk && (*tc != '\0'); tc++)
    isOk = (*tc == '_') || isalnum(*tc);
if (!isOk)
    gff3AnnErr(g3a, "invalid attribute tag, must start with an alphabetic character and be composed of alphanumeric or underscore characters: %s", tag);
return isOk;
}

static struct slName *parseAttrVals(struct gff3Ann *g3a, char *tag, char *valsStr)
/* parse an attribute into its values */
{
int i, numVals = chopString(valsStr, ",", NULL, 0);
char **vals = needMem((numVals+1)*sizeof(char**)); // +1 allows for no values
chopString(valsStr, ",", vals, numVals);
struct slName *unescVals = NULL;
for (i = 0; i < numVals; i++)
    slAddHead(&unescVals, unescapeSlName(g3a, vals[i]));
if (unescVals == NULL)
    slAddHead(&unescVals, slNameNew(""));  // empty value
freeMem(vals);
slReverse(&unescVals);
return unescVals;
}

static void addAttr(struct gff3Ann *g3a, char *tag, char *valStr)
/* Add an attribute to the list of attributes.  If attribute has already been
 * specified, values are merged.  Attribute name must already be unescaped,
 * attribute values will be split and then unescaped. */
{
struct gff3Attr *attr = gff3AnnFindAttr(g3a, tag);
if (attr == NULL)
    {
    attr = gff3FileAlloc(g3a->file, sizeof(struct gff3Attr));
    attr->tag = gff3FileCloneStr(g3a->file, tag);
    slAddHead(&g3a->attrs, attr);
    }
attr->vals = slCat(attr->vals, parseAttrVals(g3a, tag, valStr));
}

static void parseAttr(struct gff3Ann *g3a, char *attrStr)
/* parse one attribute from an annotation record */
{
char *eq = strchr(attrStr, '=');
if ((eq == NULL) || (eq == attrStr))
    gff3AnnErr(g3a, "expected name=value: %s", attrStr);
else
    {
    char *tag = attrStr;
    char *vals = eq+1;
    *eq = '\0';
    unescapeStr(g3a, tag, tag);
    if (checkAttrTag(g3a, tag))
        addAttr(g3a, tag, vals);
    }
}

static void parseAttrs(struct gff3Ann *g3a, char *attrsCol)
/* parse the attribute column in an annotation record */
{
int i, numAttrs = chopString(attrsCol, ";", NULL, 0);
char **attrStrs = needMem(numAttrs*sizeof(char**));
chopString(attrsCol, ";", attrStrs, numAttrs);
for (i = 0; i < numAttrs; i++)
    {
    char *attrStr = trimSpaces(attrStrs[i]);
    if (strlen(attrStr) > 0)
        parseAttr(g3a, attrStr);
    }
freeMem(attrStrs);
slReverse(&g3a->attrs);
}

static void checkSingleValAttr(struct gff3Ann *g3a, struct gff3Attr *attr)
/* validate that an attribute has only one value */
{
if (attr->vals->next != NULL)
    gff3AnnErr(g3a, "attribute %s must have a single value, found multiple comma-separated values", attr->tag);
}

static void parseIDAttr(struct gff3Ann *g3a, struct gff3Attr *attr)
/* parse the ID attribute */
{
checkSingleValAttr(g3a, attr);
g3a->id = attr->vals->name;
// link into other parts of feature if discontinuous
struct hashEl *hel = hashStore(g3a->file->byId, g3a->id);
struct gff3Ann *head = hel->val;
if (head != NULL)
    head->prevPart = g3a;
g3a->nextPart = head;
hel->val = g3a;
}

static void parseNameAttr(struct gff3Ann *g3a, struct gff3Attr *attr)
/* parse the Name attribute */
{
checkSingleValAttr(g3a, attr);
g3a->name = attr->vals->name;
}

static void parseAliasAttr(struct gff3Ann *g3a, struct gff3Attr *attr)
/* parse the Alias attribute */
{
g3a->aliases = attr->vals;
}

static void parseParentAttr(struct gff3Ann *g3a, struct gff3Attr *attr)
/* parse the Parent attribute */
{
g3a->parentIds = attr->vals;
}

static void parseTargetAttr(struct gff3Ann *g3a, struct gff3Attr *attr)
/* parse the Target attribute */
{
checkSingleValAttr(g3a, attr);

// target_id start end [strand]
int numWords;
char **words = dynChopStringWhite(g3a->file, attr->vals->name, 3, 4, &numWords,
                                  "Target attribute in the form \"target_id start end [strand]\"");
if (words == NULL)
    return;  // got an error
g3a->targetId = gff3FileCloneStr(g3a->file, words[0]);
g3a->targetStart = gff3FileStrToInt(g3a->file, words[1])-1;
g3a->targetEnd = gff3FileStrToInt(g3a->file, words[2]);
if (numWords > 3)
    g3a->targetStrand = parseStrand(g3a, words[3]);
freeMem(words);
}

static void parseGapAttr(struct gff3Ann *g3a, struct gff3Attr *attr)
/* parse the Gap attribute */
{
checkSingleValAttr(g3a, attr);
g3a->gap = attr->vals->name;
}

static void parseDerivesFromAttr(struct gff3Ann *g3a, struct gff3Attr *attr)
/* parse the Derives_from attribute */
{
g3a->derivesFromId = attr->vals->name;
}

static void parseNoteAttr(struct gff3Ann *g3a, struct gff3Attr *attr)
/* parse the Note attribute */
{
g3a->notes = attr->vals;
}

static void parseDbxrefAttr(struct gff3Ann *g3a, struct gff3Attr *attr)
/* parse the Dbxref attribute */
{
g3a->dbxrefs = attr->vals;
}

static void parseOntologyTermAttr(struct gff3Ann *g3a, struct gff3Attr *attr)
/* parse the Ontology_term attribute */
{
g3a->ontologyTerms = attr->vals;
}

static void parseStdAttr(struct gff3Ann *g3a, struct gff3Attr *attr)
/* Parse one of the standard specified attributes (those starting with upper
 * case) into fields. Multiple specifications of an attribute should have been
 * merged before calling this function. */
{
if (sameString(attr->tag, gff3AttrID))
    parseIDAttr(g3a, attr);
else if (sameString(attr->tag, gff3AttrName))
    parseNameAttr(g3a, attr);
else if (sameString(attr->tag, gff3AttrAlias))
    parseAliasAttr(g3a, attr);
else if (sameString(attr->tag, gff3AttrParent))
    parseParentAttr(g3a, attr);
else if (sameString(attr->tag, gff3AttrTarget))
    parseTargetAttr(g3a, attr);
else if (sameString(attr->tag, gff3AttrGap))
    parseGapAttr(g3a, attr);
else if (sameString(attr->tag, gff3AttrDerivesFrom))
    parseDerivesFromAttr(g3a, attr);
else if (sameString(attr->tag, gff3AttrNote))
    parseNoteAttr(g3a, attr);
else if (sameString(attr->tag, gff3AttrDbxref))
    parseDbxrefAttr(g3a, attr);
else if (sameString(attr->tag, gff3AttrOntologyTerm))
    parseOntologyTermAttr(g3a, attr);
else
    gff3AnnErr(g3a, "unknown standard attribute, user defined attributes must start with a lower-case letter: %s", attr->tag);
}

static void parseStdAttrs(struct gff3Ann *g3a)
/* parse standard attributes (starting with upper case) into attributes
 * have been parsed into attribute list, which would have  merged multiply
 * specified attributes. */
{
struct gff3Attr *attr;
for (attr = g3a->attrs; attr != NULL; attr = attr->next)
    {
    if (isupper(attr->tag[0]))
        parseStdAttr(g3a, attr);
    }
}

static void parseAnn(struct gff3File *g3f, char *line)
/* parse an annotation line */
{
// extra column to check for too many
char *words[gffNumCols+1];
int numWords = chopString(line, "\t", words, gffNumCols+1);
if (numWords != gffNumCols)
    gff3FileErr(g3f, "expected %d tab-separated columns: %s", gffNumCols, line);

struct gff3Ann *g3a = gff3FileAlloc(g3f, sizeof(struct gff3Ann));
g3a->file = g3f;
g3a->lineNum = g3f->lf->lineIx;
parseFields(g3a, words);
parseAttrs(g3a, words[8]);
parseStdAttrs(g3a);
slAddHead(&g3f->anns, gff3AnnRefNew(g3a));
}

static void writeAttr(struct gff3Attr *attr, FILE *fh)
/* write one attribute and it's values */
{
writeEscaped(attr->tag, fh);
fputc('=', fh);
struct slName *val;
for (val = attr->vals; val != NULL; val = val->next)
    {
    if (val != attr->vals)
        fputc(',', fh);
    writeEscaped(val->name, fh);
    }
}

static void writeAttrs(struct gff3Ann *g3a, FILE *fh)
/* write annotation record attributes */
{
struct gff3Attr *attr;
for (attr = g3a->attrs; attr != NULL; attr = attr->next)
    {
    if (attr != g3a->attrs)
        fputc(';', fh);
    writeAttr(attr, fh);
    }
}

static void writeFields(struct gff3Ann *g3a, FILE *fh)
/* write an annotation record fields */
{
writeEscaped(g3a->seqid, fh);
fputc('\t', fh);
writeEscaped(g3a->source, fh);
fputc('\t', fh);
writeEscaped(g3a->type, fh);
fprintf(fh, "\t%d\t%d", g3a->start+1, g3a->end);
fputc('\t', fh);
if (g3a->haveScore)
    fprintf(fh, "%g", g3a->score);
else
    fputc('.', fh);
fprintf(fh, "\t%c", (g3a->strand != NULL) ? g3a->strand[0] : '.');
fputc('\t', fh);
if (g3a->phase < 0)
    fputc('.', fh);
else
    fprintf(fh, "%d", g3a->phase);
}

static void writeAnn(struct gff3Ann *g3a, FILE *fh)
/* write an annotation record to the specified file */
{
writeFields(g3a, fh);
fputc('\t', fh);
writeAttrs(g3a, fh);
fputc('\n', fh);
}

static void addFasta(struct gff3File *g3f, char *dna, int size, char *name)
/* add one fasta record */
{
struct dnaSeq *dnaSeq = gff3FileAlloc(g3f, sizeof(struct dnaSeq));
slAddHead(&g3f->seqs, dnaSeq);
struct hashEl *hel = hashAdd(g3f->seqMap, name, dnaSeq);
dnaSeq->name = hel->name;
dnaSeq->dna = gff3FileCloneStr(g3f, dna);
dnaSeq->size = size;
}

static void parseFasta(struct gff3File *g3f)
/* parse fasta records in the file, consumes remainder of file */
{
char *dna, *name;
int size;
g3f->seqMap = hashNew(0);
while (faMixedSpeedReadNext(g3f->lf, &dna, &size, &name))
    addFasta(g3f, dna, size, name);
}

static void writeFastas(struct gff3File *g3f, FILE *fh)
/* write fasta records fo the file */
{
if (g3f->seqs != NULL)
    {
    fputs("##FASTA\n", fh);
    struct dnaSeq *seq;
    for (seq = g3f->seqs; seq != NULL; seq = seq->next)
        faWriteNext(fh, seq->name, seq->dna, seq->size);
    }
}

static void parseSequenceRegion(struct gff3File *g3f, char *line)
/* parse ##sequence-region seqid start end */
{
char **words = dynChopStringWhite(g3f, line, 4, 4, NULL,
                                  "\"##sequence-region seqid start end\"");
if (words == NULL)
    return;  // got an error
struct gff3SeqRegion *sr = gff3FileAlloc(g3f, sizeof(struct gff3SeqRegion));
sr->seqid = gff3FileCloneStr(g3f, words[1]);
sr->start = gff3FileStrToInt(g3f, words[2])-1;
sr->end = gff3FileStrToInt(g3f, words[3]);
if (g3f->seqRegionMap == NULL)
    g3f->seqRegionMap = hashNew(0);
struct hashEl *hel = hashStore(g3f->seqRegionMap, sr->seqid);
if (hel->val != NULL)
    gff3FileErr(g3f, "duplicate ##sequence-region for %s", sr->seqid);
else
    {
    hel->val = sr;
    slAddHead(&g3f->seqRegions, sr);
    }
freeMem(words);
}

static void writeSequenceRegions(struct gff3File *g3f, FILE *fh)
/* parse ##sequence-region metadata */
{
struct gff3SeqRegion *sr;
for (sr = g3f->seqRegions; sr != NULL; sr = sr->next)
    fprintf(fh, "##sequence-region %s %d %d\n", sr->seqid, sr->start, sr->end);
}

static void writeSlNameMetas(char *metaName, struct slName *metas, FILE *fh)
/* write meta records stores as slNames */
{
struct slName  *m;
for (m = metas; m != NULL; m = m->next)
    fprintf(fh, "%s %s\n", metaName, m->name);
}

static void parseFeatureOntology(struct gff3File *g3f, char *line)
/* parse ##feature-ontology URI  */
{
char **words = dynChopStringWhite(g3f, line, 2, 2, NULL,
                                  "\"##feature-ontology URI\"");
if (words == NULL)
    return;  // got an error
slSafeAddHead(&g3f->featureOntologies, gff3FileSlNameNew(g3f, words[1]));
freeMem(words);
}

static void writeFeatureOntologies(struct gff3File *g3f, FILE *fh)
/* parse ##feature-ontology metas */
{
writeSlNameMetas("##feature-ontology", g3f->featureOntologies, fh);
}

static void parseAttributeOntology(struct gff3File *g3f, char *line)
/* parse ##attribute-ontology URI */
{
char **words = dynChopStringWhite(g3f, line, 2, 2, NULL,
                                  "\"##attribute-ontology URI\"");
if (words == NULL)
    return;  // got an error
slSafeAddHead(&g3f->attributeOntologies, gff3FileSlNameNew(g3f, words[1]));
freeMem(words);
}

static void writeAttributeOntologies(struct gff3File *g3f, FILE *fh)
/* write ##attribute-ontology metas */
{
writeSlNameMetas("##attribute-ontology", g3f->attributeOntologies, fh);
}

static void parseSourceOntology(struct gff3File *g3f, char *line)
/* parse ##source-ontology URI */
{
char **words = dynChopStringWhite(g3f, line, 2, 2, NULL,
                                  "\"##source-ontology URI\"");
if (words == NULL)
    return;  // got an error
slSafeAddHead(&g3f->sourceOntologies, gff3FileSlNameNew(g3f, words[1]));
freeMem(words);
}

static void writeSourceOntologies(struct gff3File *g3f, FILE *fh)
/* write ##source-ontology metas */
{
writeSlNameMetas("##source-ontology", g3f->sourceOntologies, fh);
}

static void parseSpecies(struct gff3File *g3f, char *line)
/* parse ##species NCBI_Taxonomy_URI */
{
char **words = dynChopStringWhite(g3f, line, 2, 2, NULL,
                                  "\"##species NCBI_Taxonomy_URI\"");
if (words == NULL)
    return;  // got an error
slSafeAddHead(&g3f->species, gff3FileSlNameNew(g3f, words[1]));
freeMem(words);
}

static void writeSpecies(struct gff3File *g3f, FILE *fh)
/* write ##species NCBI_Taxonomy_URI */
{
writeSlNameMetas("##species", g3f->species, fh);
}

static void parseGenomeBuild(struct gff3File *g3f, char *line)
/* parse ##genome-build source buildName */
{
if (g3f->genomeBuildSource != NULL)
    gff3FileErr(g3f, "multiple ##genome-build records");
char **words = dynChopStringWhite(g3f, line, 3, 3, NULL,
                                  "\"##genome-build source buildName\"");
if (words == NULL)
    return;  // got an error
g3f->genomeBuildSource = gff3FileCloneStr(g3f, words[1]);
g3f->genomeBuildName = gff3FileCloneStr(g3f, words[2]);
freeMem(words);
}

static void writeGenomeBuild(struct gff3File *g3f, FILE *fh)
/* parse ##genome-build source buildName */
{
if (g3f->genomeBuildSource != NULL)
    fprintf(fh, "##genome-build %s %s\n", g3f->genomeBuildSource, g3f->genomeBuildName);
}

static void parseMeta(struct gff3File *g3f, char *line)
/* parse a meta line of a gff3 file */
{
eraseTrailingSpaces(line);
if (sameString("###", line))
    ; // ignore
else if (sameString("##FASTA", line))
    parseFasta(g3f);
else if (startsWithWord("##sequence-region", line))
    parseSequenceRegion(g3f, line);
else if (startsWithWord("##feature-ontology", line))
    parseFeatureOntology(g3f, line);
else if (startsWithWord("##attribute-ontology", line))
    parseAttributeOntology(g3f, line);
else if (startsWithWord("##source-ontology", line))
    parseSourceOntology(g3f, line);
else if (startsWithWord("##species", line))
    parseSpecies(g3f, line);
else if (startsWithWord("##genome-build", line))
    parseGenomeBuild(g3f, line);
else if (startsWithWord("##gff-spec-version", line) ||
         startsWithWord("##source-version", line) ||
         startsWithWord("##date", line) ||
         startsWithWord("##Type", line))
    ;  /* FIXME: silently ignore these.  Mark says. */
else
    gff3FileErr(g3f, "invalid meta line: %s", line);
}

static void parseLine(struct gff3File *g3f, char *line)
/* parse one line of a gff3 file */
{
if (startsWith("##", line))
    parseMeta(g3f, line);
else if (!startsWith("#", line) && (strlen(line) > 0))
    parseAnn(g3f, line);
}

static void parseHeader(struct gff3File *g3f)
/* parse and validate a GFF3 header */
{
char *line;
if (!lineFileNext(g3f->lf, &line, NULL))
    gff3FileErr(g3f, "empty GFF file, must have header");
char *ver = skipToSpaces(line);
if (*ver != '\0')
    {
    *ver++ = '\0';
    ver = trimSpaces(ver);
    }
if (!(sameString(line, "##gff-version") && sameString(ver, "3")))
    gff3FileErr(g3f, "invalid GFF3 header");
}

static void parseFile(struct gff3File *g3f)
/* do parsing phase of reading a GFF3 file */
{
g3f->lf = lineFileOpen(g3f->fileName, TRUE);
parseHeader(g3f);
char *line;
while (lineFileNext(g3f->lf, &line, NULL))
    {
    parseLine(g3f, line);
    if (gff3FileStopDueToErrors(g3f))
        break;
    }
lineFileClose(&g3f->lf);
slReverse(&g3f->anns);
}

static int gff3AnnCount(struct gff3Ann *g3a)
/* count the number of gff3Ann objects linked together in a feature */
{
int cnt = 0;
for (; g3a != NULL; g3a = g3a->nextPart)
    cnt++;
return cnt;
}

static void discontinFeatureCheck(struct gff3Ann *g3a)
/* sanity check linked gff3Ann discontinuous features */
{
struct gff3Ann *g3a2;
for (g3a2 = g3a->nextPart; (g3a2 != NULL) && !gff3FileStopDueToErrors(g3a->file); g3a2 = g3a2->nextPart)
    {
    if (!sameString(g3a->type, g3a2->type))
        gff3AnnErr(g3a, "Annotation records for discontinuous features with ID=\"%s\" do not have the same type, found \"%s\" and \"%s\"", g3a->id, g3a->type, g3a2->type);
    }
}

static void discontinFeatureFillArray(struct gff3Ann *g3a, int numAnns, struct gff3Ann *featAnns[])
/* convert list to array for sorting */
{
int i = 0;
for (; g3a != NULL; g3a = g3a->nextPart)
    featAnns[i++] = g3a;
}

static struct gff3Ann *discontinFeatureArrayLink(int numAnns, struct gff3Ann *featAnns[])
/* convert sorted array to a list */
{
struct gff3Ann *g3aHead = NULL, *g3aPrev = NULL;
int i;
for (i = 0; i < numAnns; i++)
    {
    if (g3aHead == NULL)
        g3aHead = featAnns[i];
    if (g3aPrev != NULL)
        g3aPrev->nextPart = featAnns[i];
    featAnns[i]->prevPart = g3aPrev;
    }
return g3aHead;
}

static int discontigFeatureSortCmp(const void *p1, const void *p2)
/* compare function for discontigFeatureSort */
{
struct gff3Ann *g3a1 = *((struct gff3Ann **)p1);
struct gff3Ann *g3a2 = *((struct gff3Ann **)p2);
int diff = g3a1->start - g3a2->start;
if (diff == 0)
    diff = g3a1->end - g3a2->end;
return diff;
}

static struct gff3Ann *discontigFeatureSort(struct gff3Ann *g3a)
/* sort a list of gff3Ann object representing discontinuous */
{
int numAnns = gff3AnnCount(g3a);
struct gff3Ann *featAnns[numAnns];
discontinFeatureFillArray(g3a, numAnns, featAnns);
qsort(featAnns, numAnns, sizeof(struct gff3Ann*), discontigFeatureSortCmp);
return discontinFeatureArrayLink(numAnns, featAnns);
}

static void discontigFeatureFinish(struct gff3File *g3f)
/* finish up discontinuous features, sorting them into ascending order */
{
// only both sorting if more than one annotation
struct hashCookie cookie = hashFirst(g3f->byId);
struct hashEl *hel;
while (((hel = hashNext(&cookie)) != NULL) && !gff3FileStopDueToErrors(g3f))
    {
    struct gff3Ann *g3a = hel->val;
    if (g3a->nextPart != NULL)
        {
        discontinFeatureCheck(g3a);
        hel->val = discontigFeatureSort(g3a);
        }
    }
}

static struct gff3Ann *resolveRef(struct gff3Ann *g3a, char *id, char *attr)
/* resolve a link for an attribute */
{
struct gff3Ann *ann = gff3FileFindAnn(g3a->file, id);
if (ann == NULL)
    gff3AnnErr(g3a, "Can't find annotation record \"%s\" referenced by \"%s\" %s attribute", id, g3a->id, attr);
return ann;
}

static struct gff3AnnRef *resolveRefs(struct gff3Ann *g3a, struct slName *ids, char *attr)
/* resolve links for an attribute */
{
struct gff3AnnRef *refs = NULL;
struct slName *id;
for (id = ids; id != NULL; id = id->next)
    {
    struct gff3Ann *ann = resolveRef(g3a, id->name, attr);
    if (ann != NULL)
        slSafeAddHead(&refs, gff3AnnRefAlloc(ann));
    }
return refs;
}

static void resolveAnn(struct gff3Ann *g3a)
/* resolve links for an gff3Ann */
{
g3a->parents = resolveRefs(g3a, g3a->parentIds, gff3AttrParent);
if (g3a->parents == NULL)
    slSafeAddHead(&g3a->file->roots, gff3AnnRefAlloc(g3a));
else
    {
    struct gff3AnnRef *par;
    for (par = g3a->parents; par != NULL; par = par->next)
        slSafeAddHead(&par->ann->children, gff3AnnRefAlloc(g3a));
    }
if (g3a->derivesFromId != NULL)
    g3a->derivesFrom = resolveRef(g3a, g3a->derivesFromId, gff3AttrDerivesFrom);
}

static void resolveAnns(struct gff3File *g3f)
/* resolve links */
{
struct gff3AnnRef *g3aRef;
for (g3aRef = g3f->anns; (g3aRef != NULL) && !gff3FileStopDueToErrors(g3f); g3aRef = g3aRef->next)
    resolveAnn(g3aRef->ann);
}

static void resolveFile(struct gff3File *g3f)
/* do resolution phase of reading a GFF3 file */
{
// must sort first, as links point to the first feature
discontigFeatureFinish(g3f);
resolveAnns(g3f);
// reorder just for test reproducibility
slReverse(&g3f->seqRegions);
slReverse(&g3f->featureOntologies);
slReverse(&g3f->attributeOntologies);
slReverse(&g3f->sourceOntologies);
slReverse(&g3f->species);
slReverse(&g3f->seqs);
}

static struct gff3File *gff3FileNew()
/* construct a new, empty gff3File object */
{
struct gff3File *g3f;
AllocVar(g3f);
g3f->byId = hashNew(0);
g3f->pool = hashNew(0);
return g3f;
}

struct gff3File *gff3FileOpen(char *fileName, int maxErr, FILE *errFh)
/* Parse a GFF3 file into a gff3File object.  If maxErr not zero, then
 * continue to parse until this number of error have been reached.  A maxErr
 * less than zero does not stop reports all errors. Write errors to errFh,
 * if NULL, use stderr. */
{
struct gff3File *g3f = gff3FileNew();
g3f->fileName = gff3FileCloneStr(g3f, fileName);
g3f->errFh = (errFh != NULL) ? errFh : stderr;
g3f->maxErr = (maxErr < 0) ? INT_MAX : maxErr;
parseFile(g3f);
if (!gff3FileStopDueToErrors(g3f))
    resolveFile(g3f);
if (g3f->errCnt > 0)
    errAbort("GFF3: %d parser errors", g3f->errCnt);
return g3f;
}

void gff3FileFree(struct gff3File **g3fPtr)
/* Free a gff3File object */
{
struct gff3File *g3f = *g3fPtr;
if (g3f != NULL)
    {
    hashFree(&g3f->byId);
    hashFree(&g3f->pool);
    hashFree(&g3f->seqRegionMap);
    freeMem(g3f);
    *g3fPtr = NULL;
    }
}

struct gff3Ann *gff3FileFindAnn(struct gff3File *g3f, char *id)
/* find an annotation record by id, or NULL if not found. */
{
return hashFindVal(g3f->byId, id);
}

static void writeMeta(struct gff3File *g3f, FILE *fh)
/* write meta data */
{
fputs("##gff-version 3\n", fh);
writeSequenceRegions(g3f, fh);
writeFeatureOntologies(g3f, fh);
writeAttributeOntologies(g3f, fh);
writeSourceOntologies(g3f, fh);
writeSpecies(g3f, fh);
writeGenomeBuild(g3f, fh);
}

void gff3FileWrite(struct gff3File *g3f, char *fileName)
/* write contents of an GFF3File object to a file */
{
FILE *fh = mustOpen(fileName, "w");
writeMeta(g3f, fh);
struct gff3AnnRef *g3aRef;
for (g3aRef = g3f->anns; g3aRef != NULL; g3aRef = g3aRef->next)
    writeAnn(g3aRef->ann, fh);
writeFastas(g3f, fh);
carefulClose(&fh);
}

int gff3AnnRefLocCmp(const void *va, const void *vb)
/* sort compare function for two gff3AnnRef objects */
{
const struct gff3Ann *a = (*((struct gff3AnnRef **)va))->ann;
const struct gff3Ann *b = (*((struct gff3AnnRef **)vb))->ann;
int diff = strcmp(a->seqid, b->seqid);
if ((diff == 0) && (a->strand != b->strand))
    {
    // allow for various types of strand fields. above tests handles both null
    if (a->strand == NULL)
        diff = 1;
    else if (b->strand == NULL)
        diff = -1;
    else
        diff = strcmp(a->strand, b->strand);
    }
if (diff == 0)
    diff = a->start - b->start;
if (diff == 0)
    diff = a->end - b->end;
return diff;
}
