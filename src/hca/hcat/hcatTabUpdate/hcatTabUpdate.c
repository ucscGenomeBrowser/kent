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

char *fieldedTableLookupNamedFieldInRow(struct fieldedTable *table, char *field, char **row)
/* Look up field by name,  not as effient as by index but what the heck. */
{
int ix = fieldedTableFindFieldIx(table, field);
if (ix < 0)
    return NULL;
return row[ix];
}

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
struct dyString *csvScratch = dyStringNew(0);

/* Make the contributors list in two pieces first off of the contact */
int contact_email = fieldedTableFindFieldIx(inProject, "contact_email");
int contact_phone = fieldedTableFindFieldIx(inProject, "contact_phone");
//int contact_department = fieldedTableFindFieldIx(inProject, "contact_department");
//int contact_institute = fieldedTableFindFieldIx(inProject, "contact_institute");
//int contact_address = fieldedTableFindFieldIx(inProject, "contact_address");
//int contact_city = fieldedTableFindFieldIx(inProject, "contact_city");
//int contact_country = fieldedTableFindFieldIx(inProject, "contact_country");
//int contact_zip_postal_code = fieldedTableFindFieldIx(inProject, "contact_zip_postal_code");


/* Figure out which contact data we actually have */
const int maxContacts = 32;
char *contactFields[maxContacts+1];  // An extra for the project_role
int contactIx[maxContacts];
char **oldFields = inProject->fields;

// Add contact name field separately from the rest.  We know it's there since it's a 
// required field, and also we need to decorate it's name
contactFields[0] = "?name";
contactIx[0] = fieldedTableMustFindFieldIx(inProject, "contact_name");
int realFieldCount = 1;

// The rest of the contact pieces are added just conditionally
addIfReal(contact_email, oldFields, contactFields, contactIx, maxContacts, &realFieldCount);
addIfReal(contact_phone, oldFields, contactFields, contactIx, maxContacts, &realFieldCount);
//addIfReal(contact_department, oldFields, contactFields, contactIx, maxContacts, &realFieldCount);
//#addIfReal(contact_institute, oldFields, contactFields, contactIx, maxContacts, &realFieldCount);
//#addIfReal(contact_address, oldFields, contactFields, contactIx, maxContacts, &realFieldCount);
//addIfReal(contact_city, oldFields, contactFields, contactIx, maxContacts, &realFieldCount);
//addIfReal(contact_country, oldFields, contactFields, contactIx, maxContacts, &realFieldCount);
//addIfReal(contact_zip_postal_code, oldFields, 
//    contactFields, contactIx, maxContacts, &realFieldCount);
contactFields[realFieldCount] = "project_role";
realFieldCount += 1;

/* Make contributor output table.  The first row of it will be seeded with the contact.
 * We can fill out names, but not other info on the other contributors, who will make
 * up the rest of the rows. */
struct fieldedTable *contributors = fieldedTableNew("contributor", contactFields, 
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
    outVals[outIx] = cloneString(csvEscapeToDyString(csvScratch, inVal));
    }
outVals[outIx] = "lab contact";
char *contactName = cloneString(outVals[0]);
fieldedTableAdd(contributors, outVals, realFieldCount, 1);

/* Unroll the contributors field  into further rows*/
for (outIx=0; outIx<realFieldCount; ++outIx)
    outVals[outIx] = "";	// Empty out all rows.
int inContribIx = fieldedTableMustFindFieldIx(inProject, "contributors");
int outContribIx = fieldedTableMustFindFieldIx(contributors, "?name");
char *inTsv = projectRow[inContribIx];
char *oneVal;
while ((oneVal = csvParseNext(&inTsv, scratch)) != NULL)
    {
    char *escaped = csvEscapeToDyString(csvScratch, oneVal);
    if (differentString(escaped, contactName))  // We already got the contact as a contributor
	{
	outVals[outContribIx] = escaped;
	outVals[realFieldCount-1] = "contributor";
	fieldedTableAdd(contributors, outVals, realFieldCount, contributors->rowCount+1);
	}
    }
dyStringFree(&csvScratch);
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
    char fieldName[256];
    char *strippedField = cloneString(field);
    stripChar(strippedField, '_');
    safef(fieldName, sizeof(fieldName), 
	"@@%s@id@hcat_project_%s@project_id@%s_id@hcat_%s.short_name@id", 
	field, strippedField, field, strippedField);
    newFields[curCount] = cloneString(fieldName);
    newVals[curCount] = slNameToCsv(list);
    *pCurCount = curCount+1;
    freez(&strippedField);
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
	// its many-to-many, whoot!
	inName = "@@species@id@hcat_project_species@project_id@species_id@hcat_species@common_name@id";  
	inVal = taxonsToSpecies(inVal, scratch);
	}

    /* We might modify names of some fields */
    char nameBuf[128];
    if (sameString("state_reached", inName) || sameString("cur_state", inName))
        {
	safef(nameBuf, sizeof(nameBuf), "@%s_id@hcat_project_state.state@id", inName);
	inName = cloneString(nameBuf);
	}
    else if (sameString("consent", inName) || sameString("effort", inName))
        {
	safef(nameBuf, sizeof(nameBuf), "@%s_id@hcat_%s.short_name@id", inName, inName);
	inName = cloneString(nameBuf);
	}

    /* Output all the ones we haven't dealt with already or will deal with later */
    if (!startsWith("contact_", inName) && !sameString("contributors", inName))
        {
	outFields[outFieldCount] = inName;
	outRow[outFieldCount] = inVal;
	++outFieldCount;
	}
    }

/* Add in contributors as a multi to multi field */
outFields[outFieldCount] = "@@contributors@hcat_project_contributors@id@project_id@contributor_id@hcat_contributor@name@id";
outRow[outFieldCount] = fieldedTableLookupNamedFieldInRow(inProject, "contributors", inRow);

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

struct fieldedTable *makeLab(struct fieldedTable *inProject)
/* If there's a lab field we make a lab table and seed it with the contacts. */
{
int labIx = fieldedTableFindFieldIx(inProject, "lab");
if (labIx >= 0)
    {
    char **inRow = inProject->rowList->row;
    char *short_name = inRow[labIx];
    char *contact = fieldedTableLookupNamedFieldInRow(inProject, "contact_name", inRow);
    char *contributors = fieldedTableLookupNamedFieldInRow(inProject, "contributors", inRow);
    char *institute = emptyForNull(fieldedTableLookupNamedFieldInRow(inProject, "contact_institute", inRow));
    char labName[256];
    if (strlen(short_name) < 20)  // Unlikely to be unique, may cause trouble
	safef(labName, sizeof(labName), "%s %s", short_name, institute);
    else
        safef(labName, sizeof(labName), "%s", short_name);
    labName[50] = 0;  // not too long

    char *outFields[4] = {"?short_name", "institution", "@contact_id@hcat_contributor@name@id", 
	"@@contributors@id@hcat_lab_contributors@lab_id@contributor_id@hcat_contributor@name@id"};
    struct fieldedTable *labTable = fieldedTableNew("lab", outFields, ArraySize(outFields));
    char *outRow[4] = {labName, institute, contact, contributors};
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
char *projectRequired[] = {"short_name", "contact_name", "contributors"};
struct fieldedTable *inProject = fieldedTableFromTabFile(inPath, inPath, 
    projectRequired, ArraySize(projectRequired));

/* Load up samples table */
char *sampleFile = "hcat_sample.tsv";
safef(inPath, sizeof(inPath), "%s/%s", inDir, sampleFile);
char *sampleRequired[] = {"short_name",};
struct fieldedTable *inSample = fieldedTableFromTabFile(inPath, inPath, 
    sampleRequired, ArraySize(sampleRequired));


/* Make sure inProject table makes sense by having exactly one row */
if (inProject->rowCount != 1)
    errAbort("Expected one row in %s, got %d\n", projectFile, inProject->rowCount);

struct fieldedTable *outContributor = makeContributors(inProject);
struct fieldedTable *outProject = makeProject(inProject, inSample);
struct fieldedTable *outLab = makeLab(inProject);

/* Write output from lowest level to highest level tables. */
makeDirsOnPath(outDir);
char outPath[PATH_LEN];
safef(outPath, sizeof(outPath), "%s/hcat_%s", outDir, "contributor.tsv");
fieldedTableToTabFile(outContributor, outPath);

if (outLab != NULL)
    {
    safef(outPath, sizeof(outPath), "%s/hcat_%s", outDir, "lab.tsv");
    fieldedTableToTabFile(outLab, outPath);
    }

safef(outPath, sizeof(outPath), "%s/hcat_%s", outDir, "project.tsv");
fieldedTableToTabFile(outProject, outPath);
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
