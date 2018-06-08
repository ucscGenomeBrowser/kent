/* gff3ToPsl - convert a GFF3 file to a genePred file. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "gff3.h"
#include "psl.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "gff3ToPsl - convert a GFF3 CIGAR file to a PSL file\n"
  "usage:\n"
  "   gff3ToPsl [options] queryChromSizes targetChomSizes inGff3 out.psl\n"
  "arguments:\n"
  "   queryChromSizes file with query (main coordinates) chromosome sizes  .\n"
  "               File formatted:  chromeName<tab>chromSize\n"
  "   targetChromSizes file with target (Target attribute)  chromosome sizes .\n"
  "   inGff3     GFF3 formatted file with Gap attribute in match records\n"
  "   out.psl    PSL formatted output\n"
  "options:\n"
  "   -dropQ     drop record when query not found in queryChromSizes\n"
  "   -dropT     drop record when target not found in targetChromSizes\n"
  "This converts:\n"
  "The first step is to parse GFF3 file, up to 50 errors are reported before\n"
  "aborting.  If the GFF3 files is successfully parse, it is converted to PSL\n"
  "\n"
  "Input file must conform to the GFF3 specification:\n"
  "   http://www.sequenceontology.org/gff3.shtml\n"
  );
}

static struct optionSpec options[] =
{
    {"dropQ", OPTION_BOOLEAN},
    {"dropT", OPTION_BOOLEAN},
    {NULL, 0}
};

static int maxParseErrs = 50;  // maximum number of errors during parse
static int maxConvertErrs = 50;  // maximum number of errors during conversion
static int convertErrCnt = 0;  // number of convert errors
static boolean dropQ = FALSE;
static boolean dropT = FALSE;

struct nameAndSize
{
char *name;
int size;
};


static void cnvError(char *format, ...)
/* print a GFF3 to gene conversion error.  This will return.  Code must check
 * for error count to be exceeded and unwind to the top level to print a usefull
 * error message and abort. */
{
fputs("Error: ", stderr);
va_list args;
va_start(args, format);
vfprintf(stderr, format, args);
va_end(args);
fputc('\n', stderr);
convertErrCnt++;
}

static char *mkAnnAddrKey(struct gff3Ann *ann)
/* create a key for a gff3Ann from its address.  WARNING: static return */
{
static char buf[64];
safef(buf, sizeof(buf), "%lu", (unsigned long)ann);
return buf;
}

static boolean isProcessed(struct hash *processed, struct gff3Ann *ann)
/* has an ann record be processed? */
{
return hashLookup(processed, mkAnnAddrKey(ann)) != NULL;
}

static void recProcessed(struct hash *processed, struct gff3Ann *ann)
/* add an ann record to processed hash */
{
hashAdd(processed, mkAnnAddrKey(ann), ann);
}

static struct gff3File *loadGff3(char *inGff3File)
/* load GFF3 into memory */
{
struct gff3File *gff3File = gff3FileOpen(inGff3File, maxParseErrs, 0, NULL);
if (gff3File->errCnt > 0)
    errAbort("%d errors parsing GFF3 file: %s", gff3File->errCnt, inGff3File); 
return gff3File;
}

static void processSourceLine(FILE *pslF, struct gff3Ann *node)
{
// what to do?
}

static struct nameAndSize *getNameAndSize(struct hash *hash, char *name, boolean target)
/* Find size of name in hash or die trying if allowed to. */
{
struct hashEl *hel = hashLookup(hash, name);
if (hel == NULL)
{
    if (target)
       {
       if (dropT)
          return NULL;
       }
    else if (dropQ)
            return NULL;
    errAbort("couldn't find %s in %s.chrom.sizes file", name, target ? "target" : "query");
}
return hel->val;
}

static boolean checkTarget(struct gff3Ann *node,
                           struct nameAndSize *nsT)
/* check the Target attribute values, which were bogus in some NCBI genome
 * alignments */
{
// we had a bogus target range
if (node->targetStart >= node->targetEnd)
    {
    cnvError("zero or negative Target attribute range %s:%d-%d\n", node->targetId, node->targetStart, node->targetEnd);
    return FALSE;
    }
if ((node->targetStart < 0) || (node->targetEnd > nsT->size))
    {
    cnvError("Target attribute range %s:%d-%d outside of sequence range %s:%d-%d \n", node->targetId, node->targetStart, node->targetEnd,
             nsT->name, 0, nsT->size);
    return FALSE;
    }
return TRUE;
}

static char checkStrand(char strand, char *strandType)
{
if ((strand == '+') || (strand == '-'))
    return strand;
cnvError("illegal %s strand %c\n", strandType, strand);
return '+';
}

static void processMatchLine(FILE *pslF, struct gff3Ann *node,
                             struct hash *queryChromSizes, struct hash *targetChromSizes)
{
struct gff3Attr *attr = gff3AnnFindAttr(node, "Gap");

char *cigar = NULL;
if (!((attr == NULL) || (attr->vals == NULL) || isEmpty(attr->vals->name)))
    cigar = attr->vals->name;

struct nameAndSize *nsT = getNameAndSize(targetChromSizes, node->targetId, TRUE);
struct nameAndSize *nsQ = getNameAndSize(queryChromSizes, node->seqid, FALSE);
if (NULL == nsT || NULL == nsQ)
    return; // can not find chrom.sizes
if (!checkTarget(node, nsT))
    return; // invalid Target
char strand[3];
strand[0] = checkStrand(*node->strand, "query");
strand[1] = checkStrand(*node->targetStrand, "target");
strand[2] = 0;
struct psl *psl = pslFromGff3Cigar(node->seqid, nsQ->size,  node->start, node->end,
                                   nsT->name, nsT->size,  node->targetStart, node->targetEnd, 
                                   strand, cigar);
pslOutput(psl, pslF, '\t' , '\n');
// validate PSL, which can find a bad CIGAR
int pslErrCnt = pslCheck("converted GFF3 CIGAR alignment", stderr, psl);
if (pslErrCnt > 0)
    cnvError("%d errors found in generated PSL, most likely CIGAR mismatch with query or target range: %s line %d\n", pslErrCnt, node->file->fileName, node->lineNum);

pslFree(&psl);
}

static void processRoot(FILE *pslF, struct gff3Ann *node, 
                        struct hash *processed, struct hash *queryChromSizes, struct hash *targetChromSizes)
/* process a root node in the tree */
{
recProcessed(processed, node);

if (sameString(node->type, "source"))
    processSourceLine(pslF, node);
else if (sameString(node->type, "match") || sameString(node->type, "cDNA_match"))
    processMatchLine(pslF, node, queryChromSizes, targetChromSizes);
else
    cnvError("no support for type %s\n", node->type);
}


struct hash *readSizes(char *fileName)
/* Read tab-separated file into hash with
 * name key size value. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct hash *hash = newHash(0);
char *row[2];
while (lineFileRow(lf, row))
    {
    struct nameAndSize *ns;

    char *name = row[0];
    if (hashLookup(hash, name) != NULL)
        errAbort("Duplicate %s in size file %s\n", name, fileName);

    AllocVar(ns);
    ns->name = cloneString(row[0]);
    ns->size = atoi(row[1]);
    hashAdd(hash, name, ns);
    }
lineFileClose(&lf);
return hash;
}

static void gff3ToPsl(char *queryChromSizesFile, char *targetChromSizesFile, char *inGff3File, char *outPSL)
/* gff3ToPsl - convert a GFF3 file to a genePred file. */
{
struct hash *queryChromSizes = readSizes(queryChromSizesFile);
struct hash *targetChromSizes = readSizes(targetChromSizesFile);
struct hash *processed = hashNew(12);
struct gff3File *gff3File = loadGff3(inGff3File);
FILE *pslF = mustOpen(outPSL, "w");
struct gff3AnnRef *root;
for (root = gff3File->roots; root != NULL; root = root->next)
    {
    if (!isProcessed(processed, root->ann))
        {
        processRoot(pslF, root->ann, processed, queryChromSizes, targetChromSizes);
        if (convertErrCnt >= maxConvertErrs)
            break;
        }
    }
carefulClose(&pslF);
if (convertErrCnt > 0)
    errAbort("%d errors converting GFF3 file: %s", convertErrCnt, inGff3File); 

#if 0  // free memory for leak debugging if 1
gff3FileFree(&gff3File);
hashFree(&processed);
#endif
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
dropQ = optionExists("dropQ");
dropT = optionExists("dropT");
gff3ToPsl(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
