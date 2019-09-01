/* hcatTabUpdate - take the tabToTabDir result of the geo/sra import
 * and unpack a few fields that are just too hard in strex. 
 * Put results in an output dir. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "fieldedTable.h"
#include "portable.h"
#include "csv.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hcatTabUpdate - Update the hcat database given a tab seperated input and output dir.\n"
  "hcatTabUpdate - take the tabToTabDir result of the geo/sra import\n"
  "and unpack a few fields that are just too hard in strex. \n"
  "Put results in an output dir.\n" 
  "usage:\n"
  "   hcatTabUpdate inDir outDir\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void addIfReal(int fieldIx, char *oldFieldNames[], 
    char *newFields[], int newIxs[], int maxNewCount,int *pCurCount)
/* If fieldIx is positive we consider it real anc add it to the newFields */
{
if (fieldIx > 0)
    {
    char *oldName = cloneString(oldFieldNames[fieldIx]);
    char *newName = oldName;
    char *prefix = "contact_";
    if (startsWith(prefix, oldName))
	 {
         newName += strlen(prefix);;
	 }
    int curCount = *pCurCount;
    if (curCount >= maxNewCount)
       errAbort("Too many fields in addIfRead on %s, %d max", oldName, curCount);
    newIxs[curCount] = fieldIx;
    newFields[curCount] = newName;
    *pCurCount = curCount+1;
    }
}

struct slName *uniqVals(struct fieldedTable *table, char *field)
/* Return list of all unique values in the field */
{
struct dyString *scratch = dyStringNew(0);
int fieldIx = fieldedTableFindFieldIx(table, field);
if (fieldIx < 0)
    return NULL;
struct hash *uniqHash = hashNew(0);
struct slName *valList = NULL;
struct fieldedRow *fr;
for (fr = table->rowList; fr != NULL; fr = fr->next)
    {
    char *inTsv = fr->row[fieldIx];
    char *val;
    while ((val = csvParseNext(&inTsv, scratch)) != NULL)
	{
	if (hashLookup(uniqHash, val) == NULL)
	    {
	    hashAdd(uniqHash, val, NULL);
	    slNameAddHead(&valList, val);
	    }
	}
    }
slReverse(&valList);
hashFree(&uniqHash);
dyStringFree(&scratch);
return valList;
}

char *slNameToCsv(struct slName *list)
/* Convert slNames to a long string */
{
struct dyString *dy = dyStringNew(0);
struct slName *el;
for (el = list; el != NULL; el = el->next)
    csvEscapeAndAppend(dy, el->name);
return dyStringCannibalize(&dy);
}


struct fieldedTable *makeContributors(struct fieldedTable *inProject)
/* Make a fielded table from project contact info and contributors list */
{
char **projectRow = inProject->rowList->row;

/* Make the contributors list in two pieces first off of the contact */
int contact_name = fieldedTableFindFieldIx(inProject, "contact_name");
int contact_email = fieldedTableFindFieldIx(inProject, "contact_email");
int contact_phone = fieldedTableFindFieldIx(inProject, "contact_phone");
int contact_department = fieldedTableFindFieldIx(inProject, "contact_department");
int contact_institute = fieldedTableFindFieldIx(inProject, "contact_institute");
int contact_address = fieldedTableFindFieldIx(inProject, "contact_address");
int contact_city = fieldedTableFindFieldIx(inProject, "contact_city");
int contact_country = fieldedTableFindFieldIx(inProject, "contact_country");
int contact_zip_postal_code = fieldedTableFindFieldIx(inProject, "contact_zip_postal_code");


/* Figure out which contact data we actually have */
const int maxContacts = 32;
char *contactFields[maxContacts+1];  // An extra for the project_role
int contactIx[maxContacts];
int realFieldCount = 0;
char **oldFields = inProject->fields;
addIfReal(contact_name, oldFields, contactFields, contactIx, maxContacts, &realFieldCount);
addIfReal(contact_email, oldFields, contactFields, contactIx, maxContacts, &realFieldCount);
addIfReal(contact_phone, oldFields, contactFields, contactIx, maxContacts, &realFieldCount);
addIfReal(contact_department, oldFields, contactFields, contactIx, maxContacts, &realFieldCount);
addIfReal(contact_institute, oldFields, contactFields, contactIx, maxContacts, &realFieldCount);
addIfReal(contact_address, oldFields, contactFields, contactIx, maxContacts, &realFieldCount);
addIfReal(contact_city, oldFields, contactFields, contactIx, maxContacts, &realFieldCount);
addIfReal(contact_country, oldFields, contactFields, contactIx, maxContacts, &realFieldCount);
addIfReal(contact_zip_postal_code, oldFields, 
    contactFields, contactIx, maxContacts, &realFieldCount);
contactFields[realFieldCount] = "project_role";
realFieldCount += 1;
uglyf("Got %d real contact fields\n", realFieldCount);

/* Make contributor output table.  The first row of it will be seeded with the contact.
 * We can fill out names, but not other info on the other contributors, who will make
 * up the rest of the rows. */
struct fieldedTable *contributors = fieldedTableNew("contributors", contactFields, 
    realFieldCount);
contributors->startsSharp = inProject->startsSharp;

/* Make up first row from contacts */
char *outVals[realFieldCount];
int outIx;
struct dyString *scratch = dyStringNew(0);
for (outIx=0; outIx<realFieldCount-1; ++outIx)
    {
    char *inTsv = projectRow[contactIx[outIx]];
    char *inVal = emptyForNull(cloneString(csvParseNext(&inTsv, scratch)));
    outVals[outIx] = inVal;
    }
outVals[outIx] = "lab contact";
char *contactName = cloneString(outVals[0]);
fieldedTableAdd(contributors, outVals, realFieldCount, 1);

/* Unroll the contributors field  into further rows*/
for (outIx=0; outIx<realFieldCount; ++outIx)
    outVals[outIx] = "";	// Empty out all rows.
int inContribIx = fieldedTableMustFindFieldIx(inProject, "contributors");
int outContribIx = fieldedTableMustFindFieldIx(contributors, "name");
char *inTsv = projectRow[inContribIx];
char *oneVal;
while ((oneVal = csvParseNext(&inTsv, scratch)) != NULL)
    {
    if (differentString(oneVal, contactName))  // We already got the contact as a contributor
	{
	outVals[outContribIx] = cloneString(oneVal);
	outVals[realFieldCount-1] = "contributor";
	fieldedTableAdd(contributors, outVals, realFieldCount, contributors->rowCount+1);
	}
    }
return contributors;
}

char *lookupSpecies(char *taxon)
/* Some day we may query a decent database, for now
 * just have some of the most common */
{
if (sameString(taxon, "9606")) return "human";
if (sameString(taxon, "10090")) return "mouse";
if (sameString(taxon, "10116")) return "rat";
if (sameString(taxon, "7955")) return "zebrafish";
if (sameString(taxon, "7227")) return "fly";
if (sameString(taxon, "6239")) return "worm";
if (sameString(taxon, "4932")) return "yeast";
errAbort("Unknown taxon %s", taxon);
return NULL;
}

char *taxonsToSpecies(char *inVal, struct dyString *scratch)
/* Convert a comma separated list of taxons in inVal to a
 * comma separated species list, using scratch. */
{
struct dyString *result = dyStringNew(64);
char *taxon;
char *s = inVal;
while ((taxon = csvParseNext(&s, scratch)) != NULL)
    {
    char *species = lookupSpecies(taxon);
    csvEscapeAndAppend(result, species);
    }
return dyStringCannibalize(&result);
}

void addListFieldIfNonempty(char *field, struct slName *list,
    char *newFields[], char *newVals[], int maxNewCount,int *pCurCount)
/* Add field to newFields if list is non-empty, taking care not to go past end. */
{
if (list != NULL)
    {
    int curCount = *pCurCount;
    if (curCount >= maxNewCount)
       errAbort("Too many fields in addListFieldIfNonempty on %s, %d max", field, curCount);
    newFields[curCount] = field;
    newVals[curCount] = slNameToCsv(list);
    *pCurCount = curCount+1;
    }
}


struct fieldedTable *makeProject(struct fieldedTable *inProject, struct fieldedTable *inSample)
/* Make output project table.  This is the big one - 35 fields now
 * probably twice that by the time HCA is done.  Fortunately we only need
 * to deal with some of the fields and it only has one row. */
{
char **inFields = inProject->fields;
char **inRow = inProject->rowList->row;
int inFieldCount = inProject->fieldCount;
struct dyString *scratch = dyStringNew(0);

int outFieldMax = inFieldCount + 16;  // Mostly we remove fields but we do add a few.  Gets checked
char *outFields[outFieldMax];  
char *outRow[outFieldMax];
int outFieldCount = 0;

/* First we make up the basics of the outProject table.  Mostly this is just
 * passing through from the inProject, but there's exceptions like contacts. */
int inIx;
for (inIx=0; inIx<inFieldCount; ++inIx)
    {
    /* Fetch input name and value */
    char *inName = inFields[inIx];
    char *inVal = inRow[inIx];

    /* Go through list of input fields we tweak slightly */
    if (sameString("taxons", inName))
        {
	inName = "species";
	inVal = taxonsToSpecies(inVal, scratch);
	}

    /* Output all the ones we haven't dealt with already */
    if (!startsWith("contact_", inName) && !sameString("contributors", inName))
        {
	outFields[outFieldCount] = inName;
	outRow[outFieldCount] = inVal;
	++outFieldCount;
	}
    }

/* Add the fields we scan and merge from sample at end */
struct slName *organList = uniqVals(inSample, "organ");
struct slName *organPartList = uniqVals(inSample, "organ_part");
struct slName *assayTypeList = uniqVals(inSample, "assay_type");
struct slName *diseaseList = uniqVals(inSample, "disease");
addListFieldIfNonempty("organ", organList, outFields, outRow, outFieldMax, &outFieldCount);
addListFieldIfNonempty("organ_part", organPartList, outFields, outRow, outFieldMax, &outFieldCount);
addListFieldIfNonempty("assay_type", assayTypeList, outFields, outRow, outFieldMax, &outFieldCount);
addListFieldIfNonempty("disease", diseaseList, outFields, outRow, outFieldMax, &outFieldCount);

struct fieldedTable *outTable = fieldedTableNew("project", outFields, outFieldCount);
outTable->startsSharp = inProject->startsSharp;
fieldedTableAdd(outTable, outRow, outFieldCount, 1);
dyStringFree(&scratch);
return outTable;
}

char *fieldedTableLookupNamedFieldInRow(struct fieldedTable *table, char *field, char **row)
/* Look up field by name,  not as effient as by index but what the heck. */
{
int ix = fieldedTableFindFieldIx(table, field);
if (ix < 0)
    return NULL;
return row[ix];
}

struct fieldedTable *makeLab(struct fieldedTable *inProject)
/* If there's a lab field we make a lab table and seed it with the contacts. */
{
int labIx = fieldedTableFindFieldIx(inProject, "lab");
if (labIx >= 0)
    {
    char **inRow = inProject->rowList->row;
    char *short_name = inRow[labIx];
    char *contact = fieldedTableLookupNamedFieldInRow(inProject, "contact_name", inRow);
    char *institute = emptyForNull(fieldedTableLookupNamedFieldInRow(inProject, "contact_institute", inRow));
    char labName[256];
    if (strlen(short_name) < 20)  // Unlikely to be unique, may cause trouble
	safef(labName, sizeof(labName), "%s %s", short_name, institute);
    else
        safef(labName, sizeof(labName), "%s", short_name);
    labName[50] = 0;  // not too long

    char *outFields[3] = {"short_name", "institute", "contact"};
    struct fieldedTable *labTable = fieldedTableNew("lab", outFields, ArraySize(outFields));
    char *outRow[3] = {labName, institute, contact};
    fieldedTableAdd(labTable, outRow, ArraySize(outRow), 1);
    return labTable;
    }
else
    return NULL;
}

void hcatTabUpdate(char *inDir, char *outDir)
/* hcatTabUpdate - Update the hcat database given a tab seperated input and output dir. */
{
// We are actually just looking for specific files in inDir. */

/* Load up input projects table */
char *projectFile = "hcat_project.tsv";
char inPath[PATH_LEN];
safef(inPath, sizeof(inPath), "%s/%s", inDir, projectFile);
char *projectRequired[] = {"short_name", "contact_name"};
struct fieldedTable *inProject = fieldedTableFromTabFile(inPath, inPath, 
    projectRequired, ArraySize(projectRequired));
uglyf("Got %d rows, %d columns from %s\n", 
    inProject->rowCount, inProject->fieldCount, inProject->name);

/* Load up samples table */
char *sampleFile = "hcat_sample.tsv";
safef(inPath, sizeof(inPath), "%s/%s", inDir, sampleFile);
char *sampleRequired[] = {"short_name",};
struct fieldedTable *inSample = fieldedTableFromTabFile(inPath, inPath, 
    sampleRequired, ArraySize(sampleRequired));
uglyf("Got %d rows, %d columns from %s\n", 
    inSample->rowCount, inSample->fieldCount, inSample->name);


/* Make sure inProject table makes sense by having exactly one row */
if (inProject->rowCount != 1)
    errAbort("Expected one row in %s, got %d\n", projectFile, inProject->rowCount);

struct fieldedTable *outContributor = makeContributors(inProject);
struct fieldedTable *outProject = makeProject(inProject, inSample);
struct fieldedTable *outLab = makeLab(inProject);

/* Write output */
makeDirsOnPath(outDir);
char outPath[PATH_LEN];
safef(outPath, sizeof(outPath), "%s/%s", outDir, "contributors.tsv");
fieldedTableToTabFile(outContributor, outPath);
safef(outPath, sizeof(outPath), "%s/%s", outDir, "project.tsv");
fieldedTableToTabFile(outProject, outPath);
if (outLab != NULL)
    {
    safef(outPath, sizeof(outPath), "%s/%s", outDir, "lab.tsv");
    fieldedTableToTabFile(outLab, outPath);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
hcatTabUpdate(argv[1], argv[2]);
return 0;
}
