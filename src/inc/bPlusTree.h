/* bptFile - B+ Trees.  These are a method of indexing data similar to binary trees, but 
 * with many children rather than just two at each node. They work well when stored on disk,
 * since typically only two or three disk accesses are needed to locate any particular
 * piece of data.  This implementation is *just* for disk based storage.  For memory
 * use the rbTree instead. Currently the implementation is just useful for data warehouse
 * type applications.  That is it implements a function to create a b+ tree from bulk data
 * (bptFileCreate) and a function to lookup a value given a key (bptFileFind) but not functions
 * to add or delete individual items.
 *
 *
 * The layout of the file on disk is:
 *    header
 *    root node
 *    (other nodes)
 * In general when the tree is first built the higher level nodes are stored before the
 * lower level nodes.  It is possible if a b+ tree is dynamically updated for this to
 * no longer be strictly true, but actually currently the b+ tree code here doesn't implement
 * dynamic updates - it just creates a b+ tree from a sorted list.
 *
 * Each node can be one of two types - index or leaf.  The index nodes contain pointers
 * to child nodes.  The leaf nodes contain the actual data. 
 *
 * The layout of the file header is:
 *       <magic number>  4 bytes - The value bptSig (0x78CA8C91)
 *       <block size>    4 bytes - Number of children per block (not byte size of block)
 *       <key size>      4 bytes - Number of significant bytes in key
 *       <val size>      4 bytes - Number of bytes in value
 *       <item count>    8 bytes - Number of items in index
 *       <reserved2>     4 bytes - Always 0 for now
 *       <reserved3>     4 bytes - Always 0 for now
 * The magic number may be byte-swapped, in which case all numbers in the file
 * need to be byte-swapped. 
 *
 * The nodes start with a header:
 *       <is leaf>       1 byte  - 1 for leaf nodes, 0 for index nodes.
 *       <reserved>      1 byte  - Always 0 for now.
 *       <count>         2 bytes - The number of children/items in node
 * This is followed by count items.  For the index nodes the items are
 *       <key>           key size bytes - always written most significant byte first
 *       <offset>        8 bytes - Offset of child node in index file.
 * For leaf nodes the items are
 *       <key>           key size bytes - always written most significant byte first
 *       <val>           val sized bytes - the value associated with the key.
 * Note in general the leaf nodes may not be the same size as the index nodes, though in
 * the important case where the values are file offsets they will be.
 */

#ifndef BPLUSTREE_H
#define BPLUSTREE_H

struct bptFile
/* B+ tree index file handle. */
    {
    struct bptFile *next;	/* Next in list of index files if any. */
    char *fileName;		/* Name of file - for error reporting. */
    struct udcFile *udc;			/* Open file pointer. */
    bits32 blockSize;		/* Size of block. */
    bits32 keySize;		/* Size of keys. */
    bits32 valSize;		/* Size of values. */
    bits64 itemCount;		/* Number of items indexed. */
    boolean isSwapped;		/* If TRUE need to byte swap everything. */
    bits64 rootOffset;		/* Offset of root block. */
    };

struct bptFile *bptFileOpen(char *fileName);
/* Open up index file - reading header and verifying things. */

void bptFileClose(struct bptFile **pBpt);
/* Close down and deallocate index file. */

struct bptFile *bptFileAttach(char *fileName, struct udcFile *udc);
/* Open up index file on previously open file, with header at current file position. */

void bptFileDetach(struct bptFile **pBpt);
/* Detach and free up bptFile opened with bptFileAttach. */

boolean bptFileFind(struct bptFile *bpt, void *key, int keySize, void *val, int valSize);
/* Find value associated with key.  Return TRUE if it's found. 
*  Parameters:
*     bpt - file handle returned by bptFileOpen
*     key - pointer to key string
*     keySize - size of key.  Normally just strlen(key)
*     val - pointer to where to put retrieved value
*     valSize - size of memory buffer that will hold val.  Should match bpt->valSize.
*/

void bptFileTraverse(struct bptFile *bpt, void *context,
    void (*callback)(void *context, void *key, int keySize, void *val, int valSize) );
/* Traverse bPlusTree on file, calling supplied callback function at each
 * leaf item. */


void bptFileCreate(
	void *itemArray, 	/* Sorted array of things to index. */
	int itemSize, 		/* Size of each element in array. */
	bits64 itemCount, 	/* Number of elements in array. */
	bits32 blockSize,	/* B+ tree block size - # of children for each node. */
	void (*fetchKey)(const void *va, char *keyBuf),  /* Given item, copy key to keyBuf */ 
	bits32 keySize,					 /* Size of key */
	void* (*fetchVal)(const void *va), 		 /* Given item, return pointer to value */
	bits32 valSize, 				 /* Size of value */
	char *fileName);                                 /* Name of output file. */
/* Create a b+ tree index file from a sorted array. */

void bptFileBulkIndexToOpenFile(void *itemArray, int itemSize, bits64 itemCount, bits32 blockSize,
	void (*fetchKey)(const void *va, char *keyBuf), bits32 keySize,
	void* (*fetchVal)(const void *va), bits32 valSize, FILE *f);
/* Create a b+ tree index from a sorted array, writing output starting at current position
 * of an already open file.  See bptFileCreate for explanation of parameters. */

#define bptFileHeaderSize 32
#define bptBlockHeaderSize 4

#endif /* BPLUSTREE_H */
