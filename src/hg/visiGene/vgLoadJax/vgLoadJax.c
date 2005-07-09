/* vgLoadJax - Load visiGene database from jackson database. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "options.h"
#include "portable.h"
#include "obscure.h"
#include "jksql.h"
#include "spDb.h"

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

char *colorFromLabel(char *label, char *gene)
/* Return color from labeling method.   This could be 
 * something in either the GXD_Label or the GXD_VisualizationMethod
 * tables in the jackson database. */
{
if (label == NULL)
    return "";
else if (sameString(label, "Not Applicable"))
    return "";
else if (sameString(label, "Not Specified"))
    return "";
else if (sameString(label, "Alexa Fluor"))	/* Which Alexa fluor? */
    return "";
else if (sameString(label, "Alkaline phosphatase"))
    return "purple";
else if (sameString(label, "Autoradiography"))
    return "";
else if (sameString(label, "Beta-galactosidase"))
    return "blue";
else if (sameString(label, "Biotin"))
    return "";
else if (sameString(label, "Colloidal gold"))
    return "";
else if (sameString(label, "Cy2"))
    return "green";
else if (sameString(label, "Cy3"))
    return "green";
else if (sameString(label, "Cy5"))
    return "red";
else if (sameString(label, "Digoxigenin"))
    return "red";
else if (sameString(label, "Ethidium bromide"))
    return "orange";
else if (sameString(label, "Fluorescein"))
    return "green";
else if (sameString(label, "Horseradish peroxidase"))
    return "purple";
else if (sameString(label, "I125"))
    return "";
else if (sameString(label, "Oregon Green 488"))
    return "green";
else if (sameString(label, "other - see notes"))
    return "";
else if (sameString(label, "P32"))
    return "";
else if (sameString(label, "Phosphorimaging"))
    return "";
else if (sameString(label, "P33"))
    return "";
else if (sameString(label, "Rhodamine"))
    return "red";
else if (sameString(label, "S35"))
    return "";
else if (sameString(label, "SYBR green"))
    return "green";
else if (sameString(label, "Texas Red"))
    return "red";
else 
    {
    warn("Don't know color of %s in %s", label, gene);
    return "";
    }
}

void submitRefToFiles(struct sqlConnection *conn, struct sqlConnection *conn2, char *ref, char *fileRoot)
/* Create a .ra and a .tab file for given reference. */
{
char raName[PATH_LEN], tabName[PATH_LEN];
FILE *ra = NULL, *tab = NULL;
struct dyString *query = dyStringNew(0);
struct sqlResult *sr;
char **row;
char *copyright;
struct slName *list, *el;
boolean gotAny = FALSE;

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
fprintf(ra, "taxon 10090\n");	/* Mus musculus taxon */
fprintf(ra, "fullDir /gbdb/visiGene/jax/full\n");
fprintf(ra, "screenDir /gbdb/visiGene/jax/screen\n");
fprintf(ra, "thumbDir /gbdb/visiGene/jax/thumb\n");
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
	       "ACC_Accession.numericPart as fileKey,"
	       "IMG_Image._Image_key as imageKey,"
	       "GXD_Assay._ProbePrep_key as probePrepKey,"
	       "GXD_Assay._AntibodyPrep_key as antibodyPrepKey,"
	       "GXD_Assay._ReporterGene_key as reporterGeneKey,"
	       "GXD_Assay._Assay_key as assayKey\n"
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
	  "and ACC_Accession.prefixPart = 'PIX:' "
	  "and GXD_Assay._ImagePane_key = 0 "
	);
dyStringPrintf(query, "and GXD_Assay._Refs_key = %s", ref);
sr = sqlGetResult(conn, query->string);

fprintf(tab, "#");
fprintf(tab, "gene\t");
fprintf(tab, "probeColor\t");
fprintf(tab, "sex\t");
fprintf(tab, "age\t");
fprintf(tab, "ageMin\t");
fprintf(tab, "ageMax\t");
fprintf(tab, "paneLabel\t");
fprintf(tab, "fileName\t");
fprintf(tab, "submitId\t");
fprintf(tab, "abName\t");
fprintf(tab, "abTaxon\n");
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
    char *probePrepKey = row[8];
    char *antibodyPrepKey = row[9];
    char *reporterGeneKey = row[10];
    char *assayKey = row[11];
    double calcAge = -1;
    char *probeColor = "";
    char *abName = NULL;
    char abTaxon[32];

    if (age == NULL)
        continue;

    uglyf("assay %s, gene %s, sex %s, age %s, probePrepKey %s, antibodyPrepKey %s, reporterGeneKey %s\n", assayKey, gene, sex, age, probePrepKey, antibodyPrepKey, reporterGeneKey);
    /* Massage sex */
        {
	if (sameString(sex, "Male"))
	    sex = "male";
	else if (sameString(sex, "Female"))
	    sex = "female";
	else
	    sex = "";
	}

    /* Massage age */
	{
	char *embryoPat = "embryonic day ";
	char *newbornPat = "postnatal newborn";
	char *dayPat = "postnatal day ";
	char *weekPat = "postnatal week ";
	char *adultPat = "postnatal adult";
	double calcMinAge = atof(ageMin);
	double calcMaxAge = atof(ageMax);
	double mouseBirthAge = 21.0;
	double mouseAdultAge = 63.0;	/* Relative to conception, not birth */

	if (age[0] == 0)
	    {
	    warn("age null, ageMin %s, ageMax %s\n", ageMin, ageMax);
	    calcAge = (calcMinAge + calcMaxAge) * 0.5;
	    }
	else if (startsWith(embryoPat, age))
	    calcAge = atof(age+strlen(embryoPat));
	else if (sameString(newbornPat, age))
	    calcAge = mouseBirthAge;
	else if (startsWith(dayPat, age))
	    calcAge = atof(age+strlen(dayPat)) + mouseBirthAge;
        else if (startsWith(weekPat, age))
	    calcAge = 7.0 * atof(age+strlen(weekPat)) + mouseBirthAge;
	else if (sameString(adultPat, age) && calcMaxAge - calcMinAge > 1000 
		&& calcMinAge < 365)
	    calcAge = 365;	/* Most adult mice are relatively young */
	else
	    {
	    warn("Calculating age from %s\n", age);
	    calcAge = (calcMinAge + calcMaxAge) * 0.5;
	    }
	if (calcAge < calcMinAge)
	    calcAge = calcMinAge;
	if (calcAge > calcMaxAge)
	    calcAge = calcMaxAge;
	}
    
    /* Massage probeColor */
        {
	if (!sameString(reporterGeneKey, "0"))
	    {
	    char *name = NULL;
	    dyStringClear(query);
	    dyStringPrintf(query, 
	    	"select term from VOC_Term where _Term_key = %s", 
	    	reporterGeneKey);
	    name = sqlQuickString(conn2, query->string);
	    if (name == NULL)
	        warn("Can't find _ReporterGene_key %s in VOC_Term", 
			reporterGeneKey);
	    else if (sameString(name, "GFP"))
	        probeColor = "green";
	    else if (sameString(name, "lacZ"))
	        probeColor = "blue";
	    else 
	        warn("Don't know color of reporter gene %s", name);
	    freez(&name);
	    }
	if (!sameString(probePrepKey, "0"))
	    {
	    char *name = NULL;
	    dyStringClear(query);
	    dyStringPrintf(query, 
	      "select GXD_VisualizationMethod.visualization "
	      "from GXD_VisualizationMethod,GXD_ProbePrep "
	      "where GXD_ProbePrep._ProbePrep_key = %s "
	      "and GXD_ProbePrep._Visualization_key = GXD_VisualizationMethod._Visualization_key"
	      , probePrepKey);
	    name = sqlQuickString(conn2, query->string);
	    if (name == NULL)
	        warn("Can't find visualization from _ProbePrep_key %s", probePrepKey);
	    probeColor = colorFromLabel(name, gene);
	    freez(&name);
	    if (probeColor[0] == 0)
	        {
		dyStringClear(query);
		dyStringPrintf(query, 
			"select GXD_Label.label from GXD_Label,GXD_ProbePrep "
		        "where GXD_ProbePrep._ProbePrep_key = %s " 
			"and GXD_ProbePrep._Label_key = GXD_Label._Label_key"
		        , probePrepKey);
		name = sqlQuickString(conn2, query->string);
		if (name == NULL)
		    warn("Can't find label from _ProbePrep_key %s", 
		    	probePrepKey);
		probeColor = colorFromLabel(name, gene);
		}
	    freez(&name);
	    }
	if (!sameString(antibodyPrepKey, "0") && probeColor[0] == 0 )
	    {
	    char *name = NULL;
	    dyStringClear(query);
	    dyStringPrintf(query, 
		  "select GXD_Label.label from GXD_Label,GXD_AntibodyPrep "
		  "where GXD_AntibodyPrep._AntibodyPrep_key = %s "
		  "and GXD_AntibodyPrep._Label_key = GXD_Label._Label_key"
		  , antibodyPrepKey);
	    name = sqlQuickString(conn2, query->string);
	    if (name == NULL)
		warn("Can't find label from _AntibodyPrep_key %s", antibodyPrepKey);
	    probeColor = colorFromLabel(name, gene);
	    freez(&name);
	    }
	}

    /* Get abName, abTaxon */
    abTaxon[0] = 0;
    if (!sameString(antibodyPrepKey, "0"))
        {
	struct sqlResult *sr = NULL;
	int orgKey = 0;
	char **row;
	dyStringClear(query);
	dyStringPrintf(query, 
		"select antibodyName,_Organism_key "
		"from GXD_AntibodyPrep,GXD_Antibody "
		"where GXD_AntibodyPrep._AntibodyPrep_key = %s "
		"and GXD_AntibodyPrep._Antibody_key = GXD_Antibody._Antibody_key"
		, antibodyPrepKey);
	sr = sqlGetResult(conn2, query->string);
	row = sqlNextRow(sr);
	if (row != NULL)
	    {
	    abName = cloneString(row[0]);
	    orgKey = atoi(row[1]);
	    }
	sqlFreeResult(&sr);

	if (orgKey > 0)
	    {
	    struct sqlConnection *sp = sqlConnect("uniProt");
	    char *latinName = NULL, *commonName = NULL;
	    int spTaxon = 0;
	    dyStringClear(query);
	    dyStringPrintf(query, "select latinName from MGI_Organism "
	                          "where _Organism_key = %d", orgKey);
	    latinName = sqlQuickString(conn2, query->string);
	    if (latinName != NULL && !sameString(latinName, "Not Specified"))
		{
		char *e = strchr(latinName, '/');
		if (e != NULL) 
		   *e = 0;	/* Chop off / and after. */
		spTaxon = spBinomialToTaxon(sp, latinName);
		}
	    else
	        {
		dyStringClear(query);
		dyStringPrintf(query, "select commonName from MGI_Organism "
	                          "where _Organism_key = %d", orgKey);
		commonName = sqlQuickString(conn2, query->string);
		if (commonName != NULL && !sameString(commonName, "Not Specified"))
		    {
		    spTaxon = spCommonToTaxon(sp, commonName);
		    }
		}
	    if (spTaxon != 0)
	        safef(abTaxon, sizeof(abTaxon), "%d", spTaxon);
	    freez(&latinName);
	    freez(&commonName);
	    sqlDisconnect(&sp);
	    }
	}
    if (abName == NULL)
        abName = cloneString("");

    fprintf(tab, "%s\t", gene);
    fprintf(tab, "%s\t", probeColor);
    fprintf(tab, "%s\t", sex);
    fprintf(tab, "%3.2f\t", calcAge);
    fprintf(tab, "%s\t", ageMin);
    fprintf(tab, "%s\t", ageMax);
    fprintf(tab, "%s\t", paneLabel);
    fprintf(tab, "%s.gif\t", fileKey);
    fprintf(tab, "%s\t", imageKey);
    fprintf(tab, "%s\t", abName);
    fprintf(tab, "%s\n", abTaxon);
    gotAny = TRUE;
    freez(&abName);
    }
sqlFreeResult(&sr);

carefulClose(&ra);
carefulClose(&tab);
if (!gotAny)
    {
    remove(raName);
    remove(tabName);
    }
dyStringFree(&query);
}

void submitToDir(struct sqlConnection *conn, struct sqlConnection *conn2, char *outDir)
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
    submitRefToFiles(conn, conn2, ref->name, path);
    {static int count; if (++count >= 3000) uglyAbort("All for now");}
    }

slNameFreeList(&specList);
}

void vgLoadJax(char *jaxDb, char *visiDb)
/* vgLoadJax - Load visiGene database from jackson database. */
{
struct sqlConnection *conn = sqlConnect(jaxDb);
struct sqlConnection *conn2 = sqlConnect(jaxDb);
submitToDir(conn, conn2, visiDb);

uglyf("vgLoadJax %s %s\n", jaxDb, visiDb);

sqlDisconnect(&conn2);
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
