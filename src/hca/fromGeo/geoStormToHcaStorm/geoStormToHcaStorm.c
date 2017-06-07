/* geoStormToHcaStorm - Convert output of geoToTagStorm to something closer to what the Human Cell 
 * Atlas wants.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "tagStorm.h"

struct hash *gSrxToSrr;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "geoStormToHcaStorm - Convert output of geoToTagStorm to something closer to what the Human \n"
  "Cell Atlas wants.\n"
  "usage:\n"
  "   geoStormToHcaStorm in.tags srxToSrr.tab out.tags\n"
  "Where in.tags is geoToTagStorm output, srxToSrr.tab is a two column file with\n"
  "NCBI short read archive SRX IDs followed by SRR ones, and out.tags is the output\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

char *weeds[] =  
/* Tags we'll get rid of */
    {
    "platform.contact_country",
    "platform.contact_name",
    "platform.data_row_count",
    "platform.distribution",
    "platform.geo_accession",
    "platform.last_update_date",
    "platform.status",
    "platform.submission_date",
    "platform.technology",
    "platform.title",
    "sample.channel_count",
    "sample.data_row_count",
    "sample.platform_id",
    "sample.series_id",
    "sample.type",
    "series.platform_id",
    "series.platform_taxid",
    "series.sample_id",
    "series.sample_taxid",
    "series.type",
    };


char *substitutions[][2] = 
/* Tags we'll rename */
{
    {"platform.organism", "sample.donor.species"},
    {"platform.taxid", "sample.donor.ncbi_taxon"},
    {"sample.characteristics_age", "sample.donor.age"},
    {"sample.characteristics_bmi", "sample.donor.body_mass_index"},
    {"sample.characteristics_cell_type", "cell.type"},
    {"sample.characteristics_developmental_stage", "sample.donor.life_stage"},
    {"sample.characteristics_ethnicity", "sample.donor.ethnicity"},
    {"sample.characteristics_genotype", "sample.donor.genotype"},
    {"sample.characteristics_exome_capture", "assay.genome.method"},
    {"sample.characteristics_sex", "sample.donor.sex"},
    {"sample.characteristics_Sex", "sample.donor.sex"},
    {"sample.characteristics_strain", "sample.donor.strain"},
    {"sample.characteristics_tissue", "sample.body_part.name"},
    {"sample.contact_address", "project.contact.address"},
    {"sample.contact_city", "project.contact.city"},
    {"sample.contact_country", "project.contact.country"},
    {"sample.contact_department", "project.contact.department"},
    {"sample.contact_email", "project.contact.email"},
    {"sample.contact_institute", "project.contact.institute"},
    {"sample.contact_laboratory", "project.contact.laboratory"},
    {"sample.contact_name", "project.contact.name"},
    {"sample.contact_phone", "project.contact.phone"},
    {"sample.contact_state", "project.contact.state"},
    {"sample.contact_zip/postal_code", "project.contact_postal_code"},
    {"sample.contributor", "project.contributor"},
    {"sample.description", "sample.short_label"},
    {"sample.geo_accession", "sample.geo_sample"},
    {"sample.instrument_model", "assay.seq.machine"},
    {"sample.last_update_date sample.last_update_date"},
    {"sample.library_selection", "assay.rna.primer"},
    {"sample.library_source", "assay.rna.prep"},
    {"sample.library_strategy", "assay.seq.prep"},
    {"sample.molecule", "assay.seq.molecule"},
    {"sample.organism", "sample.donor.species"},
    {"sample.source_name", "sample.body_part.name"},
    {"sample.status", "project.release_status"},
    {"sample.submission_date", "sample.submission_date"},
    {"sample.taxid", "sample.donor.ncbi_taxon"},
    {"sample.title", "sample.long_label"},
    {"series.contact_address", "project.contact.address"},
    {"series.contact_city", "project.contact.city"},
    {"series.contact_country", "project.contact.country"},
    {"series.contact_department", "project.contact.department"},
    {"series.contact_email", "project.contact.email"},
    {"series.contact_institute", "project.contact.institute"},
    {"series.contact_laboratory", "project.contact.laboratory"},
    {"series.contact_name", "project.contact.name"},
    {"series.contact_phone", "project.contact.phone"},
    {"series.contact_state", "project.contact.state"},
    {"series.contact_zip/postal_code", "project.contact.postal_code"},
    {"series.contributor", "project.contributor"},
    {"series.geo_accession", "project.geo_series"},
    {"series.last_update_date", "project.last_update_date"},
    {"series.organism", "sample.donor.species"},
    {"series.overall_design", "project.overall_design"},
    {"series.pubmed_id", "project.pubmed_id"},
    {"series.relation_SubSeries_of", "project.geo_parent_series"},
    {"series.status", "project.release_status"},
    {"series.submission_date", "project.submission_date"},
    {"series.summary", "project.summary"},
    {"series.supplementary_file", "project.supplementary_files"},
    {"series.taxid", "sample.donor.ncbi_taxon"},
    {"series.title", "project.title"},
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
return s+1;
}

void fixAccessions(struct tagStorm *storm, struct tagStanza *stanza)
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
	pair->name = "project.ncbi_bioproject";
	pair->val = accFromEnd(pair->val, '/', "PRJNA", "BioProject");
	}
    else if (sameString("series.relation_SRA", name))
        {
	/* Convert something like https://www.ncbi.nlm.nih.gov/sra?term=SRP018525
	 * to SRP018525 */
	pair->name = "project.sra_project";
	pair->val = accFromEnd(pair->val, '=', "SRP", "SRA");
	}
    else if (sameString("sample.relation_SRA", name))
        {
	/* Convert something like https://www.ncbi.nlm.nih.gov/sra?term=SRX229786
	 * to SRX229786 */
	pair->name = "assay.sra_experiment";
	char *srx = accFromEnd(pair->val, '=', "SRX", "SRA");
	pair->val = srx;

	/* Now make comma separated list of all SRR id's associated */
	struct hashEl *hel = hashLookup(gSrxToSrr, srx);
	if (hel == NULL)
	    errAbort("Can't find SRX tag %s in srxToSra.tab file", srx);
	else
	    dyStringAppend(srrDy, hel->val);
	while ((hel = hashLookupNext(hel)) != NULL)
	    {
	    dyStringAppendC(srrDy, ',');
	    dyStringAppend(srrDy, hel->val);
	    }
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
    tagStanzaAppend(storm, stanza, "assay.sra_run", srrDy->string);
    }
dyStringFree(&srrDy);

}

void rFixAccessions(struct tagStorm *storm, struct tagStanza *list)
/* Go through and fix accessions in all stanzas */
{
struct tagStanza *stanza;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    fixAccessions(storm, stanza);
    rFixAccessions(storm, stanza->children);
    }
}

void fixDates(struct tagStorm *storm, struct tagStanza *stanza)
/* Convert various URLs containing accessions to just accessions */
{
struct slPair *pair;
for (pair = stanza->tagList; pair != NULL; pair = pair->next)
    {
    if (endsWith(pair->name, "_date"))
	geoDateToHcaDate(pair->val, pair->val, strlen(pair->val)+1);
    }
}

void rFixDates(struct tagStorm *storm, struct tagStanza *list)
/* Go through and fix accessions in all stanzas */
{
struct tagStanza *stanza;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    fixDates(storm, stanza);
    rFixDates(storm, stanza->children);
    }
}

void geoStormToHcaStorm(char *inTags, char *inSrxSrr, char *output)
/* geoStormToHcaStorm - Convert output of geoToTagStorm to something closer to what the Human Cell 
 *  Atlas wants.. */
{
struct tagStorm *storm = tagStormFromFile(inTags);
gSrxToSrr = hashTwoColumnFile(inSrxSrr);
hashReverseAllBucketLists(gSrxToSrr);
tagStormWeedArray(storm, weeds, ArraySize(weeds));
rFixAccessions(storm, storm->forest);
rFixDates(storm, storm->forest);
tagStormSubArray(storm, substitutions, ArraySize(substitutions));
tagStormWrite(storm, output, 0);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
geoStormToHcaStorm(argv[1], argv[2], argv[3]);
return 0;
}
