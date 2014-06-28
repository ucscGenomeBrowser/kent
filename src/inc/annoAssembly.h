/* annoAssembly -- basic metadata about an assembly for the annoGrator framework. */

#ifndef ANNOASSEMBLY_H
#define ANNOASSEMBLY_H

#include "common.h"
#include "hash.h"

struct annoAssembly
/* Basic information about a genome assembly. */
    {
    char *name;			// UCSC symbolic name for assembly, e.g. "hg19"
    struct twoBitFile *tbf;	// Opened twoBit sequence file for assembly
    char *twoBitPath;		// twoBit file name
    struct hash *seqSizes;	// cache of sequence names to sizes (twoBitSeqSize does a seek&read)
    struct dnaSeq *curSeq;	// cache of most recently accessed sequence (chrom/scaffold)
    };

struct annoAssembly *annoAssemblyNew(char *name, char *twoBitPath);
/* Return an annoAssembly with open twoBitFile. */

struct slName *annoAssemblySeqNames(struct annoAssembly *aa);
/* Return a list of sequence names in this assembly. */

uint annoAssemblySeqSize(struct annoAssembly *aa, char *seqName);
/* Return the number of bases in seq which must be in aa's twoBitFile. */

void annoAssemblyGetSeq(struct annoAssembly *aa, char *seqName, uint start, uint end,
			char *buf, size_t bufSize);
/* Copy sequence to buf; bufSize must be at least end-start+1 chars in length. */

void annoAssemblyClose(struct annoAssembly **pAa);
/* Close aa's twoBitFile and free mem. */

#endif//ndef ANNOASSEMBLY_H
