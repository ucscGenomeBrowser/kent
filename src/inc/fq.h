/* fq - stuff for doing i/o on fastq files. */

struct fq3
/* Representation of record as 3 lines of text, not further parsed.  We omit the 3 of 4 lines in the
 * fastq record, since it is always just '+'. */
    {
    struct fq3 *next;
    char *atLine;   // This line starts with @ and contains the sequence name plus other stuff
    char *seqLine;  // ACGT and on occassional N normally
    char *qualLine; // Might be 33-based (Sanger) or 64 based (Solexa)
    };

struct fq3 *fq3ReadNext(struct lineFile *lf);
/* Read next record, return it as fq3. */
