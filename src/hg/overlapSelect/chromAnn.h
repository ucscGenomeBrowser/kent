/* chromAnn - chomosome annotations, generic object to store annotations from
 * other formats */
#ifndef CHROMANN_H
#define CHROMANN_H

struct lineFile;
struct coordCols;

enum chromAnnOpts
/* bit set of options */ 
{
    chromAnnCds        = 0x01,  /* use only CDS in blocks */
    chromAnnSaveLines  = 0x02   /* save records as limnes os they can be
                                 * outputted latter */
};

struct chromAnn
/* chomosome annotations, generic object to store annotations from other
 * formats */
{
    struct chromAnn *next;
    char* name;     /* optional name of the item */
    char* chrom;    /* object coordinates */
    char strand;
    int start;      /* start of first block */
    int end;        /* end of last block */
    char *recLine;  /* optional record containing the original record */
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

struct chromAnn* chromAnnFromBed(unsigned opts, struct lineFile *lf, char *line);
/* create a chromAnn object from line read from a BED file */

struct chromAnn* chromAnnFromGenePred(unsigned opts, struct lineFile *lf, char *line);
/* create a chromAnn object from line read from a GenePred file */

struct chromAnn* chromAnnFromPsl(unsigned opts, struct lineFile *lf, char *line);
/* create a chromAnn object from line read from a psl file */

struct chromAnn* chromAnnFromCoordCols(unsigned opts, struct lineFile *lf, char *line, struct coordCols* cols);
/* create a chromAnn object from a line read from tab file with coordiates at
 * a specified columns */

#endif
