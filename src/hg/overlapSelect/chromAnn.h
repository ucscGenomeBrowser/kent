/* chromAnn - chomosome annotations, generic object to store annotations from
 * other formats */
#ifndef CHROMANN_H
#define CHROMANN_H
struct rowReader;
struct coordCols;

enum chromAnnOpts
/* bit set of options */ 
{
    chromAnnCds        = 0x01,  /* use only CDS in blocks */
    chromAnnRange      = 0x02,  /* save entire range, not just blocks */
    chromAnnSaveLines  = 0x04   /* save records as lines os they can be
                                 * outputted latter */
};

struct chromAnn
/* chomosome annotations, generic object to store annotations from other
 * formats */
{
    struct chromAnn *next;
    char* name;      /* optional name of the item */
    char* chrom;     /* object coordinates */
    char strand;
    int start;       /* start of first block */
    int end;         /* end of last block */
    int totalSize;   /* size of all blocks */
    char **rawCols;  /* optional columns of original record, NULL terminated,
                      * single malloc block,  */
    struct chromAnnBlk *blocks;  /* ranges associated with this object */
};

struct chromAnnBlk
/* specifies a range to select */
{
    struct chromAnnBlk *next;
    struct chromAnn *ca;  /* link back to chromAnn */
    int start;            /* block coordinates */
    int end;
};

void chromAnnFree(struct chromAnn **caPtr);
/* free an object */

void chromAnnWrite(struct chromAnn* ca, FILE *fh, char term);
/* write tab separated row using rawCols */

struct chromAnn* chromAnnFromBed(unsigned opts, struct rowReader *rr);
/* create a chromAnn object from a row read from a BED file or table */

struct chromAnn* chromAnnFromGenePred(unsigned opts, struct rowReader *rr);
/* create a chromAnn object from a row read from a GenePred file or table.  If
 * there is no CDS, and chromAnnCds is specified, it will return a record with
 * zero-length range.*/

struct chromAnn* chromAnnFromPsl(unsigned opts, struct rowReader *rr);
/* create a chromAnn object from a row read from a psl file or table */

struct chromAnn* chromAnnFromCoordCols(unsigned opts, struct coordCols* cols,
                                       struct rowReader *rr);
/* create a chromAnn object from a line read from a tab file or table with
 * coordiates at a specified columns */

#endif
