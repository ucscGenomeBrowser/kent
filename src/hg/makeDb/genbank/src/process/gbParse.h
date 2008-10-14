/* Parse a gbRecord.  Module serves as a global singleton with state of last
 * record read.  Also has some parsing utility function. */
#ifndef GBPARSE_H
#define GBPARSE_H

struct kvt;
struct lineFile;

/* flags for a field */
#define GBF_NONE       0x00  /* no special flags */
#define GBF_TRACK_VALS 0x01
#define GBF_MULTI_LINE 0x02
#define GBF_TO_LOWER   0x04
#define GBF_MULTI_VAL  0x08
#define GBF_CONCAT_VAL 0x10 /* don't space-seperate multiline values */
#define GBF_BOOLEAN    0x20 /* boolean value */
#define GBF_SUB_SPACE  0x40 /* change spaces to _ in values */
#define GBF_MULTI_SEMI 0x80 /* semi-colon separate multiple values */

struct gbFieldUseCounter
/* For some fields we use this to keep track of all
 * values in that field and how often they are used. */
    {
    struct gbFieldUseCounter *next;     /* Next in list. */
    char *val;                          /* A value seen in this field. */
    int useCount;                       /* How many times it's been seen */
    };

struct gbField
/* Structure that helps parse one field of a genBank file. */    
    {
    struct gbField *next;       /* Next field. */
    struct gbField *children;   /* First subfield if any. */

    struct hash *valHash;       /* Hash of all vals seen (may be null) */
    struct gbFieldUseCounter *valList;  /* List of all vals (may be null) */
    struct dyString *val;               /* String value. */
    unsigned flags;                     /* Various flags. */
    int indent;                         /* expected indentation level, -1 if
                                           parsed from other fields */
    char *readName;                     /* GenBank's name for field. */
    char *writeName;                    /* Jim's name for field.  If null field not saved. */
    };

extern struct gbField *gbStruct;               /* Root of field tree. */
extern struct gbField *gbLocusField;     
extern struct gbField *gbDefinitionField;
extern struct gbField *gbAccessionField;
extern struct gbField *gbVersionField;
extern struct gbField *gbCommentField;
extern struct gbField *gbAuthorsField;
extern struct gbField *gbJournalField;
extern struct gbField *gbKeywordsField;
extern struct gbField *gbOrganismField;
extern struct gbField *gbTissueField;
extern struct gbField *gbSexField;
extern struct gbField *gbDevStageField;
extern struct gbField *gbCloneField;
extern struct gbField *gbChromosomeField;
extern struct gbField *gbMapField;
extern struct gbField *gbSourceOrganism;
extern struct gbField *gbPrtField;
extern struct gbField *gbGeneDbxField;
extern struct gbField *gbGeneGeneField;
extern struct gbField *gbCdsDbxField;
extern struct gbField *gbCdsGeneField;
extern struct gbField *gbProteinIdField;
extern struct gbField *gbTranslationField;
extern struct gbField *gbCloneLibField;

/* RefSeq specific data */
extern struct gbField *gbRefSeqStatusField;
extern struct gbField *gbRefSeqSummaryField;
extern struct gbField *gbRefSeqCompletenessField;
extern struct gbField *gbRefSeqDerivedField;


struct gbMiscDiff
/* object used to hold one misc diff value */
{
    struct gbMiscDiff *next;
    char *loc;     /* location in cDNA */
    char *note;    /* /note= field */
    char *gene;    /* /gene= field */
    char *replace; /* /replace= field */
};

/* list of misc diffs in current record */
extern struct gbMiscDiff *gbMiscDiffVals;

/* appears to be an invirtorgen evil clone (modified to match genome) */
extern boolean isInvitrogenEvilEntry;
/* Athersys RAGE library */
extern boolean isAthersysRageEntry;
/* ORESTES PCR library */
extern boolean isOrestesEntry;

char *skipLeadingNonSpaces(char *s);
/* Return first non-white space or NULL. */

char *skipToNextWord(char *s);
/* Returns start of next (white space separated) word.
 * Returns NULL if not another word. */

char *nextWordStart(char *s);
/* Return start of next (white space separated) word. */

char *skipThrough(char *s, char *through);
/* Skip to next character in s that's not part of through. */

char *skipTo(char *s, char *target);
/* Skip to next character in s that is part of target. */

void gbfInit();
/* Create a heirarchical structure for parsing genBank fields. */

boolean gbfReadFields(struct lineFile *lf);
/* Read in a single Gb record up to the ORIGIN. */

DNA* gbfReadSequence(struct lineFile *lf, int *retSize);
/* Read in sequence part of genBank file, or NULL if no sequence */

void gbfSkipSequence(struct lineFile *lf);
/* Skip to '//' if sequence not read */

void gbfFlatten(struct kvt *kvt);
/* Flatten out parsed genebank data into keyVal array. */

#endif

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
