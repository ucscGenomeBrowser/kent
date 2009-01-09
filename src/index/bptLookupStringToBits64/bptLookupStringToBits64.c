/* bptLookupStringToBits64 - Given a string value look up and return associated 64 bit value if 
 * any. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sig.h"

static char const rcsid[] = "$Id: bptLookupStringToBits64.c,v 1.1 2009/01/09 02:59:24 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bptLookupStringToBits64 - Given a string value look up and return associated 64 bit value if\n"
  "any.\n"
  "usage:\n"
  "   bptLookupStringToBits64 index.bpt string\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct bptFile
/* B+ tree index file handle. */
    {
    struct bptFile *next;	/* Next in list of index files if any. */
    char *fileName;		/* Name of file - for error reporting. */
    FILE *f;			/* Open file pointer. */
    bits32 blockSize;		/* Size of block. */
    bits32 keySize;		/* Size of keys. */
    bits32 valSize;		/* Size of values. */
    bits64 itemCount;		/* Number of items indexed. */
    boolean isSwapped;		/* If TRUE need to byte swap everything. */
    bits64 rootOffset;		/* Offset of root block. */
    };

struct bptFile *bptFileOpen(char *fileName)
/* Open up index file - reading header and verifying things. */
{
/* Open file and allocate structure to hold info from header etc. */
struct bptFile *bpt = needMem(sizeof(*bpt));
bpt->fileName = cloneString(fileName);
FILE *f = mustOpen(fileName, "rb");
bpt->f = f;

/* Read magic number at head of file and use it to see if we are proper file type, and
 * see if we are byte-swapped. */
bits32 magic;
boolean isSwapped = FALSE;
mustReadOne(f, magic);
if (magic != bptSig)
    {
    magic = byteSwap32(magic);
    isSwapped = bpt->isSwapped = TRUE;
    if (magic != bptSig)
       errAbort("%s is not a bpt b-plus tree index file", fileName);
    }

/* Read rest of defined bits of header, byte swapping as needed. */
bpt->blockSize = readBits32(f, isSwapped);
bpt->keySize = readBits32(f, isSwapped);
bpt->valSize = readBits32(f, isSwapped);
bpt->itemCount = readBits64(f, isSwapped);

/* Skip over reserved bits of header. */
bits32 reserved32;
mustReadOne(f, reserved32);
mustReadOne(f, reserved32);

/* Save position of root block of b+ tree. */
bpt->rootOffset = ftell(f);

return bpt;
}

void bptFileClose(struct bptFile **pBpt)
/* Close down and deallocate index file. */
{
struct bptFile *bpt = *pBpt;
if (bpt != NULL)
    {
    carefulClose(&bpt->f);
    freeMem(bpt->fileName);
    freez(&bpt);
    }
}

static boolean rFind(struct bptFile *bpt, bits64 blockStart, void *key, void *val)
/* Find value corresponding to key.  If found copy value to memory pointed to by val and return 
 * true. Otherwise return false. */
{
/* Seek to start of block. */
fseek(bpt->f, blockStart, SEEK_SET);

/* Read block header. */
UBYTE isLeaf;
UBYTE reserved;
bits16 i, childCount;
mustReadOne(bpt->f, isLeaf);
mustReadOne(bpt->f, reserved);
boolean isSwapped = bpt->isSwapped;
childCount = readBits16(bpt->f, isSwapped);

UBYTE keyBuf[bpt->keySize];   /* Place to put a key, buffered on stack. */

if (isLeaf)
    {
    for (i=0; i<childCount; ++i)
        {
	mustRead(bpt->f, keyBuf, bpt->keySize);
	mustRead(bpt->f, val, bpt->valSize);
	if (memcmp(key, keyBuf, bpt->keySize) == 0)
	    return TRUE;
	}
    return FALSE;
    }
else
    {
    /* Read and discard first key. */
    mustRead(bpt->f, keyBuf, bpt->keySize);

    /* Scan info for first file offset. */
    bits64 fileOffset = readBits64(bpt->f, isSwapped);

    /* Loop through remainder. */
    for (i=1; i<childCount; ++i)
	{
	mustRead(bpt->f, keyBuf, bpt->keySize);
	if (memcmp(key, keyBuf, bpt->keySize) < 0)
	    break;
	fileOffset = readBits64(bpt->f, isSwapped);
	}
    return rFind(bpt, fileOffset, key, val);
    }
}

static boolean bptFileFind(struct bptFile *bpt, void *key, int keySize, void *val, int valSize)
/* Find value associated with key.  Return TRUE if it's found. 
*  Parameters:
*     bpt - file handle returned by bptFileOpen
*     key - pointer to key string, which needs to be bpt->keySize long
*     val - pointer to where to put retrieved value
*/
{
/* Check key size vs. file key size, and act appropriately.  If need be copy key to a local
 * buffer and zero-extend it. */
if (keySize > bpt->keySize)
    return FALSE;
char keyBuf[keySize];
if (keySize != bpt->keySize)
    {
    memcpy(keyBuf, key, keySize);
    memset(keyBuf+keySize, 0, bpt->keySize - keySize);
    key = keyBuf;
    }

/* Make sure the valSize matches what's in file. */
if (valSize != bpt->valSize)
    errAbort("Value size mismatch between bptFileFind (valSize=%d) and %s (valSize=%d)",
    	valSize, bpt->fileName, bpt->valSize);

/* Call recursive finder. */
return rFind(bpt, bpt->rootOffset, key, val);
}


void bptLookupStringToBits64(char *indexFile, char *string)
/* bptLookupStringToBits64 - Given a string value look up and return associated 64 bit value if 
 * any */
{
struct bptFile *bpt = bptFileOpen(indexFile);
bits64 val;
if (bptFileFind(bpt, string, strlen(string), &val, sizeof(val)) )
    printf("%lld\n", val);
else
    errAbort("%s not found", string);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
bptLookupStringToBits64(argv[1], argv[2]);
return 0;
}
