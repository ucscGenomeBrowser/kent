/* estOrient - convert ESTs so that orientation matches directory of
 * transcription */
#include "common.h"
#include "psl.h"
#include "pslReader.h"
#include "estOrientInfo.h"
#include "hash.h"
#include "genbank.h"
#include "sqlNum.h"
#include "jksql.h"
#include "hdb.h"
#include "options.h"

/* command line options and values */
static struct optionSpec optionSpecs[] =
{
    {"chrom", OPTION_STRING|OPTION_MULTI},
    {"keepDisoriented", OPTION_BOOLEAN},
    {"disoriented", OPTION_STRING},
    {"info", OPTION_STRING},
    {"inclVer", OPTION_BOOLEAN},
    {"fileInput", OPTION_BOOLEAN},
    {"estOrientInfo", OPTION_STRING},
    {NULL, 0}
};

static struct slName *gChroms = NULL;
static boolean gKeepDisoriented = FALSE;
static boolean gInclVer = FALSE;
static boolean gFileInput = FALSE;
static char *gDisoriented = NULL;
static char *gEstOrientInfoFile = NULL;
static char *gInfo = NULL;

void usage(char *msg, ...)
/* usage msg and exit */
{
va_list ap;
va_start(ap, msg);
vfprintf(stderr, msg, ap);
errAbort("\n%s",
         "estOrient [options] db estTable outPsl\n"
         "\n"
         "Read ESTs from a database and determine orientation based on\n"
         "estOrientInfo table or direction in gbCdnaInfo table.  Update\n"
         "PSLs so that the strand reflects the direction of transcription.\n"
         "By default, PSLs where the direction can't be determined are dropped.\n"
         "\n"
         "Options:\n"
         "   -chrom=chr - process this chromosome, maybe repeated\n"
         "   -keepDisoriented - don't drop ESTs where orientation can't\n"
         "    be determined.\n"
         "   -disoriented=psl - output ESTs that where orientation can't\n"
         "    be determined to this file.\n"
         "   -inclVer - add NCBI version number to accession if not already\n"
         "    present.\n"
         "   -fileInput - estTable is a psl file\n"
         "   -estOrientInfo=file - instead of getting the orientation information\n"
         "    from the estOrientInfo table, load it from this file.  This data is the\n"
         "    output of polyInfo command.  If this option is specified, the direction\n"
         "    will not be looked up in the gbCdnaInfo table and db can be `no'.\n"
         "   -info=infoFile - write information about each EST to this tab\n"
         "    separated file \n"
         "       qName tName tStart tEnd origStrand newStrand orient\n"
         "    where orient is < 0 if PSL was reverse, > 0 if it was left unchanged\n"
         "    and 0 if the orientation couldn't be determined (and was left\n"
         "    unchanged).\n"
         );
}

/* cache of information about current EST from gbCdnaInfo. */
static char gbCacheAcc[32];
static int gbCacheDir = -1;   // read directory, -1 if unknown
static int gbCacheVer = -1;   // version, or -1 if unknown

static void gbCacheLoad(struct sqlConnection *conn, char *acc)
/* load cache for gbCdnaInfo */
{
safecpy(gbCacheAcc, sizeof(gbCacheAcc), acc);
gbCacheDir = -1;
gbCacheVer = -1;
char query[512];
safef(query, sizeof(query), "select version, direction from gbCdnaInfo where acc='%s'", acc);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row = sqlNextRow(sr);
if (row != NULL)
    {
    gbCacheVer = sqlSigned(row[0]);
    gbCacheDir = sqlSigned(row[1]);
    }
sqlFreeResult(&sr);
}
static void gbCacheNeed(struct sqlConnection *conn, char *acc)
/* load cache with acc if not already loaded */
{
if (!sameString(acc, gbCacheAcc))
    gbCacheLoad(conn, acc);
}

static int cDnaReadDirection(struct sqlConnection *conn, char *acc)
/* Return the direction field from the gbCdnaInfo table for accession
   acc. Return -1 if not in table.*/
{
gbCacheNeed(conn, acc);
return gbCacheDir;
}

static int cDnaVersion(struct sqlConnection *conn, char *acc)
/* Return the version field from the gbCdnaInfo table for accession
   acc. Return -1 if not in table.*/
{
gbCacheNeed(conn, acc);
return gbCacheVer;
}

static struct hash *loadOrientInfoTbl(struct sqlConnection *conn, char *chrom)
/* load data from estOrientInfo table for chrom, or all if chrom is NULL */
{
struct hash *orientHash = hashNew(24);
struct sqlResult *sr;
int rowOff;
char **row;

/* to save memory, only select ones where orientation was determined from
 * introns. */
if (chrom != NULL)
    sr = hChromQuery(conn, "estOrientInfo", chrom, "(intronOrientation != 0)", &rowOff);
else
    {
    rowOff = (sqlFieldIndex(conn, "estOrientInfo", "bin") < 0) ? 0 : 1;
    sr = sqlGetResult(conn, "select * from estOrientInfo where (intronOrientation != 0)");
    }

while ((row = sqlNextRow(sr)) != NULL)
    {
    struct estOrientInfo *eoi = estOrientInfoLoadLm(row+rowOff, orientHash->lm);
    hashAdd(orientHash, eoi->name, eoi);
    }
return orientHash;
}

static struct hash *loadOrientInfoFile(char *orientInfoFile)
/* load data estOrientInfo data from a file */
{
struct hash *orientHash = hashNew(24);
struct lineFile *lf = lineFileOpen(orientInfoFile, TRUE);
char *row[EST_ORIENT_INFO_NUM_COLS];
while (lineFileNextRowTab(lf, row, EST_ORIENT_INFO_NUM_COLS))
    {
    struct estOrientInfo *eoi = estOrientInfoLoadLm(row, orientHash->lm);
    if (eoi->intronOrientation != 0)
        hashAdd(orientHash, eoi->name, eoi);
    }
lineFileClose(&lf);
return orientHash;
}


static struct estOrientInfo *findOrientInfo(struct hash *orientHash, char *acc, struct psl *psl)
/* search for an orientInfo object for a psl. */
{
/* can be multiple entries for a name, find overlap */
struct hashEl *hel = hashLookup(orientHash, acc);
while (hel != NULL)
    {
    struct estOrientInfo *eoi = hel->val;
    if ((eoi->chromStart == psl->tStart) && (eoi->chromEnd == psl->tEnd)
        && sameString(eoi->chrom, psl->tName))
        return eoi;
    hel = hashLookupNext(hel);
    }
return NULL;
}

static int getOrient(struct sqlConnection *conn, struct hash *orientHash,
                     struct psl *est)
/* get orientation of an EST, or 0 if unknown */
{
char acc[GENBANK_ACC_BUFSZ];
int orient = 0;
genbankDropVer(acc, est->qName);

// try both without and with version
struct estOrientInfo *eoi = findOrientInfo(orientHash, acc, est);
if ((eoi == NULL) && !sameString(acc, est->qName))
    eoi = findOrientInfo(orientHash, est->qName, est);
if (eoi != NULL)
    orient = eoi->intronOrientation;
if ((orient == 0) && (conn != NULL))
    {
    int readDir = cDnaReadDirection(conn, acc);
    if (readDir == 5) 
        orient = 1;
    else if (readDir == 3) 
        orient = -1;
    }
return orient;
}

static void addVersion(struct sqlConnection *conn, struct psl *est)
/* update the qName field of a psl to have the version (unless it already
 * has it */
{
if (strchr(est->qName, '.') == NULL)
    {
    int ver = cDnaVersion(conn, est->qName);
    if (ver >= 0)
        {
        char newQName[32];
        safef(newQName, sizeof(newQName), "%s.%d", est->qName, ver);
        est->qName = needMoreMem(est->qName, 0, strlen(newQName)+1);
        strcpy(est->qName, newQName);
        }
    }
}

static void revOrient(struct psl *est)
/* reverse query orientation on an EST */
{
est->strand[0] = ((est->strand[0] == '+') ? '-' : '+');
reverseIntRange(&est->qStart, &est->qEnd, est->qSize);
}

static void processEst(struct sqlConnection *conn, struct hash *orientHash,
                       struct psl *est, FILE *outPslFh, FILE *disorientedFh,
                       FILE *infoFh)
/* adjust orientation of an EST and output */
{
int orient = getOrient(conn, orientHash, est); 
char origStrand[3];
safecpy(origStrand, sizeof(origStrand), est->strand);
if (gInclVer)
    addVersion(conn, est);
if ((orient != 0) || gKeepDisoriented)
    {
    if (orient < 0)
        revOrient(est);
    pslTabOut(est, outPslFh);
    }
else if (disorientedFh != NULL)
    pslTabOut(est, disorientedFh);
if (infoFh != NULL)
    fprintf(infoFh, "%s\t%s\t%d\t%d\t%s\t%s\t%d\n",
            est->qName, est->tName, est->tStart, est->tEnd,
            origStrand, est->strand, orient);
}

static void orientEstsChromTbl(struct sqlConnection *conn, struct sqlConnection *conn2,
                               char *chrom, char *estTable, FILE *outPslFh,
                               FILE *disorientedFh, FILE *infoFh)
/* orient ESTs on one chromsome */
{
struct psl *psl;
struct hash *orientHash = loadOrientInfoTbl(conn, chrom);

struct pslReader *pr = pslReaderChromQuery(conn, estTable, chrom, NULL);
while ((psl = pslReaderNext(pr)) != NULL)
    {
    processEst(conn2, orientHash, psl, outPslFh, disorientedFh, infoFh);
    pslFree(&psl);
    }

pslReaderFree(&pr);
hashFree(&orientHash);
}

static void orientEstsTbl(char *db, char *estTable, FILE *outPslFh,
                          FILE *disorientedFh, FILE *infoFh)
/* orient ESTs from a table */
{
struct sqlConnection *conn = sqlConnectReadOnly(db);
struct sqlConnection *conn2 = sqlConnectReadOnly(db);
struct slName *chroms = NULL;
struct slName *chrom;
if (gChroms != NULL)
    chroms = gChroms;
else
    chroms = hAllChromNames(db);
slNameSort(&chroms);

for (chrom = chroms; chrom != NULL; chrom = chrom->next)
    orientEstsChromTbl(conn, conn2, chrom->name, estTable, outPslFh, disorientedFh, infoFh);

sqlDisconnect(&conn);
sqlDisconnect(&conn2);
}

static void orientEstsFile(char *db, char *estFile, FILE *outPslFh,
                           FILE *disorientedFh, FILE *infoFh)
/* orient ESTs on one chromsome */
{
struct sqlConnection *conn = sameString(db, "no") ? NULL : sqlConnectReadOnly(db);
struct psl *psl;
struct hash *orientHash = (gEstOrientInfoFile != NULL)
    ? loadOrientInfoFile(gEstOrientInfoFile)
    : loadOrientInfoTbl(conn, NULL);

struct pslReader *pr = pslReaderFile(estFile, NULL);
while ((psl = pslReaderNext(pr)) != NULL)
    {
    processEst(conn, orientHash, psl, outPslFh, disorientedFh, infoFh);
    pslFree(&psl);
    }

pslReaderFree(&pr);
hashFree(&orientHash);
sqlDisconnect(&conn);
}

static void estOrient(char *db, char *estTable, char *outPslFile)
/* convert ESTs so that orientation matches directory of transcription */
{
FILE * outPslFh = mustOpen(outPslFile, "w");
FILE *disorientedFh = NULL;
if (gDisoriented != NULL)
    disorientedFh = mustOpen(gDisoriented, "w");
FILE *infoFh = NULL;
if (gInfo != NULL)
    {
    infoFh = mustOpen(gInfo, "w");
    fprintf(infoFh, "#qName\ttName\ttStart\ttEnd\torigStrand\tnewStrand\torient\n");
    }
if (gFileInput)
    orientEstsFile(db, estTable, outPslFh, disorientedFh, infoFh);
else
    orientEstsTbl(db, estTable, outPslFh, disorientedFh, infoFh);

carefulClose(&infoFh);
carefulClose(&disorientedFh);
carefulClose(&outPslFh);
}

int main(int argc, char *argv[])
/* Process command line */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 4)
    usage("wrong # of args:");
ZeroVar(&gbCacheAcc);
char *db = argv[1], *estTable = argv[2], *outPslFile = argv[3];
gChroms = optionMultiVal("chrom", NULL);
gKeepDisoriented = optionExists("keepDisoriented");
gDisoriented = optionVal("disoriented", NULL);
gInfo = optionVal("info", NULL);
gInclVer = optionExists("inclVer");
gFileInput = optionExists("fileInput");
gEstOrientInfoFile = optionVal("estOrientInfo", NULL);
if (gKeepDisoriented && (gDisoriented != NULL))
    errAbort("can't specify both -keepDisoriented and -disoriented");
if (gFileInput && (gChroms != NULL))
    errAbort("can't specify both -fileInput and -chrom");
if (gFileInput && gInclVer)
    errAbort("can't specify both -fileInput and -inclVer");
if (sameString(db, "no") && !gFileInput)
    errAbort("must specify -fileInput with db set to \"no\"");
if (sameString(db, "no") && !gEstOrientInfoFile)
    errAbort("must specify -estOrientInfo with db set to \"no\"");

estOrient(db, estTable, outPslFile);
return 0;
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

