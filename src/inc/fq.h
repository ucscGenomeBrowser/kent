/* fq - stuff for doing i/o on fastq files. */

#ifndef FQ_H
#define FQ_H

struct fq
/* Representation of record as 3 lines of text, not further parsed.  We omit the 3 of 4 lines in the
 * fastq record, since it is always just '+'. */
    {
    struct fq *next;
    char *header;   // This line starts with @ and contains the sequence name plus other stuff
    char *dna;  // ACGT and on occassional N normally
    char *quality; // Might be 33-based (Sanger) or 64 based (Solexa)
    };

struct fq *fqReadNext(struct lineFile *lf);
/* Read next record, return it as fq3. */

void fqFree(struct fq **pFq);
/* Free up *pFq and set it to NULL */

#endif /* FQ_H */

