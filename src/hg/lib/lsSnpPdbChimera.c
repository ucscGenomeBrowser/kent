/* lsSnpPdbChimera - generate Chimera scripts to visualize proteins with SNPs
 * annotated.  Takes an external python script that defines methods used in
 * the rendering.  The generates a .chimerax file that contains the script as
 * well as the code to initialize Chimera for the specific PDB and SNP set.
 */

#include "common.h"
#include "lsSnpPdbChimera.h"
#include "obscure.h"
#include "jksql.h"
#include "lsSnpPdb.h"
#include "trashDir.h"

/* Chimerax XML wrapper around python code in .chimerax files */
static char *chimeraxHead =
    "<?xml version=\"1.0\"?>\n"
    "<ChimeraPuppet type=\"std_webdata\">\n"
    "  <commands>\n"
    "    <py_cmd>\n"
    "<![CDATA[\n";
static char *chimeraxTail =
    "]]>\n"
    "    </py_cmd>\n"
    "  </commands>\n"
    "</ChimeraPuppet>\n";

/* location of python code relative to CGI dir */
static char *chimeraxPythonFile = "lsSnpPdbChimera.py";


static void getOutFile(char *pdbId, char *other,
                       struct tempName *outName)
/* generate output file names, other can be NULL*/
{
char baseName[PATH_LEN];
if (other != NULL)
    safef(baseName, sizeof(baseName), "%s.%s", pdbId, other);
else 
    safecpy(baseName, sizeof(baseName), pdbId);
trashDirFile(outName, "lssnp", baseName, ".chimerax");
}

static void prSnp(FILE *xfh, struct lsSnpPdb *pdbSnp, char *primarySnpId)
/* output one SNP tuple entry in list passed to displayPdb function */
{
boolean isPrimary = (primarySnpId != NULL) && sameString(pdbSnp->snpId, primarySnpId);
fprintf(xfh, "(\"%s\", \"%c\", %d,%s), ", pdbSnp->snpId, pdbSnp->chain,
        pdbSnp->snpPdbLoc, (isPrimary ? " True" : ""));
}

static void chimeraxGen(struct sqlConnection *conn,
                        char *pdbId, char *where,
                        char *primarySnpId,
                        char *outName)
/* Generate a chimerax file for the given PDB, optionally coloring
 * primarySnpId differently.  The where arguments specifies which entries to
 * obtain from the lsSnpPdb table.
 */
{
FILE *xfh = mustOpen(outName, "w");
fputs(chimeraxHead, xfh);

FILE *pxf = mustOpen(chimeraxPythonFile, "r");
copyOpenFile(pxf, xfh);
carefulClose(&pxf);

fprintf(xfh, "\ndisplayPdb(\"%s\", (", pdbId);

struct lsSnpPdb *pdbSnp, *pdbSnps = NULL;
if (sqlTableExists(conn, "lsSnpPdb"))
    pdbSnps = sqlQueryObjs(conn, (sqlLoadFunc)lsSnpPdbLoad, sqlQueryMulti,
                           "SELECT * FROM lsSnpPdb WHERE %s", where);
for (pdbSnp = pdbSnps; pdbSnp != NULL; pdbSnp = pdbSnp->next)
    prSnp(xfh, pdbSnp, primarySnpId);
if (pdbSnps == NULL)
    fprintf(xfh, ",");  // no table or none selected
lsSnpPdbFreeList(&pdbSnps);

fprintf(xfh, "))\n");

fputs(chimeraxTail, xfh);
carefulClose(&xfh);
}

void lsSnpPdbChimeraSnpAnn(struct sqlConnection *conn,
                           char *pdbId, char *primarySnpId,
                           struct tempName *outName)
/* Generate a chimerax file for the given pdb with all non-synonymous SNPs
 * that have been mapped to this protein.  If primarySnpId is not NULL, it is
 * colored differently than the other SNPs.  Fills in outName structure. */
{
getOutFile(pdbId, primarySnpId, outName);
char where[512];
safef(where, sizeof(where), "(pdbId=\"%s\")", pdbId);
chimeraxGen(conn, pdbId, where, primarySnpId, outName->forCgi);
}

struct slName *lsSnpPdbChimeraGetSnpPdbs(struct sqlConnection *conn,
                                         char *snpId)
/* get list of PDBs to which snpId is mapped.  */
{
if (!sqlTableExists(conn, "lsSnpPdb"))
    return NULL;
char query[256];
safef(query, sizeof(query), "SELECT distinct pdbId FROM lsSnpPdb WHERE (snpId = \"%s\")",
      snpId);
struct slName *pdbIds = sqlQuickList(conn, query);
slNameSort(&pdbIds);
return pdbIds;
}

char *lsSnpPdbChimeraGetStructType(struct sqlConnection *conn,
                                   char *pdbId)
/* Determine structure type of a PDB (NMR or X-Ray.  Constant result, don't
 * free. */
{

char query[256], buf[32];
safef(query, sizeof(query), "SELECT structType FROM lsSnpPdb WHERE (pdbId = \"%s\")",
      pdbId);
char *structType = sqlNeedQuickQuery(conn, query, buf, sizeof(buf));
if (sameString(structType, "XRay"))
    return "X-Ray";
else if (sameString(structType, "NMR"))
    return "NMR";
else
    return "unknown";
}

char *lsSnpPdbGetUrlPdbSnp(char *pdbId, char *snpId)
/* get LS-SNP/PDB URL for a particular PDB+SNP. */
{
char url[256];
safef(url, sizeof(url), "http://karchinlab.org:8080/LS-SNP_pdb/SNPViewer?pdbId=%s&snpName=%s",
      pdbId, snpId);
return cloneString(url);
}
