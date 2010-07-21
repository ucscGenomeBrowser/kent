/* gff3ToPsl - convert a GFF3 file to a genePred file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "gff3.h"
#include "psl.h"

static char const rcsid[] = "$Id: gff3ToPsl.c,v 1.7 2010/05/25 03:45:04 markd Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "gff3ToPsl - convert a GFF3 CIGAR file to a PSL file\n"
  "usage:\n"
  "   gff3ToPsl mapFile inGff3 out.psl\n"
  "arguments:\n"
  "   mapFile    mapping of locus names to chroms and sizes.\n"
  "              File formatted:  locusName chromeName chromSize\n"
  "   inGff3     GFF3 formatted file with Gap attribute in match records\n"
  "   out.psl    PSL formatted output\n"
  "options:\n"
  "This converts:\n"
  "The first step is to parse GFF3 file, up to 50 errors are reported before\n"
  "aborting.  If the GFF3 files is successfully parse, it is converted to PSL\n"
  "\n"
  "Input file must conform to the GFF3 specification:\n"
  "   http://www.sequenceontology.org/gff3.shtml\n"
  );
}

static struct optionSpec options[] = {
    {NULL, 0},
};

static int maxParseErrs = 50;  // maximum number of errors during parse
static int maxConvertErrs = 50;  // maximum number of errors during conversion
static int convertErrCnt = 0;  // number of convert errors


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
struct gff3File *gff3File = gff3FileOpen(inGff3File, maxParseErrs, NULL);
if (gff3File->errCnt > 0)
    errAbort("%d errors parsing GFF3 file: %s", gff3File->errCnt, inGff3File); 
return gff3File;
}

static void processSourceLine(FILE *pslF, struct gff3Ann *node)
{
// what to do?
}

static boolean getNextCigarOp(char **ptr, char *op, int *size)
{
char *str = *ptr;

if ((str == NULL) || (*str == 0))
    return FALSE;

char *end = strchr(str, '+');
if (end)
    {
    *end = 0;
    *ptr = end + 1;
    }
else 
    *ptr = NULL;

*op = *str++;
*size = atoi(str);

return TRUE;
}

static void processMatchLine(FILE *pslF, struct gff3Ann *node,
    struct hash *chromHash)
{
int blocksAlloced = 20;
struct gff3Attr *attr = gff3AnnFindAttr(node, "Gap");

if ((attr == NULL) || (attr->vals == NULL) || (attr->vals->name == NULL))
    errAbort("match record without Gap attribute");

// should look up targetId in chromHash to find chrom and size
printf("id %s %d %d: Gap %s\n", node->targetId, node->targetStart, node->targetEnd, attr->vals->name);

struct psl *psl = pslNew(node->seqid, node->end - node->start, node->start, 
    node->end, node->targetId, node->targetEnd - node->targetStart,
    node->targetStart, node->targetEnd, node->strand, blocksAlloced, 0);
char *ptr = attr->vals->name;
char op;
int size;
int tStart = node->targetStart;
int qStart = node->start;

while(getNextCigarOp(&ptr, &op, &size))
    {
    printf("%c %d\n",op,size);
    switch (op)
        {
        case 'M': // match or mismatch (gapless aligned block)
            if (psl->blockCount == blocksAlloced)
                pslGrow(psl, &blocksAlloced);

            psl->blockSizes[psl->blockCount] = size;
            psl->qStarts[psl->blockCount] = qStart;
            psl->tStarts[psl->blockCount] = tStart;
            psl->blockCount++;
            tStart += size;
            qStart += size;
            break;

        case 'I': // inserted in query
        case 'S': // skipped query bases at beginning or end ("soft clipping")
            qStart += size;
            break;
        case 'D': // deleted from query
        case 'N': // long deletion from query (intron as opposed to small del)
            tStart += size;
            break;
        
        default:
            errAbort("bamToFfAli: unrecognized CIGAR op %c -- update me", op);
        }
    }
pslOutput(psl, pslF, '\t' , '\n');
pslFree(&psl);
}

static void processRoot(FILE *pslF, struct gff3Ann *node, 
    struct hash *processed, struct hash *chromHash)
/* process a root node in the tree */
{
recProcessed(processed, node);

if (sameString(node->type, "source"))
    processSourceLine(pslF, node);
else if (sameString(node->type, "match"))
    processMatchLine(pslF, node, chromHash);
else
    cnvError("no support for type %s\n", node->type);
}

struct hash *readChromMap(char *mapFile)
{
return (struct hash *)0;
}

static void gff3ToPsl(char *mapFile, char *inGff3File, char *outPSL)
/* gff3ToPsl - convert a GFF3 file to a genePred file. */
{
struct hash *chromHash = readChromMap(mapFile);
struct hash *processed = hashNew(12);
struct gff3File *gff3File = loadGff3(inGff3File);
FILE *pslF = mustOpen(outPSL, "w");
struct gff3AnnRef *root;
for (root = gff3File->roots; root != NULL; root = root->next)
    {
    if (!isProcessed(processed, root->ann))
        {
        processRoot(pslF, root->ann, processed, chromHash);
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
if (argc != 4)
    usage();
gff3ToPsl(argv[1], argv[2], argv[3]);
return 0;
}
