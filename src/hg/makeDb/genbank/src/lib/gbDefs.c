/* Various genbank related constants and some functions that operate on them.
 * these are uses by many modules and don't really make sense anywhere. */
#include "common.h"
#include "gbDefs.h"
#include "gbRelease.h"
#include "gbUpdate.h"
#include "gbGenome.h"
#include "hash.h"

static char const rcsid[] = "$Id: gbDefs.c,v 1.12 2009/06/24 17:00:37 genbank Exp $";

/* Directories */
char* GB_PROCESSED_DIR = "data/processed";
char* GB_ALIGNED_DIR = "data/aligned";

/* Update names and glob patterns */
char* GB_FULL_UPDATE = "full";
char* GB_DAILY_UPDATE_PREFIX = "daily.";
char* GB_DAILY_UPDATE_GLOB = "daily.*";

/* molType enum/sym mappings  */
static struct {
    enum molType type;
    char *sym;
} molTypeMappings[] = {
    {mol_DNA,       "DNA"},
    {mol_RNA,       "RNA"},
    {mol_ds_RNA,    "ds-RNA"},
    {mol_ds_mRNA,   "ds-mRNA"},
    {mol_ds_rRNA,   "ds-rRNA"},
    {mol_mRNA,      "mRNA"},
    {mol_ms_DNA,    "ms-DNA"},
    {mol_ms_RNA,    "ms-RNA",},
    {mol_rRNA,      "rRNA"},
    {mol_scRNA,     "scRNA"},
    {mol_snRNA,     "snRNA"},
    {mol_snoRNA,    "snoRNA"},
    {mol_ss_DNA,    "ss-DNA"},
    {mol_ss_RNA,    "ss-RNA"},
    {mol_ss_snoRNA, "ss-snoRNA"},
    {mol_tRNA,      "tRNA"},
    {mol_cRNA,      "cRNA"},
    {mol_ss_cRNA,   "ss-cRNA"},
    {mol_ds_cRNA,   "ds-cRNA"},
    {mol_ms_rRNA,   "ms-rRNA",},
    {-1,            NULL}
};
static struct hash *molSymToType = NULL;

short gbSplitAccVer(char* accVer, char* acc)
/* Split an GenBank version into accession and version number. acc maybe
 * null to just get the version. */
{
char* dot = strchr(accVer, '.');
int len = (dot == NULL) ? strlen(accVer) : (dot-accVer);
if ((dot == NULL) || (len >= GB_ACC_BUFSZ))
    errAbort("invalid genbank acc.ver: \"%s\"", accVer);
if (acc != NULL)
    {
    strncpy(acc, accVer, len);
    acc[len] = '\0';
    }
if (dot == NULL)
    return -1;
else
    {
    char* stop;
    int ver = strtol(dot+1, &stop, 10);
    if ((stop == (dot+1)) || (*stop != '\0'))
        errAbort("invalid genbank acc.ver: \"%s\"", accVer);
    return ver;
    }
}

unsigned gbTypeIdx(unsigned type)
/* To convert a type const (or flag set containing it) to a zero-based index */
{
switch (type & GB_TYPE_MASK) {
  case GB_MRNA:
    return 0;
  case GB_EST:
    return 1;
  default:
    internalErr();
    return 0;
  }
}

unsigned gbSrcDbIdx(unsigned srcDb)
/* To convert a source database const (or flag set containing it) to a
 * zero-based index */
{
switch (srcDb & GB_SRC_DB_MASK) {
  case GB_GENBANK:
    return 0;
  case GB_REFSEQ:
    return 1;
  default:
    internalErr();
    return 0;
  }
}

unsigned gbParseType(char* typeStr)
/* Parse a type name (mrna or est), or comma seperate list of types
 * into a constant. Case is ignored. */
{
unsigned type = 0;
char* next = typeStr;
while (*next != '\0')
    {
    int len = strcspn(next, ",");
    if (strncasecmp(next, "mrna", len) == 0)
        type |= GB_MRNA;
    else if (strncasecmp(next, "est", len) == 0)
        type |= GB_EST;
    else
        errAbort("invalid type \"%.*s\", expected \"mrna\" or \"est\"",
                 len, next);
    next += len;
    if (*next == ',')
        next++;
    }
return type;
}

char* gbTypeName(unsigned type)
/* convert a type constant to string */
{
if (type == GB_MRNA)
    return "mRNA";
if (type == GB_EST)
    return "EST";
assert(FALSE);
return NULL;

}

unsigned gbParseSrcDb(char* srcDbStr)
/* Parse a src db name (GenBank or RefSeq), or comma seperate list of them
 * into a constant. Case is ignored */
{
unsigned srcDb = 0;
char* next = srcDbStr;
while (*next != '\0')
    {
    int len = strcspn(next, ",");
    if (strncasecmp(next, "GenBank", len) == 0)
        srcDb |= GB_GENBANK;
    else if (strncasecmp(next, "RefSeq", len) == 0)
        srcDb |= GB_REFSEQ;
    else
        errAbort("invalid srcDb \"%.*s\", expected \"GenBank\" or \"RefSeq\"",
                 len, next);
    next += len;
    }
return srcDb;
}

char* gbSrcDbName(unsigned srcDb)
/* Get string name for a srcDb */
{
if (srcDb == GB_GENBANK)
    return "GenBank";
if (srcDb == GB_REFSEQ)
    return "RefSeq";
assert(FALSE);
return NULL;
}

unsigned gbGuessSrcDb(char* acc)
/* Guess the src db from an accession */
{
/* refseq ids start with N?_ or X?_ */
if ((strlen(acc) > 3) && (acc[2] == '_') 
    && ((acc[0] == 'N') || (acc[0] == 'X')))
    return GB_REFSEQ;
else
    return GB_GENBANK;
}

unsigned gbOrgCatIdx(unsigned orgCat)
/* To convert a organism category const (or flag set containing it) to a
 * zero-based index */
{
switch (orgCat & GB_ORG_CAT_MASK) {
  case GB_NATIVE:
    return 0;
  case GB_XENO:
    return 1;
  default:
    internalErr();
    return 0;
  }
}

unsigned gbParseOrgCat(char* orgCatStr)
/* Parse a organism category string (native or xeno), or comma seperate list
 * of them into a constant. Case is ignored */
{
unsigned orgCat = 0;
char* next = orgCatStr;
while (*next != '\0')
    {
    int len;
    if (*next == ',')
        next++;
    len = strcspn(next, ",");
    if (strncasecmp(next, "native", len) == 0)
        orgCat |= GB_NATIVE;
    else if (strncasecmp(next, "xeno", len) == 0)
        orgCat |= GB_XENO;
    else
        errAbort("invalid organism category \"%.*s\", expected \"native\" or \"xeno\"",
                 len, next);
    next += len;
    }
return orgCat;
}

char* gbOrgCatName(unsigned orgCat)
/* Convert an organism category (native or xeno) to a name */
{
if (orgCat == GB_NATIVE)
    return "native";
if (orgCat == GB_XENO)
    return "xeno";
assert(FALSE);
return NULL;
}

char* gbFmtSelect(unsigned select)
/* Format a set of the various selection flags.  If restricted to a particular
 * type, then this set is parsable by the parse methods.
 * Warning: return is a static buffer, however rotated between 8 buffers to
 * make it easy to use in print statements. */
/* FIXME: this name is now confusing due other select */
{
static int iBuf = 0;
static char bufs[8][256];
if (++iBuf >= 8)
    iBuf = 0;
bufs[iBuf][0] = '\0';

if (select &GB_MRNA)
    strcat(bufs[iBuf], ",mrna");
if (select & GB_EST)
    strcat(bufs[iBuf], ",est");
if (select & GB_NATIVE)
    strcat(bufs[iBuf], ",native");
if (select & GB_XENO)
    strcat(bufs[iBuf], ",xeno");
if (select & GB_GENBANK)
    strcat(bufs[iBuf], ",genbank");
if (select & GB_REFSEQ)
    strcat(bufs[iBuf], ",refseq");
return bufs[iBuf]+1; /* skip first comma */
}

char* gbSelectDesc(struct gbSelect* select)
/* Get selection description for messages.  Warning, static return buffer */
{
static char desc[512];

strcpy(desc, select->release->name);
if (select->update != NULL)
    {
    strcat(desc, " ");
    strcat(desc, select->update->name);
    }
if (select->release->genome != NULL)
    {
    strcat(desc, " ");
    strcat(desc, select->release->genome->database);
    }
if (select->orgCats != 0)
    {
    strcat(desc, " ");
    strcat(desc, gbFmtSelect(select->orgCats));
    }
if (select->type != 0)
    {
    strcat(desc, " ");
    strcat(desc, gbFmtSelect(select->type));
    }
if (select->accPrefix != 0)
    {
    strcat(desc, ".");
    strcat(desc, select->accPrefix);
    }
return desc;
}

char* gbSrcDbSym(unsigned srcDb)
/* convert a srcDb type to the database enum symbol used in various tables.
 * will mask srcDb as a bit set */
{
if (srcDb & GB_GENBANK)
    return "GenBank";
if (srcDb & GB_REFSEQ)
    return "RefSeq";
internalErr();
return NULL;
}

char* gbTypeSym(unsigned type)
/* convert a type to the database enum symbol used in various tables.
 * will mask type as a bit set */
{
if (type & GB_MRNA)
    return "mRNA";
if (type & GB_EST)
    return "EST";
internalErr();
return NULL;
}

static void buildMolTypeHash()
/* build molSymToType hash */
{
int i;
molSymToType = hashNew(12);
for (i = 0; molTypeMappings[i].sym != NULL; i++)
    hashAddInt(molSymToType, molTypeMappings[i].sym, molTypeMappings[i].type);
}

enum molType gbParseMolType(char* molTypeStr)
/* Parse molecule type string. */
{
if (molSymToType == NULL)
    buildMolTypeHash();
struct hashEl *hel = hashLookup(molSymToType, molTypeStr);
#if 0 // Disable hack, looks like it occurs only on hgw1
if (hel == NULL)
    {
    // FIXME: attempt to correct mRN -> mRNA corruption that is seen on RR.
    // no idea why this happens, but it's observed only in this column, however it's
    // corrupt when first read into memory in lineFile.
    int l = strlen(molTypeStr);
    if ((l > 0) && ((molTypeStr[l-1] & 0xf0) == 0))
        {
        char fixed = molTypeStr[l-1] |  0x40;
        fprintf(stderr, "WARNING: attempting to fix changing '%c' to '%c'\n", molTypeStr[l-1], fixed);
        molTypeStr[l-1] = fixed;
        hel = hashLookup(molSymToType, molTypeStr);
        }
    }
#endif
if (hel == NULL)
    errAbort("unknown molecule type: %s", molTypeStr);
return ptrToLL(hel->val);;
}

char* gbMolTypeSym(enum molType molType)
/* return symbolic molecule type */
{
if ((molType < 0) || (molType >= ArraySize(molTypeMappings)))
    errAbort("invalid molecule type number: %d", molType);
return  molTypeMappings[molType].sym;
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */


