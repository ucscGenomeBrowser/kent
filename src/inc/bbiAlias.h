#ifndef BBIALIAS_H
#define BBIALIAS_H

struct bptIndex   // A structure that contains the field number and the open index of an external index
{
struct bptIndex *next;
int fieldIx;
struct bptFile *bpt;
};

struct slName *bbiAliasFindAliases(struct bbiFile *bbi, struct lm *lm, char *seqName);
/* Find the aliases for a given seqName using an alias bigBed. */

char *bbiAliasFindNative(struct bbiFile *bbi, struct bptIndex *bptIndex, struct lm *lm,  char *alias);
/* Find the native seqName for a given alias given an alias bigBed. */

struct bptIndex *bbiAliasOpenExtra(struct bbiFile *bbi);
/* Open any extra indices that this bigBed has. */

unsigned bbiAliasChromSize(struct bbiFile *bbi, struct bptIndex *bptIndex, struct lm *lm, char *chrom);
/* Find the size of the given chrom in the given chromAlias bbi file.  */

unsigned bbiAliasChromSizeExt(struct bbiFile *bbi, struct bptIndex *bptIndex, struct lm *lm, char *chrom, struct hash *usedHash, int lineIx);
/* Find the size of the given chrom in the given chromAlias bbi file. If this alias has been used before, complain and exit. */

#endif /* BBIALIAS_H */
