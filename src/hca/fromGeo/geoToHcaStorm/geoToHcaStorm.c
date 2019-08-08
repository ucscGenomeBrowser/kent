/* geoToHcaStorm - Convert geo tag storm to hca tag storm.  Both are intermediate representations.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "tagStorm.h"
#include "fieldedTable.h"
#include "csv.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "geoToHcaStorm - Convert geo tag storm to hca tag storm.  Both are intermediate representations.\n"
  "usage:\n"
  "   geoToHcaStorm geoIn.tags sraTable.tsv seqType hcaOut.tags\n"
  "where:\n"
  "   geoIn.tags is the output of geoToTagStorm\n"
  "   sraTable.tsv contains the fields Experiment_s, Run_s, and possibly more.\n"
  "      normally it is gotten off the NCBI web site\n"
  "   seqType is one of single, paired, or 10x\n"
  "   hcaOut.tags is the output tagStorm\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};


char *substitutions[][2] = 
/* Tags we'll rename.  In this case do some pretty optional cleanup on the sample characteristics, which are going to
 * have to be curated into real tags anyway. */
{
    {"sample.characteristics_Sex", "sample.characteristics_sex"},
    {"sample.characteristics_Age", "sample.characteristics_age"},
};

char *mon[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

char *geoDateToHcaDate(char *geoDate, char *hcaBuf, int hcaBufSize)
/* Convert Mon DD YYYY format date to YYYY-MM-DD format in hcaBuf, which should be
 * at least 11 long. */
{ 
/* Make sure input is right size and make a copy of it for parsing */
int geoDateSize = strlen(geoDate);
if (geoDateSize != 11)
    errAbort("date %s is not in DD Mon YYY format", geoDate);
char geoParsed[geoDateSize+1];
strcpy(geoParsed, geoDate);

/* Parse out three parts */
char *parser = geoParsed;
char *month = nextWord(&parser);
char *day = nextWord(&parser);
char *year = nextWord(&parser);
if (year == NULL)
    errAbort("date %s is not in DD Mon YYY format", geoDate);

/* Look up month */
int monthIx = stringArrayIx(month, mon, sizeof(mon));
if (monthIx < 0)
    errAbort("unrecognized month in %s", geoDate);

safef(hcaBuf, hcaBufSize, "%s-%02d-%s", year, monthIx+1, day);
return hcaBuf;
}


void fixDates(struct tagStorm *storm, struct tagStanza *stanza, void *context)
/* Convert various URLs containing accessions to just accessions */
{
struct slPair *pair;
for (pair = stanza->tagList; pair != NULL; pair = pair->next)
    {
    char *name = pair->name;
    char *val = pair->val;
    if (endsWith(name, "_date"))
	geoDateToHcaDate(val, val, strlen(val)+1);
    else if (sameString(name, "series.status") || sameString(name, "sample.status"))
        {
	char *pat = "Public on ";
	if (startsWith(pat, val))
	    geoDateToHcaDate(val + strlen(pat), val, strlen(val)+1);
	}
    }
}

boolean mergeAddress(struct tagStanza *stanza, struct dyString *dy, char **tags, int tagCount)
/* Look up all of tags in stanza and concatenate together with space separation */
{
char *address = emptyForNull(tagFindLocalVal(stanza, tags[0]));
char *city = emptyForNull(tagFindLocalVal(stanza, tags[1]));
char *state = emptyForNull(tagFindLocalVal(stanza, tags[2]));
char *zip = emptyForNull(tagFindLocalVal(stanza, tags[3]));
char *country = emptyForNull(tagFindLocalVal(stanza, tags[4]));
if (address[0] || city[0] || state[0] || zip[0] || country[0])
    {
    dyStringPrintf(dy, "\"%s; %s, %s %s %s\"", address, city, state, zip, country);
    return TRUE;
    }
else
    return FALSE;
}

void mergeAddresses(struct tagStorm *storm, struct tagStanza *stanza, void *context)
/* Because we want to be international, we merge the address into a single line.
 * Something like 101 Main St. Sausilito CA 94965 USA with the format of
 * number, street, city, state, postal code is not how they represent things
 * in all countries.  Some don't have states.  Some put the number after the street.
 * So we just store the whole thing as a single string.  We do keep the separate
 * components as well since we are data hoarders. */
{

/* Take care of series address */
struct dyString *dy = dyStringNew(0);
char *seriesComponents[] = 
    {
    "series.contact_address", "series.contact_city", "series.contact_state", 
    "series.contact_zip/postal_code", "series.contact_country"
    };
if (mergeAddress(stanza, dy, seriesComponents, ArraySize(seriesComponents)))
    tagStanzaAppend(storm, stanza, "series.contact_full_address", dy->string);

/* Take care of sample address */
dyStringClear(dy);
char *sampleComponents[] = 
    {
    "sample.contact_address", "sample.contact_city", "sample.contact_state", 
    "sample.contact_zip/postal_code", "sample.contact_country"
    };
if (mergeAddress(stanza, dy, sampleComponents, ArraySize(sampleComponents)))
    tagStanzaAppend(storm, stanza, "sample.contact_full_address", dy->string);
dyStringFree(&dy);
}

void geoToHcaStorm(char *inTags, char *inTable, char *seqType, char *output)
/* geoToHcaStorm - Convert output of geoToTagStorm to something closer to what the Human Cell 
 *  Atlas wants.. */
{
/* Read in input. */
struct tagStorm *storm = tagStormFromFile(inTags);

/* Do simple subsitutions. */
tagStormSubArray(storm, substitutions, ArraySize(substitutions));

/* Read in SRA table and make up array of "sra." field names. */
struct fieldedTable *table = fieldedTableFromTabFile(inTable, inTable, NULL, 0);
char *sraFields[table->fieldCount];
int i;
for (i=0; i<table->fieldCount; ++i)
    {
    char *field = table->fields[i];
    char buf[512];
    safef(buf, sizeof(buf), "sra.%s", table->fields[i]);
    if (endsWith(field, "_l") || endsWith(field, "_s"))
        chopSuffixAt(buf, '_');
    sraFields[i] = cloneString(buf);
    }
int joinIx = fieldedTableMustFindFieldIx(table, "Experiment");
int runIx = fieldedTableMustFindFieldIx(table, "Run");

/* Join based on sra_experiment/Experiment_s which contains an SRX123456 type id */
struct hash *joinHash = tagStormIndexExtended(storm, "sample.sra_experiment", FALSE, FALSE);
struct fieldedRow *fr;
for (fr = table->rowList; fr != NULL; fr = fr->next)
    {
    char **row = fr->row;
    char *joinVal = row[joinIx];
    struct hashEl *hel;
    for (hel = hashLookup(joinHash, joinVal); hel != NULL; hel = hashLookupNext(hel))
	{
	struct tagStanza *geoStanza = hel->val;
	struct tagStanza *hcaStanza = tagStanzaNew(storm, geoStanza);
	int i;
	for (i=0; i<table->fieldCount; ++i)
	    {
	    char *val = row[i];
	    if (!isEmpty(val))
		{
		tagStanzaAdd(storm, hcaStanza, sraFields[i], val);	
		}
	    }
	slReverse(&hcaStanza->tagList);

	/* Figure out stuff related to pairing.  One file or two? */
	int fileCount = 2;
	char *pairedEnd = "yes";
	if (sameString("single", seqType))
	    {
	    fileCount = 1;
	    pairedEnd = "no";
	    }
	else if (sameString("paired", seqType))
	    ;
	else
	    errAbort("Unrecognized seqType %s", seqType);

	/* Add in file info */
	char *runName = row[runIx];
	int fileIx;
	for (fileIx = 0; fileIx < fileCount; ++fileIx)
	    {
	    struct tagStanza *fileStanza = tagStanzaNew(storm, hcaStanza);
	    char fileName[FILENAME_LEN];
	    safef(fileName, sizeof(fileName), "%s_%d.fastq.gz", runName, fileIx+1);
	    tagStanzaAdd(storm, fileStanza, "file.name", fileName);
	    tagStanzaAdd(storm, fileStanza, "file.format", ".fastq.gz");
	    tagStanzaAdd(storm, fileStanza, "file.paired_end", pairedEnd);
	    char readIndex[16];
	    safef(readIndex, sizeof(readIndex), "read%d", fileIx+1);
	    tagStanzaAdd(storm, fileStanza, "file.read_index", readIndex);
	    }
	}
    }

/* Fix up all stanzas with various functions */
tagStormTraverse(storm, storm->forest, NULL, fixDates);
tagStormTraverse(storm, storm->forest, NULL, mergeAddresses);

/* Save results */
tagStormWrite(storm, output, 0);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
geoToHcaStorm(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
