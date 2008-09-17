#include "common.h"
#include "sig.h"
#include "localmem.h"
#include "rbTree.h"
#include "hash.h"
#include "rangeTreeFile.h"
#include "genomeRangeTreeFile.h"
#include <limits.h>

static char const rcsid[] = "$Id: genomeRangeTreeFile.c,v 1.4 2008/09/17 19:45:00 mikep Exp $";

struct genomeRangeTree *genomeRangeTreeRead(char *fileName)
/* Read in the genomeRangeTree data for each chromosome and
 * return the genomeRangeTree.
 * Squawk and die if there is a problem. */
{
struct genomeRangeTreeFile *f = genomeRangeTreeFileReadHeader(fileName);
struct genomeRangeTree *tree = genomeRangeTreeFileReadData(f);
genomeRangeTreeFileFree(&f);
return tree;
}

void genomeRangeTreeWrite(struct genomeRangeTree *t, char *fileName)
/* Write out genomeRangeTree including: 
 * header portion
 * index of chromosomes
 * data for each range tree */
{
struct genomeRangeTreeFile *f = genomeRangeTreeFileNew(t, fileName);
genomeRangeTreeFileWriteHeader(f);
genomeRangeTreeFileWriteData(f);
genomeRangeTreeFileFree(&f);
}

struct genomeRangeTreeFile *genomeRangeTreeFileNew(struct genomeRangeTree *tree, char *fileName)
/* Create a genomeRangeTreeFile to save a genomeRangeTree in 'fileName'. 
 * Opens the file, does not write the header.
 * To write the header data only use: genomeRangeTreeFileWriteHeader().
 * To write the data portion only use: genomeRangeTreeFileWriteData(). 
 * If fileName is null, does not open any file. */
{
struct genomeRangeTreeFile *tf;
AllocVar(tf);
if (fileName)
    {
    tf->name = cloneString(fileName);
    tf->file =  mustOpen(fileName, "wb");
    }
tf->tree = tree;
tf->sig = genomeRangeTreeSig;
tf->version = 0;
tf->numChroms = tree->hash->elCount;
tf->valDataSize = 0; /* zero for no data, (later: -ve for variable size, +ve specifies fixed size */
tf->valDataType = 0; /* unused (later: signature for type ) */
tf->reserved1 = 0;
tf->reserved2 = 0;
/* Figure out location of first byte past header (location of the index). */
tf->headerLen = sizeof(tf->sig) + sizeof(tf->version) + sizeof(tf->headerLen) + sizeof(tf->numChroms)
    + sizeof(tf->valDataSize) + sizeof(tf->valDataType) + sizeof(tf->reserved1) + sizeof(tf->reserved2);

bits32 offset = 0;
bits32 nodes = 0;
long long counter = 0; /* check for 32 bit overflow */
/* get a sorted list of all chroms */
tf->chromList = hashElListHash(tree->hash);
slSort(&(tf->chromList), hashElCmp); /* strcmp() sort */

/* Figure out location of first byte past index (location of the data).
 * Each index entry contains 4+4 bytes of offset information
 * and the size and name of the sequence, which is variable length. */
struct hashEl *chrom;
offset = tf->headerLen;
for ( chrom = tf->chromList ; chrom ; chrom = chrom->next )
    {
    int nameLen = strlen(chrom->name);
    if (nameLen > 255)
        errAbort("name %s too long", chrom->name);
    /* index is list of triples: (name nodes offset) */
    offset += nameLen + 1 + sizeof(nodes) + sizeof(offset);
    }

/* Set up index. */ 
tf->nodes = newHash(0);
tf->offset = newHash(0);
for ( chrom = tf->chromList ; chrom ; chrom = chrom->next )
    {
    int size = rangeTreeSizeInFile(genomeRangeTreeFindRangeTree(tree, chrom->name));
    hashAddInt(tf->nodes, chrom->name, genomeRangeTreeFindRangeTree(tree, chrom->name)->n);
    hashAddInt(tf->offset, chrom->name, offset);
    offset += size;
    counter += (long long)size;
    if (counter > UINT_MAX )
        errAbort("Error in %s, index overflow at %s. The genomeRangeTree format "
                "does not support trees larger than %dGb, \n"
                "please split up into smaller files.\n", __FILE__, 
                chrom->name, UINT_MAX/1000000000);
    }

return tf;
}

struct genomeRangeTreeFile *genomeRangeTreeFileReadHeader(char *fileName)
/* Creates a genomeRangeTreeFile to read a genomeRangeTree from 'fileName'.
 * Opens the file, reads in header and index. 
 * Leaves file handle open at begining of data portion.
 * Returns a genomeRangeTreeFile containing file handle and index into contents.
 * To read genomeRangeTree data use: genomeRangeTreeFileReadData().
 * To return genomeRangeTree and close file and index use: genomeRangeTreeFileFree()
 * Squawk and die if there is a problem. */
{
struct genomeRangeTreeFile *f;
int i;

AllocVar(f);
f->name = cloneString(fileName);
f->file = mustOpen(fileName, "rb");

/* Allocate header verify signature, and read in
 * the constant-length bits. */
f->tree = genomeRangeTreeNew();
mustReadOne(f->file, f->sig);
if (f->sig == genomeRangeTreeSwapSig)
    f->isSwapped = TRUE;
else if (f->sig != genomeRangeTreeSig)
    errAbort("%s doesn't have a valid genomeRangeTreeSig", fileName);
f->version = readBits32(f->file, f->isSwapped);
if (f->version != 0)
    {
    errAbort("Can only handle version 0 of this file. This is version %d", (int)f->version);
    }
f->headerLen = readBits32(f->file, f->isSwapped);
f->numChroms = readBits32(f->file, f->isSwapped);
f->valDataSize = readBits32(f->file, f->isSwapped);
f->valDataType = readBits32(f->file, f->isSwapped);
f->reserved1 = readBits32(f->file, f->isSwapped);
f->reserved2 = readBits32(f->file, f->isSwapped);
if (f->valDataSize != 0)
    errAbort("Can only handle valDataSize of 0. This has %d.\n", (int)f->valDataSize);
if (f->valDataType != 0)
    errAbort("Can only handle valDataType of 0. This has %d.\n", (int)f->valDataType);

/* Read in index. Set up chromList, nodes, and offset fields. */
f->nodes = newHash(0);
f->offset = newHash(0);
for (i=0; i < f->numChroms ; ++i)
    {
    char name[256];
    if (!fastReadString(f->file, name))
        errAbort("%s is truncated", fileName);
    genomeRangeTreeFindOrAddRangeTree(f->tree, name); /* add empty tree for chrom */
    hashAddInt(f->nodes,  name, readBits32(f->file, f->isSwapped)); /* node count */
    hashAddInt(f->offset, name, readBits32(f->file, f->isSwapped)); /* data offset */
    }
f->chromList = hashElListHash(f->tree->hash);
slSort(&(f->chromList), hashElCmp); /* strcmp() sort */
return f;
}

struct genomeRangeTree *genomeRangeTreeFileReadData(struct genomeRangeTreeFile *f)
/* Read in the genomeRangeTree data for each chromosome and
 * return the genomeRangeTree.
 * File handle is left open pointing at the end of the file.
 * To close and free the genomeRangeTreeFile use: genomeRangeTreeFileFree().
 * Squawk and die if there is a problem. */
{
struct hashEl *chrom;

/* Read in nodes one tree at a time */
for (chrom = f->chromList ; chrom ; chrom = chrom->next)
    {
    rangeTreeReadNodes(f->file, genomeRangeTreeFindOrAddRangeTree(f->tree, chrom->name), hashIntVal(f->nodes, chrom->name), f->isSwapped);
    }
return f->tree;
}


static void genomeRangeTreeFileWriteHeaderDetailed(struct genomeRangeTreeFile *f, boolean zeroSig)
/* Write out genomeRangeTree header including: 
 *  header portion
 *  index of chromosomes.
 * If zeroSig is TRUE then write a zero in the sig field. 
 *   This stops any other programs reading the header until the file is ready.
 *   This is to support operations such as genomeRangeTreeFileOr() 
 *   which need to go back and update the header with node and offset information 
 *   after the data section is written. On the second header write the sig field
 *   can be written correctly so that other programs can then read the completed file.
 * To close the file use: genomeRangeTreeFileFree(). */
{
struct hashEl *chrom;
bits32 zero = 0;
/* Write out fixed parts of header. */
if (zeroSig)
    writeOne(f->file, zero);
else
    writeOne(f->file, f->sig);
writeOne(f->file, f->version);
writeOne(f->file, f->headerLen);
writeOne(f->file, f->numChroms);
writeOne(f->file, f->valDataSize);
writeOne(f->file, f->valDataType);
writeOne(f->file, f->reserved1);
writeOne(f->file, f->reserved2);
/* Write out index. */
for ( chrom = f->chromList; chrom ; chrom = chrom->next )
    {
    bits32 nodes = hashIntVal(f->nodes, chrom->name);
    bits32 offset = hashIntVal(f->offset, chrom->name);
    writeString(f->file, chrom->name);
    writeOne(f->file, nodes);
    writeOne(f->file, offset);
    }
}

void genomeRangeTreeFileWriteHeader(struct genomeRangeTreeFile *f)
/* Write out genomeRangeTree header including: 
 *  header portion
 *  index of chromosomes.
 * To close the file use: genomeRangeTreeFileFree(). */
{
genomeRangeTreeFileWriteHeaderDetailed(f, FALSE);
}

void genomeRangeTreeFileWriteData(struct genomeRangeTreeFile *f)
/* Write out all genomeRangeTree data. */
{
struct hashEl *chrom;
for ( chrom = f->chromList ; chrom ; chrom = chrom->next)
    {
    rangeTreeWriteNodes(genomeRangeTreeFindRangeTree(f->tree, chrom->name), f->file);
    }
}

struct genomeRangeTree *genomeRangeTreeFileFree(struct genomeRangeTreeFile **pFile)
/* Free up the resources associated with a genomeRangeTreeFile.
 * Close the file.
 * Return the genomeRangeTree. */
{
struct genomeRangeTreeFile *f = *pFile;
struct genomeRangeTree *tree = f->tree;
freez(&(f->name));
if (f->file)
    carefulClose(&(f->file));
hashElFreeList(&(f->chromList));
hashFree(&(f->nodes));
hashFree(&(f->offset));
freez(pFile);
return tree;
}

void genomeRangeTreeFileUnionDetailed(struct genomeRangeTreeFile *tf1, struct genomeRangeTreeFile *tf2, char *outFile, int *numChroms, int *nodes, unsigned *size, boolean saveMem, boolean orDirectToFile)
/* Create union of two saved genomeRangeTrees through a linear file scan.
 * Writes resulting genomeRangeTree to outFile. 
 * The resulting file cannot be safely read until the operation is complete. The header
 * information at the beginning of the file has to be updated after all the data is written
 * since the number of nodes in the final merged rangeTree is not known until the ranges are merged.
 * To enforce this, the header is written with a zero initial 'sig' field so that it cannot
 * be read as a genomeRangeTree file. The header information and 'sig' is re-written with 
 * correct data at the end of the process via an 'fseek' operation to the beginning of the file. 
 * If outFile is null, does not output the file. 
 * The number of nodes in the resulting tree is returned in n.
 * If size is not NULL, this will return the total size of the resulting ranges (adds 'n' 
 * calculations to run time of program). */
{
struct genomeRangeTree *tree = genomeRangeTreeNew();
struct hashEl *c1, *c2;
struct genomeRangeTreeFile *tf;
int res, nodes1, nodes2, i=0;
if (size)
    *size = 0;
/* Add all chroms to the tree and write the header */
for (c1 = tf1->chromList ; c1 ; c1=c1->next)
    {
    genomeRangeTreeFindOrAddRangeTree(tree, c1->name);
    }
for (c2 = tf2->chromList ; c2 ; c2=c2->next)
    {
    genomeRangeTreeFindOrAddRangeTree(tree, c2->name);
    }

/* write the header with a zero sig so that noone 
 * else can read this file until its ready (the header
 * will need to be re-written at the end to update the
 * chrom node and offset information. */
tf = genomeRangeTreeFileNew(tree, outFile);
if (tf->file)
    genomeRangeTreeFileWriteHeaderDetailed(tf, TRUE); 

/* loop through the chromosomes merging the rangeTrees */
c1 = tf1->chromList;
c2 = tf2->chromList;
while (c1 && c2) /* at least one chrom in each tree */
    {
    res = strcmp(c1->name, c2->name);
    nodes1 = hashIntVal(tf1->nodes, c1->name);
    nodes2 = hashIntVal(tf2->nodes, c2->name);
    if (res < 0) /* chrom1 < chrom2 so write out chrom1 directly */
	{
	struct rangeStartSize *r1;
	hashAddInt(tf->nodes, c1->name, nodes1);
	AllocArray(r1, nodes1);
	rangeReadArray(tf1->file, r1, nodes1, tf1->isSwapped);
	if (tf->file)
		rangeWriteArray(r1, nodes1, tf->file);
	if (size)
		*size += rangeArraySize(r1, nodes1);
	freez(&r1);
	i += nodes1;
	c1 = c1->next; 
	}
    else if (res > 0) /* chrom2 < chrom1 so write out chrom2 directly */
	{
	struct rangeStartSize *r2;
	hashAddInt(tf->nodes, c2->name, nodes2);
	AllocArray(r2, nodes2);
	rangeReadArray(tf2->file, r2, nodes2, tf2->isSwapped);
	if (tf->file)
		rangeWriteArray(r2, nodes2, tf->file);
	if (size)
		*size += rangeArraySize(r2, nodes2);
	freez(&r2);
	i += nodes2;
	c2 = c2->next;
	}
    else /* res == 0 so both chroms the same and need to merge */
	{
	struct rangeStartSize *r1, *r2;
	AllocArray(r1, nodes1);
	AllocArray(r2, nodes2);
	rangeReadArray(tf1->file, r1, nodes1, tf1->isSwapped);
	rangeReadArray(tf2->file, r2, nodes2, tf2->isSwapped);
	/* merge and store the total nodes after merging */
	int n0=0;
	unsigned s0=0;
	if (orDirectToFile)
	    s0 = rangeArrayUnionToFile(r1, r2, nodes1, nodes2, tf->file, &n0);
	else
	    {
	    struct rangeStartSize *r=NULL;
	    s0 = rangeArrayUnion(r1, r2, nodes1, nodes2, &r, saveMem, NULL, NULL, NULL, &n0);
	    if (tf->file && n0 > 0)
		{
		rangeWriteArray(r, n0, tf->file);
		}
	    freez(&r);
	    }
	if (size)
	    (*size) += s0;
	i += n0;
	hashAddInt(tf->nodes, c1->name, n0);
	freez(&r1);
	freez(&r2);
	c1 = c1->next;
	c2 = c2->next;
	}
    }

/* directly write out any remaining chromosomes, either c1 or c2 */
while (c1)
    {
    struct rangeStartSize *r1;
    nodes1 = hashIntVal(tf1->nodes, c1->name);
    hashAddInt(tf->nodes, c1->name, nodes1);
    AllocArray(r1, nodes1);
    rangeReadArray(tf1->file, r1, nodes1, tf1->isSwapped);
    if (tf->file)
	rangeWriteArray(r1, nodes1, tf->file);
    if (size)
	*size += rangeArraySize(r1, nodes1);
    i += nodes1;
    freez(&r1);
    c1 = c1->next;
    }
while (c2)
    {
    struct rangeStartSize *r2;
    nodes2 = hashIntVal(tf2->nodes, c2->name);
    hashAddInt(tf->nodes, c2->name, nodes2);
    AllocArray(r2, nodes2);
    rangeReadArray(tf2->file, r2, nodes2, tf2->isSwapped);
    if (tf->file)
	rangeWriteArray(r2, nodes2, tf->file);
    if (size)
	*size += rangeArraySize(r2, nodes2);
    i += nodes2;
    freez(&r2);
    c2 = c2->next;
    }
/* Re-calculate the offset for each chrom.
 * We could skip the first loop (code duplication in genomeRangeTreeFileNew) 
 * if we stored the indexLen in the header as well.
 * We would then only have to recalculate the offsets for each chrom. */
bits32 offset = tf->headerLen;
struct hashEl *chrom;
for (chrom = tf->chromList ; chrom ; chrom = chrom->next )
    {
    int nameLen = strlen(chrom->name);
    offset += nameLen + 1 + sizeof(offset) + sizeof(offset); /* name + nodes + offset */
    }
for (chrom = tf->chromList ; chrom ; chrom = chrom->next )
    {
    int size = 2*sizeof(bits32)*hashIntVal(tf->nodes, chrom->name);
    hashAddInt(tf->offset, chrom->name, offset);
    offset += size;
    }
/* Now go back and rewrite the header, and close the file */
if (tf->file && fseek(tf->file, 0, SEEK_SET))
    errAbort("couldnt fseek(f,0,SEEK_SET) to rewrite header: errno(%d) = %s\n", errno, strerror(errno));
if (tf->file)
    genomeRangeTreeFileWriteHeader(tf);
*nodes = i;
*numChroms = tf->numChroms;
genomeRangeTreeFileFree(&tf);
}

void genomeRangeTreeFileUnion(struct genomeRangeTreeFile *tf1, struct genomeRangeTreeFile *tf2, char *outFile)
/* Combine two saved genomeRangeTrees in a logical 'or' through a linear file scan.
 * Writes resulting genomeRangeTree to outFile. 
 * The resulting file cannot be safely read until the operation is complete. The header
 * information at the beginning of the file has to be updated after all the data is written
 * since the final merged rangeTree sizes are not known until the ranges are merged.
 * To enforce this, the header is written with a zero initial 'sig' field so that it cannot
 * be read as a genomeRangeTree file. The header information and 'sig' is re-written with 
 * correct data at the end of the process via an 'fseek' operation to the beginning of the file. 
 * If outFile is null, does not output the file. */
{
int nodes, numChroms;
genomeRangeTreeFileUnionDetailed(tf1, tf2, outFile, &numChroms, &nodes, NULL, FALSE, FALSE);
}

void genomeRangeTreeFileIntersectionDetailed(struct genomeRangeTreeFile *tf1, struct genomeRangeTreeFile *tf2, char *outFile, int *numChroms, int *nodes, unsigned *size, boolean saveMem)
/* Create intersection genomeRangeTree from two saved genomeRangeTrees in a logical 'and' through a linear file scan.
 * Writes resulting genomeRangeTree to outFile if outFile is non-null.
 * The resulting file cannot be safely read until the operation is complete. The header
 * information at the beginning of the file has to be updated after all the data is written
 * since the final merged rangeTree sizes are not known until the ranges are merged.
 * To enforce this, the header is written with a zero initial 'sig' field so that it cannot
 * be read as a genomeRangeTree file. The header information and 'sig' is re-written with 
 * correct data at the end of the process via an 'fseek' operation to the beginning of the file. */
{
struct genomeRangeTree *tree = genomeRangeTreeNew();
struct hashEl *c1, *c2;
struct hash *ranges = hashNew(0), *nodeCounts = hashNew(0);
struct genomeRangeTreeFile *tf;
int res, nodes1, nodes2, i = 0;

if (size)
    *size = 0;
/* loop through the chromosomes merging the rangeTrees */
c1 = tf1->chromList;
c2 = tf2->chromList;
while (c1 && c2) /* at least one chrom in each tree */
    {
    res = strcmp(c1->name, c2->name);
    nodes1 = hashIntVal(tf1->nodes, c1->name);
    nodes2 = hashIntVal(tf2->nodes, c2->name);
    if (res < 0) /* chrom1 < chrom2 so skip chrom1 */
	{
	c1 = c1->next; 
	if (c1 && fseek(tf1->file, hashIntVal(tf1->offset, c1->name), SEEK_SET) != 0)
	    errnoAbort("could not seek to pos %d in chrom %s in file %s\n", 
		hashIntVal(tf1->offset, c1->name), c1->name, tf1->name);
	//struct rangeStartSize *r1;
	//AllocArray(r1, nodes1);
	//rangeReadArray(tf1->file, r1, nodes1, tf1->isSwapped);
	//freez(&r1);
	}
    else if (res > 0) /* chrom2 < chrom1 so skip chrom2 */
	{
	c2 = c2->next;
	if (c2 && fseek(tf2->file, hashIntVal(tf2->offset, c2->name), SEEK_SET) != 0 )
	    errAbort("could not seek to pos %d in chrom %s in file %s : error(%d) %s\n", 
		hashIntVal(tf2->offset, c2->name), c2->name, tf2->name, errno, strerror(errno));
	//struct rangeStartSize *r2; /* Need to read this chrom to move the file pointer */
	//AllocArray(r2, nodes2);
	//rangeReadArray(tf2->file, r2, nodes2, tf2->isSwapped);
	//freez(&r2);
	}
    else /* res == 0 so both chroms the same and need to merge */
	{
	struct rangeStartSize *r1, *r2;
	AllocArray(r1, nodes1);
	AllocArray(r2, nodes2);
	rangeReadArray(tf1->file, r1, nodes1, tf1->isSwapped);
	rangeReadArray(tf2->file, r2, nodes2, tf2->isSwapped);
	/* store the intersected nodes */
	unsigned s0;
	int n0=0;
	struct rangeStartSize *r = NULL;
	s0 = rangeArrayIntersection(r1, r2, nodes1, nodes2, &r, saveMem, NULL, NULL, NULL, &n0);
	/* if non-null intersection then remember chrom, array of ranges and number of nodes */
	if (n0>0)
	    {
	    genomeRangeTreeFindOrAddRangeTree(tree, c1->name);
	    hashAddUnique(ranges, c1->name, r);
	    hashAddInt(nodeCounts, c1->name, n0);
	    if (size)
		*size += s0;
	    i += n0;
	    }
	freez(&r1);
	freez(&r2);
	c1 = c1->next;
	c2 = c2->next;
	}
    }
/* write header with chroms */
tf = genomeRangeTreeFileNew(tree, outFile);
if (tf->file)
    genomeRangeTreeFileWriteHeaderDetailed(tf, TRUE); 
freeHash(&(tf->nodes));
tf->nodes = nodeCounts;

/* Re-calculate the offset for each chrom.
 * We could skip the first loop (code duplication in genomeRangeTreeFileNew) 
 * if we stored the indexLen in the header as well.
 * We would then only have to recalculate the offsets for each chrom. */
bits32 offset = tf->headerLen;
struct hashEl *chrom;
for (chrom = tf->chromList ; chrom ; chrom = chrom->next )
    {
    int nameLen = strlen(chrom->name);
    offset += nameLen + 1 + sizeof(offset) + sizeof(offset); /* name + nodes + offset */
    }
for (chrom = tf->chromList ; chrom ; chrom = chrom->next )
    {
    int size = 2*sizeof(bits32)*hashIntVal(tf->nodes, chrom->name);
    hashAddInt(tf->offset, chrom->name, offset);
    offset += size;
    }
/* Now go back and rewrite the header, data, and close the file */
if (tf->file)
    {
    if (fseek(tf->file, 0, SEEK_SET))
	errAbort("couldnt fseek(f,0,SEEK_SET) to rewrite header: errno(%d) = %s\n", errno, strerror(errno));
    genomeRangeTreeFileWriteHeader(tf);
    for (chrom = tf->chromList ; chrom ; chrom = chrom->next )
	{
	struct rangeStartSize *r = hashMustFindVal(ranges, chrom->name);
	rangeWriteArray(r, hashIntVal(tf->nodes, chrom->name), tf->file);
	freez(&r);
	}
    }
*nodes = i;
*numChroms = tf->numChroms;
genomeRangeTreeFileFree(&tf);
}

void genomeRangeTreeFileStats(char *fileName, int *numChroms, int *nodes, int *size)
/* Calculates the number of chroms, ranges, and total size of ranges in the genomeRangeTree file.
 * Performs a linear scan of the file. */
{
struct genomeRangeTreeFile *tf = genomeRangeTreeFileReadHeader(fileName);
struct hashEl *c;
int n;
*size = 0;
*nodes = 0;
*numChroms = tf->numChroms;
for (c = tf->chromList ; c ; c = c->next)
    {
    struct rangeStartSize *r;
    n = hashIntVal(tf->nodes, c->name);
    *nodes += n;
    AllocArray(r, n);
    rangeReadArray(tf->file, r, n, tf->isSwapped);
    *size += rangeArraySize(r, n);
    freez(&r);
    }
genomeRangeTreeFileFree(&tf);
}

int genomeRangeTreeFileChromSeek(struct genomeRangeTreeFile *tf, char *chrom)
/* Seek the tree file to the start of the chromosome data.
 * Returns the number of nodes in the chromosome. 
 * If this chromosome is not in this tree, does not do a seek
 * and returns 0 for number of nodes.
 */
{
if (!tf->file)
    errAbort("no file handle for genomeRangeTree chrom seek\n");
if (!chrom)
    errAbort("no chrom for genomeRangeTree chrom seek\n");
if (!hashLookup(tf->tree->hash, chrom))
    return 0; /* chrom not in the tree */
if (fseek(tf->file, hashIntVal(tf->offset, chrom), SEEK_SET) != 0)
    errnoAbort("could not seek to pos %d in chrom %s in file %s\n", 
	hashIntVal(tf->offset, chrom), chrom, tf->name);
return hashIntVal(tf->nodes, chrom);
}

void genomeRangeTreeFileWriteToBed(char *inBama, char *bedFile, boolean withId, boolean mergeAdjacent)
/* Write a genomeRangeTreeFile directly to a bed file.
 * If withId then unique identifier is added to the name field.
 * If mergeAdjacent then adjacent ranges, which would otherwise appear on individual lines,
 * are merged into a single bed line. */
{
struct genomeRangeTreeFile *tf = genomeRangeTreeFileReadHeader(inBama);
FILE *f = mustOpen(bedFile,"w");
struct hashEl *chrom;
for (chrom = tf->chromList ; chrom ; chrom = chrom->next)
    {
    struct rangeStartSize *r;
    int n = hashIntVal(tf->nodes, chrom->name);
    AllocArray(r, n);
    rangeReadArray(tf->file, r, n, tf->isSwapped);
    rangeWriteArrayToBed(chrom->name, r, n, withId, mergeAdjacent, f);
    freez(&r);
    }
carefulClose(&f);
genomeRangeTreeFileFree(&tf);
}

