/*
 * Object for accessing GFF3 files
 * See GFF3 specification for details of file format:
 *   http://www.sequenceontology.org/gff3.shtml
 */
#ifndef gff3_h
#define gff3_h

struct gff3Ann
/* Annotation record from a GFF3 file.  Attributes define in the spec (those
 * starting with upper case letters) are parsed into fields of this
 * object. User defined attributes (starting with lower-case characters) are
 * stored as in a list, along with a copy of the string versions of the spec
 * attributes. All strings stored in the object have been un-escaped.
 * All storage for the object is allocated by the gff3File object.
 * For discontinuous features, there are multiple gff3Ann objects.
 * These objects are stored in a double-linked list, and all references
 * point to the first one in ascending start order.*/
{
    struct gff3Ann *prevPart; /* Discontinuous features have linked annotation */
    struct gff3Ann *nextPart; /* field name next not used to avoid confusion */
    char *seqid;   /* The ID of the landmark used to establish the coordinate
                    * system for the current feature. IDs may contain any
                    * characters. */
    char *source;  /* The source is a free text qualifier intended to describe
                    * the algorithm or operating procedure that generated this
                    * feature.  Typically this is the name of a piece of
                    * software, such as "Genescan" or a database name, such as
                    * "Genbank."  In effect, the source is used to extend the
                    * feature ontology by adding a qualifier to the type
                    * creating a new composite type that is a subclass of the
                    * type in the type column. */

    char *type; /* The type of the feature (previously called the "method").
                 * This is constrained to be either: (a) a term from the
                 * "lite" sequence ontology, SOFA; or (b) a SOFA accession
                 * number.  The latter alternative is distinguished using the
                 * syntax SO:000000. */

    int start; /* The start and end of the feature, in 0-based, half open
                * integer coordinates, relative to the landmark given in
                * seqid.  Start is always less than or equal to end.  For
                * zero-length features, such as insertion sites, start equals
                * end and the implied site is to the right of the indicated
                * base in the direction of the landmark.*/
    int end;
    float score; /* The score of the feature, a floating point number.  As in
                    earlier versions of the format, the semantics of the score
                    are ill-defined.  It is strongly recommended that E-values
                    be used for sequence similarity features, and that
                    P-values be used for ab initio gene prediction features. */
    boolean haveScore;  /* was score specified? */

    char *strand; /* The strand of the feature.  '+' for positive strand
                   * (relative to the landmark), '-' for minus strand, and
                   * NULL for features that are not stranded.  In addition,
                   * '?' can be used for features whose strandedness is
                   * relevant, but unknown. */

    int phase; /* For features of type "CDS", the phase indicates where the
                * feature begins with reference to the reading frame.  The
                * phase is one of the integers 0, 1, or 2, indicating the
                * number of bases that should be removed from the beginning of
                * this feature to reach the first base of the next codon. In
                * other words, a phase of 0 indicates that the next codon
                * begins at the first base of the region described by the
                * current line, a phase of 1 indicates that the next codon
                * begins at the second base of this region, and a phase of 2
                * indicates that the codon begins at the third base of this
                * region. This is NOT to be confused with the frame, which is
                * simply start modulo 3.  For forward strand features, phase
                * is counted from the start field. For reverse strand
                * features, phase is counted from the end field. The phase is
                * REQUIRED for all CDS features. and -1 for other features. */

    /* The remaining fields are the attributes.  Attributes defined by the
     * GFF3 spec (starting with an upper-case letter) are stored in the fields
     * below.  Application-specific attributes (starting with a lower-case
     * letter) are stored in the attrs list.  */

    char *id;  /* Indicates the name of the feature.  IDs must be unique
                * within the scope of the GFF file.*/
    char *name;  /* Display name for the feature.  This is the name to be
                  * displayed to the user.  Unlike IDs, there is no requirement
                  * that the Name be unique within the file. */

    struct slName *aliases; /* A secondary names for the feature.  It is
                             * suggested that this tag be used whenever a
                             * secondary identifier for the feature is needed,
                             * such as locus names and accession numbers.
                             * Unlike ID, there is no requirement that Alias
                             * be unique within the file. */
    
    struct slName *parentIds; /* Indicates the parent of the feature.  A parent
                               * ID can be used to group exons into transcripts,
                               * transcripts into genes, an so forth.  A feature
                               * may have multiple parents.  Parent can *only* be
                               * used to indicate a partof relationship. */
    struct gff3AnnRef *parents; /* Parent objects for parentIds */
                                      

    char *targetId; /* Indicates the target of a nucleotide-to-nucleotide or
                       protein-to-nucleotide alignment.  NULL if not specified. */
    int targetStart; /* target start/end, in 0-based, half open coordinates */
    int targetEnd;
    char *targetStrand; /* optional target strand, or NULL if none. */

    char *gap; /* The alignment of the feature to the target if the two are
                * not collinear (e.g. contain gaps).  The alignment format is
                * taken from the CIGAR format.  See "THE GAP ATTRIBUTE"
                * section of GFF3 specification for a description of this
                * format.*/

    char *derivesFromId; /* Used to disambiguate the relationship between one
                          * feature and another when the relationship is a
                          * temporal one rather than a purely structural "part
                          * of" one.  This is needed for polycistronic
                          * genes. */
    struct gff3Ann *derivesFrom; /* Object for derivesFromId */

    struct slName *notes;  /* free text notes. */

    struct slName *dbxrefs; /* database cross references. */

    struct slName *ontologyTerms; /* cross reference to ontology terms. */

    struct gff3Attr *attrs;  /* attributes, both user-define and spec-defined,
                                  * parsed into one or more values */

    struct gff3AnnRef *children;  /* child nodes */

    struct gff3SeqRegion *seqRegion;  /* start/end of sequence region, taken
                                       * from ##sequence-region records, or
                                       * NULL if not specified.*/

    struct gff3File *file;  /* file this record is associated with */
    int lineNum;            /* line number of record in file, or -1
                             * if not known */
};

struct gff3AnnRef
/* A reference to a gff3Ann object */
{
    struct gff3AnnRef *next;   /* next link in the chain */
    struct gff3Ann *ann;       /* reference to object */
};

struct gff3Attr
/* an attribute and string values */
{
    struct gff3Attr *next;     /* next attribute in the list */
    char *tag;                 /* name of attribute */
   struct slName *vals;       /* values for the attribute */
};

struct gff3SeqRegion
/* start/end of a sequence region, taken from ##sequence-region record.*/
{
    struct gff3SeqRegion *next;     /* next region */
    char *seqid;    /* sequence if of region */
    int start;      /* bounds of region */
    int end;
};

struct gff3File
/* Object representing a GFF file. Manages all memory for related objects. */
{
    char *fileName;       /* path of file that was parsed */
    struct hash *byId;    /* index of gff3Ann object by id.  Links to first object of link discontinuous features */
    struct gff3AnnRef *anns;   /* all records in the file. Includes all parts of discontinuous features */
    struct gff3AnnRef *roots;  /* all records without parents. */
    struct hash *pool;         /* used to allocate string values that tend to
                                * be repeated in the files.  localMem is also 
                                * to allocated memory for all other objects. */
    struct gff3SeqRegion *seqRegions;  /* list of gff3SeqRegion objects. */
    struct hash *seqRegionMap;  /* map of seqId to gff3SeqRegion objects. NULL
                                 * if none are specified */

    struct slName *featureOntologies;    /* feature ontology URIs */
    struct slName *attributeOntologies;  /* attribute ontology URIs */
    struct slName *sourceOntologies;     /* source ontology URIs */
    struct slName *species;              /* Species, usually NCBI Taxonomy
                                          * URI */
    char *genomeBuildSource;             /* source of genome build */
    char *genomeBuildName;               /* name or version of genome build */
    struct dnaSeq *seqs;                 /* list of sequences */
    struct hash *seqMap;                 /* map of sequence ids to sequence
                                          * string from ##FASTA section or
                                          * NULL if none specified */
    struct lineFile *lf;                 /* only set while parsing */
    FILE *errFh;            /* write errors to this file */
    int maxErr;             /* maximum number of errors before aborting */
    int errCnt;             /* error count */
};


/* standard attribute tags */
extern char *gff3AttrID;
extern char *gff3AttrName;
extern char *gff3AttrAlias;
extern char *gff3AttrParent;
extern char *gff3AttrTarget;
extern char *gff3AttrGap;
extern char *gff3AttrDerivesFrom;
extern char *gff3AttrNote;
extern char *gff3AttrDbxref;
extern char *gff3AttrOntologyTerm;

/* commonly used features names */
extern char *gff3FeatGene;
extern char *gff3FeatMRna;
extern char *gff3FeatExon;
extern char *gff3FeatCDS;
extern char *gff3FeatThreePrimeUTR;
extern char *gff3FeatFivePrimeUTR;
extern char *gff3FeatStartCodon;
extern char *gff3FeatStopCodon;

struct gff3File *gff3FileOpen(char *fileName, int maxErr, FILE *errFh);
/* Parse a GFF3 file into a gff3File object.  If maxErr not zero, then
 * continue to parse until this number of error have been reached.  A maxErr
 * less than zero does not stop reports all errors. Write errors to errFh,
 * if NULL, use stderr. */

void gff3FileFree(struct gff3File **g3fPtr);
/* Free a gff3File object */

struct gff3Ann *gff3FileFindAnn(struct gff3File *g3f, char *id);
/* find an annotation record by id, or NULL if not found. */

struct gff3Attr *gff3AnnFindAttr(struct gff3Ann *g3a, char *tag);
/* find a user attribute, or NULL */

void gff3FileWrite(struct gff3File *g3f, char *fileName);
/* write contents of an GFF3File object to a file */

INLINE struct gff3AnnRef *gff3AnnRefNew(struct gff3Ann *g3a)
/* Allocate a gff3AnnRef object from the heap.  Not used by the parsing code, as 
 * all data is contained in localMem objects */
{
struct gff3AnnRef *ref;
AllocVar(ref);
ref->ann = g3a;
return ref;
}

int gff3AnnRefLocCmp(const void *va, const void *vb);
/* sort compare function for location of two gff3AnnRef objects */

INLINE int gff3PhaseToFrame(int phase)
/* convert a phase to a frame */
{
switch (phase)
    {
    case 0:
        return 0;
    case 1:
        return 2;
    case 2:
        return 1;
    }
return -1;
}

#endif
