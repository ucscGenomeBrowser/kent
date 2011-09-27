/* Various genbank related constants and some functions that operate on them.
 * these are uses by many modules and don't really make sense anywhere. */
#ifndef GBDEFS_H
#define GBDEFS_H

/* Size for buffer of acc, with version, plus null and some extra */ 
#define GB_ACC_BUFSZ   16

/* 
 * Flag set that specifies various attributes of entry.  These can be used
 * seperately as types, or as a mask for selecting entries.
 */

/* type of entry */
#define GB_MRNA       0x1   /* is a mRNA */
#define GB_EST        0x2   /* is a EST */
#define GB_TYPE_MASK  (GB_MRNA|GB_EST)
#define GB_NUM_TYPES  2

/* Category relative to genome being aligned (organism category) */
#define GB_NATIVE  0x4   /* is the native species */
#define GB_XENO    0x8   /* is other than the native species */
#define GB_ORG_CAT_MASK  (GB_NATIVE|GB_XENO)
#define GB_NUM_ORG_CATS  2

/* Source database */
#define GB_GENBANK      0x10
#define GB_REFSEQ       0x20
#define GB_SRC_DB_MASK  (GB_GENBANK|GB_REFSEQ)
#define GB_NUM_SRC_DB   2

/* Size for array indexed by combination attributes of srcDb, type, and,
 * orgCat */
#define GB_NUM_ATTRS ((GB_TYPE_MASK|GB_ORG_CAT_MASK|GB_SRC_DB_MASK)+1)


/* Processing state */
#define GB_ALIGNED     0x100
#define GB_PROCESSED   0x200
#define GB_STATE_MASK  (GB_ALIGNED|GB_PROCESSED)
#define GB_NUM_STATES  2


/* Enumerated type for RefSeq status */
enum refSeqStat {
    RS_UNKNOWN     = 0,
    RS_REVIEWED    = 1,
    RS_VALIDATED   = 2,
    RS_PROVISIONAL = 3,
    RS_PREDICTED   = 4
};

/* Constants for NULL values */
#define NULL_VERSION  -1
#define NULL_OFFSET   -1
#define NULL_DATE      0

/* molecule type */
enum molType {
    mol_DNA,
    mol_RNA,
    mol_ds_RNA,
    mol_ds_mRNA,
    mol_ds_rRNA,
    mol_mRNA,
    mol_ms_DNA,
    mol_ms_RNA,
    mol_rRNA,
    mol_scRNA,
    mol_snRNA,
    mol_snoRNA,
    mol_ss_DNA,
    mol_ss_RNA,
    mol_ss_snoRNA,
    mol_tRNA,
    mol_cRNA,
    mol_ss_cRNA,
    mol_ds_cRNA,
    mol_ms_rRNA,
};


/* Directories */
extern char* GB_PROCESSED_DIR;
extern char* GB_ALIGNED_DIR;

/* Update names and glob patterns */
extern char* GB_FULL_UPDATE;
extern char* GB_DAILY_UPDATE_PREFIX;
extern char* GB_DAILY_UPDATE_GLOB;

struct gbSelect
/* type used to specify what is to be processed in GenBank. */
{
    struct gbSelect *next;      /* for linked lists */
    struct gbRelease *release;  /* gbIndex objects .. */
    struct gbUpdate *update;    /* may null */
    unsigned type;              /* GB_MRNA or GB_EST */
    unsigned orgCats;           /* GB_NATIVE, GB_XENO, or both (0 if unused)
                                 */
    char *accPrefix;            /* Accession prefix or null */
};

unsigned gbTypeIdx(unsigned type);
/* To convert a type const (or flag set containing it) to a zero-based index */

unsigned gbSrcDbIdx(unsigned srcDb);
/* To convert a source database const (or flag set containing it) to a
 * zero-based index */

short gbSplitAccVer(char* accVer, char* acc);
/* Split an GenBank version into accession and version number. acc maybe
 * null to just get the version. */

INLINE char *gbDropVer(char *acc)
/* remove version from accession if it has one, returning a pointer to the
 * dot so it can be restored */
{
char *dot = strrchr(acc, '.');
if (dot != NULL)
    *dot = '\0';
return dot;
}

INLINE void gbRestoreVer(char *acc, char *dot)
/* restore version number if it was removed by gbDropVer */
{
if (dot != NULL)
    *dot = '.';
}

unsigned gbParseType(char* typeStr);
/* Parse a type name (mrna or est), or comma seperate list of types
 * into a constant. Case is ignored. */

char* gbTypeName(unsigned type);
/* convert a type constant to string */

unsigned gbParseSrcDb(char* srcDbStr);
/* Parse a src db name (GenBank or RefSeq), or comma seperate list of them
 * into a constant. Case is ignored */

char* gbSrcDbName(unsigned srcDb);
/* Get string name for a srcDb */

unsigned gbGuessSrcDb(char* acc);
/* Guess the src db from an accession */

INLINE boolean gbIsProteinCodingRefSeq(char *acc)
/* is a refseq accession for a protein coding gene */
{
return startsWith("NM_", acc) || startsWith("XM_", acc);
}

unsigned gbOrgCatIdx(unsigned orgCat);
/* To convert a organism category const (or flag set containing it) to a
 * zero-based index */

unsigned gbParseOrgCat(char* orgCatStr);
/* Parse a organism category string (native or xeno), or comma seperate list
 * of them into a constant. Case is ignored */

char* gbOrgCatName(unsigned orgCat);
/* Convert an organism category (native or xeno) to a name */

char* gbFmtSelect(unsigned select);
/* Format a set of the various selection flags.  If restricted to a particular
 * type, then this set is parsable by the parse methods. 
 * Warning: return is a static buffer, however rotated between 8 buffers to
 * make it easy to use in print statements. */

char* gbSelectDesc(struct gbSelect* select);
/* Get selection description for messages.  Warning, static return buffer */

enum molType gbParseMolType(char* molTypeStr);
/* Parse molecule type string. */

char* gbMolTypeSym(enum molType molType);
/* return symbolic molecule type */

#endif
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
