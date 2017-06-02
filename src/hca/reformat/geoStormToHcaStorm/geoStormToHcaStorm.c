/* geoStormToHcaStorm - Convert output of geoToTagStorm to something closer to what the Human Cell 
 * Atlas wants.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "tagStorm.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "geoStormToHcaStorm - Convert output of geoToTagStorm to something closer to what the Human \n"
  "Cell Atlas wants.\n"
  "usage:\n"
  "   geoStormToHcaStorm in.tags out.tags\n"
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
    "sample.platform_id	",
    "sample.series_id",
    "sample.supplementary_file",
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
    {"sample.characteristics_cell_type", "cell.type"},
    {"sample.characteristics_ethnicity", "sample.donor.ethnicity"},
    {"sample.characteristics_exome_capture", "assay.genome.method"},
    {"sample.characteristics_strain", "sample.donor.strain"},
    {"sample.contact_address", "project.contact_address"},
    {"sample.contact_city", "project.contact_city"},
    {"sample.contact_country", "project.contact_country"},
    {"sample.contact_department", "project.contact_department"},
    {"sample.contact_email", "project.contact_email"},
    {"sample.contact_institute", "project.contact_institute"},
    {"sample.contact_laboratory", "project.contact_laboratory"},
    {"sample.contact_name", "project.contact_name"},
    {"sample.contact_phone", "project.contact_phone"},
    {"sample.contact_state", "project.contact_state"},
    {"sample.contact_zip/postal_code", "project.contact_postal_code"},
    {"sample.contributor", "project.contributor"},
    {"sample.description", "sample.short_label"},
    {"sample.instrument_model", "assay.seq.machine"},
    {"sample.last_update_date sample.last_update_date"},
    {"sample.library_selection", "assay.rna.primer"},
    {"sample.library_source", "assay.rna.prep"},
    {"sample.library_strategy", "assay.seq.prep"},
    {"sample.molecule", "assay.seq.molecule"},
    {"sample.organism", "sample.donor.species"},
    {"sample.relation_BioSample", "sample.biosample_url"},
    {"sample.relation_SRA", "sample.sra_url"},
    {"sample.source_name", "sample.body_part.name"},
    {"sample.sra_url", "assay.sra_url"},
    {"sample.status", "project.release_status"},
    {"sample.submission_date", "sample.submission_date"},
    {"sample.taxid", "sample.donor.ncbi_taxon"},
    {"sample.title", "sample.long_label"},
    {"series.bioproject_url project.bioproject_url"},
    {"series.biosample_url project.biosample_url"},
    {"series.contact_address", "project.contact_address"},
    {"series.contact_city", "project.contact_city"},
    {"series.contact_country", "project.contact_country"},
    {"series.contact_department", "project.contact_department"},
    {"series.contact_email", "project.contact_email"},
    {"series.contact_institute", "project.contact_institute"},
    {"series.contact_laboratory", "project.contact_laboratory"},
    {"series.contact_name", "project.contact_name"},
    {"series.contact_phone", "project.contact_phone"},
    {"series.contact_state", "project.contact_state"},
    {"series.contact_zip/postal_code", "project.contact_postal_code"},
    {"series.contributor", "project.contributor"},
    {"series.geo_accession", "project.geo_accession"},
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
	pair->name = "project.ncbi_srp";
	pair->val = accFromEnd(pair->val, '=', "SRP", "SRA");
	}
    else if (sameString("sample.relation_SRA", name))
        {
	/* Convert something like https://www.ncbi.nlm.nih.gov/sra?term=SRX229786
	 * to SRX229786 */
	pair->name = "sample.ncbi_srx";
	pair->val = accFromEnd(pair->val, '=', "SRX", "SRA");
	}
    else if (sameString("sample.relation_BioSample", name))
        {
	/* Convert something like https://www.ncbi.nlm.nih.gov/biosample/SAMN01915417
	 * to SAMN01915417 */
	pair->name = "sample.ncbi_biosample";
	pair->val = accFromEnd(pair->val, '/', "SAMN", "biosample");
	}
    }
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

void geoStormToHcaStorm(char *input, char *output)
/* geoStormToHcaStorm - Convert output of geoToTagStorm to something closer to what the Human Cell 
 *  Atlas wants.. */
{
struct tagStorm *storm = tagStormFromFile(input);
tagStormWeedArray(storm, weeds, ArraySize(weeds));
rFixAccessions(storm, storm->forest);
tagStormSubArray(storm, substitutions, ArraySize(substitutions));
tagStormWrite(storm, output, 0);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
geoStormToHcaStorm(argv[1], argv[2]);
return 0;
}
