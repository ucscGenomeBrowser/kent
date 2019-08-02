/* geoToHcaStorm - Convert geo tag storm to hca tag storm.  Both are intermediate representations.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "tagStorm.h"
#include "csv.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "geoToHcaStorm - Convert geo tag storm to hca tag storm.  Both are intermediate representations.\n"
  "usage:\n"
  "   geoToHcaStorm geoIn.tags hcaOut.tags\n"
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

char *accFromEnd(char *url, char endChar, char *accPrefix, char *type)
/* Parse out something like
 *     https://long/url/etc.etc<endChar><accession>
 * into just <accession>  Make sure accession starts with given prefix.
 * Type is just for error reporting */
{
char *s = strrchr(url, endChar);
if (s == NULL || !startsWith(accPrefix, s+1))
    errAbort("Malformed %s URL\n\t%s", type, url);
s += 1;
if (!isSymbolString(s))
    errAbort("accFromEnd got something that doesn't look like accession: %s", s);
return s;
}

void fixAccessions(struct tagStorm *storm, struct tagStanza *stanza, void *context)
/* Convert various URLs containing accessions to just accessions */
{
/* Lets deal with the SRR/SRX issue as well */
struct dyString *srrDy = dyStringNew(0);

struct slPair *pair;
for (pair = stanza->tagList; pair != NULL; pair = pair->next)
    {
    char *name = pair->name;
    if (sameString("series.relation_BioProject", name))
        {
	/* Convert something like https://www.ncbi.nlm.nih.gov/bioproject/PRJNA189204
	 * to PRJNA189204 */
	pair->name = "series.ncbi_bioproject";
	pair->val = accFromEnd(pair->val, '/', "PRJNA", "BioProject");
	}
    else if (sameString("series.relation_SRA", name))
        {
	/* Convert something like https://www.ncbi.nlm.nih.gov/sra?term=SRP018525
	 * to SRP018525 */
	pair->name = "series.sra_project";
	pair->val = accFromEnd(pair->val, '=', "SRP", "SRA");
	}
    else if (sameString("sample.relation_SRA", name))
        {
	/* Convert something like https://www.ncbi.nlm.nih.gov/sra?term=SRX229786
	 * to SRX229786 */
	pair->name = "sample.sra_experiment";
	char *srx = accFromEnd(pair->val, '=', "SRX", "SRA");
	pair->val = srx;
	}
    else if (sameString("sample.relation_BioSample", name))
        {
	/* Convert something like https://www.ncbi.nlm.nih.gov/biosample/SAMN01915417
	 * to SAMN01915417 */
	pair->name = "sample.ncbi_biosample";
	pair->val = accFromEnd(pair->val, '/', "SAMN", "biosample");
	}
    }

if (srrDy->stringSize > 0)
    {
    tagStanzaAppend(storm, stanza, "assay.seq.sra_run", srrDy->string);
    }
dyStringFree(&srrDy);

}

void rFixAccessions(struct tagStorm *storm, struct tagStanza *list)
/* Go through and fix accessions in all stanzas */
{
tagStormTraverse(storm, list, NULL, fixAccessions);
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


#ifdef OLD
void fixProtocols(struct tagStorm *storm, struct tagStanza *stanza, void *context)
/* Convert the various protocols in sample to and array of protocol descriptions and
 * an array of types.  This helps us be compatible with array express, which has
 * an unbounded set of protocol types */
{
struct protocolInfo { char *tag, *type;} info[] = 
    {
       {"sample.growth_protocol", "growth protocol",},
       {"sample.treatment_protocol", "treatment protocol",},
       {"sample.extract_protocol", "nucleic acid library construction protocol",},
    };
struct dyString *protocol = dyStringNew(0);
struct dyString *type = dyStringNew(0);
struct dyString *scratch = dyStringNew(0);

int i;
for (i=0; i<ArraySize(info); ++i)
    {
    char *pro = tagFindVal(stanza, info[i].tag);
    if (pro != NULL)
	{
	char *escaped = csvParseNext(&pro, scratch);
        csvEscapeAndAppend(protocol, escaped);
	csvEscapeAndAppend(type, info[i].type);
	}
    }

if (protocol->stringSize > 0)
    {
    tagStanzaAppend(storm, stanza, "sample.protocols", protocol->string);
    tagStanzaAppend(storm, stanza, "sample.protocol_types", type->string);
    }
dyStringFree(&scratch);
dyStringFree(&protocol);
dyStringFree(&type);
}
#endif /* OLD */

void geoToHcaStorm(char *inTags, char *output)
/* geoToHcaStorm - Convert output of geoToTagStorm to something closer to what the Human Cell 
 *  Atlas wants.. */
{
/* Read in input. */
struct tagStorm *storm = tagStormFromFile(inTags);

/* Fix up all stanzas with various functions */
tagStormTraverse(storm, storm->forest, NULL, fixAccessions);
tagStormTraverse(storm, storm->forest, NULL, fixDates);
tagStormTraverse(storm, storm->forest, NULL, mergeAddresses);

/* Do simple subsitutions. */
tagStormSubArray(storm, substitutions, ArraySize(substitutions));

/* Save results */
tagStormWrite(storm, output, 0);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
geoToHcaStorm(argv[1], argv[2]);
return 0;
}
