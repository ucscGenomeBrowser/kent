/* vgLoadJax - Load visiGene database from jackson database. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "options.h"
#include "portable.h"
#include "obscure.h"
#include "jksql.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "vgLoadJax - Load visiGene database from jackson database\n"
  "usage:\n"
  "   vgLoadJax jaxDb visiDb\n"
  "Load everything in jackson database tagged after date to\n"
  "visiGene database.  Most commonly run as\n"
  "   vgLoadJax jackson visiGene\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

char *clJaxDb, *clVisiDb;

struct slName *jaxSpecList(struct sqlConnection *conn)
/* Get list of specimen id's. */
{
return sqlQuickList(conn, "select _Specimen_key from GXD_Specimen");
}

void submitRefToFiles(struct sqlConnection *conn, char *ref, char *fileRoot)
/* Create a .ra and a .tab file for given reference. */
{
char raName[PATH_LEN], tabName[PATH_LEN];
FILE *ra = NULL, *tab = NULL;
struct dyString *query = dyStringNew(0);
struct sqlResult *sr;
char **row;
char *copyright;
struct slName *list, *el;

safef(raName, sizeof(raName), "%s.ra", fileRoot);
safef(tabName, sizeof(tabName), "%s.tab", fileRoot);
tab = mustOpen(tabName, "w");


dyStringAppend(query, "select authors, journal, title from BIB_Refs where ");
dyStringPrintf(query, "_Refs_key = %s", ref);
sr = sqlGetResult(conn, query->string);
row = sqlNextRow(sr);
if (row == NULL)
    errAbort("Can't find _Refs_key %s in BIB_Refs", ref);

/* Make ra file with stuff common to whole submission set. */
ra = mustOpen(raName, "w");
list = charSepToSlNames(row[0], ';');
fprintf(ra, "submitSet jax%s\n", ref);
fprintf(ra, "journal %s\n", row[1]);
fprintf(ra, "publication %s\n", row[2]);
fprintf(ra, "contributor ");
for (el = list; el != NULL; el = el->next)
    {
    char *lastName = skipLeadingSpaces(el->name);
    char *initials = strrchr(lastName, ' ');
    if (initials == NULL)
	initials = "";
    else
	*initials++ = 0;
    fprintf(ra, "%s", lastName);
    if (initials[0] != 0)
	{
	char c;
	fprintf(ra, " ");
	while ((c = *initials++) != 0)
	    {
	    fprintf(ra, "%c.", c);
	    }
	}
    fprintf(ra, ",");
    }
fprintf(ra, "\n");
slNameFreeList(&list);
sqlFreeResult(&sr);

/* Add in copyright notice */
dyStringClear(query);
dyStringPrintf(query, 
	"select copyrightNote from IMG_Image where _Refs_key = %s", ref);
copyright = sqlQuickString(conn, query->string);
if (copyright != NULL)
    fprintf(ra, "copyright %s\n", copyright);
freez(&copyright);

dyStringClear(query);
dyStringAppend(query, 
	"select MRK_Marker.symbol as gene,"
               "GXD_Specimen.sex as sex,"
	       "GXD_Specimen.age as age,"
	       "GXD_Specimen.ageMin as ageMin,"
	       "GXD_Specimen.ageMax as ageMax,"
	       "IMG_ImagePane.paneLabel as paneLabel,"
	       "ACC_Accession.accID as fileKey,"
	       "IMG_Image._Image_key as imageKey\n"
	"from MRK_Marker,"
	     "GXD_Assay,"
	     "GXD_Specimen,"
	     "GXD_InSituResult,"
	     "GXD_InSituResultImage,"
	     "IMG_ImagePane,"
	     "IMG_Image,"
	     "ACC_Accession\n"
	"where MRK_Marker._Marker_key = GXD_Assay._Marker_key "
	  "and GXD_Assay._Assay_key = GXD_Specimen._Assay_key "
	  "and GXD_Specimen._Specimen_key = GXD_InSituResult._Specimen_key "
	  "and GXD_InSituResult._Result_key = GXD_InSituResultImage._Result_key "
	  "and GXD_InSituResultImage._ImagePane_key = IMG_ImagePane._ImagePane_key "
	  "and IMG_ImagePane._Image_key = IMG_Image._Image_key "
	  "and IMG_Image._Image_key = ACC_Accession._Object_key "
	  "and ACC_Accession.prefixPart = 'PIX:'"
	);
dyStringPrintf(query, "and GXD_Assay._Refs_key = %s", ref);
sr = sqlGetResult(conn, query->string);

fprintf(tab, "#");
fprintf(tab, "gene\t");
fprintf(tab, "sex\t");
fprintf(tab, "age\t");
fprintf(tab, "ageMin\t");
fprintf(tab, "ageMax\t");
fprintf(tab, "paneLabel\t");
fprintf(tab, "fileName\t");
fprintf(tab, "submitId\n");
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *gene = row[0];
    char *sex = row[1];
    char *age = row[2];
    char *ageMin = row[3];
    char *ageMax = row[4];
    char *paneLabel = row[5];
    char *fileKey = row[6];
    char *imageKey = row[7];

    fprintf(tab, "%s\t", gene);
    fprintf(tab, "%s\t", sex);
    fprintf(tab, "%s\t", age);
    fprintf(tab, "%s\t", ageMin);
    fprintf(tab, "%s\t", ageMax);
    fprintf(tab, "%s\t", paneLabel);
    fprintf(tab, "%s\t", gene);
    fprintf(tab, "%s.gif\t", fileKey);
    fprintf(tab, "%s\n", imageKey);
    }
sqlFreeResult(&sr);

carefulClose(&ra);
carefulClose(&tab);
dyStringFree(&query);
}

void submitToDir(struct sqlConnection *conn, char *outDir)
/* Create directory full of visiGeneLoad .ra/.tab files from
 * jackson database connection.  Creates a pair of files for
 * each submission set.   Returns outDir. */
{
struct dyString *query = dyStringNew(0);
struct slName *spec, *specList = sqlQuickList(conn, "select _Specimen_key from GXD_Specimen");
struct slName *ref, *refList = sqlQuickList(conn, "select distinct(_Refs_key) from GXD_Assay");




makeDir(outDir);
uglyf("%d refs\n", slCount(refList));
uglyf("%d specimens\n", slCount(specList));

for (ref = refList; ref != NULL; ref = ref->next)
    {
    char path[PATH_LEN];
    safef(path, sizeof(path), "%s/%s", outDir, ref->name);
    submitRefToFiles(conn, ref->name, path);
    {static int count; if (++count >= 800) uglyAbort("All for now");}
    }

slNameFreeList(&specList);

}

void vgLoadJax(char *jaxDb, char *visiDb)
/* vgLoadJax - Load visiGene database from jackson database. */
{
struct sqlConnection *conn = sqlConnect(jaxDb);
submitToDir(conn, visiDb);

uglyf("vgLoadJax %s %s\n", jaxDb, visiDb);

sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
clJaxDb = argv[1];
clVisiDb = argv[2];
vgLoadJax(clJaxDb, clVisiDb);
return 0;
}
