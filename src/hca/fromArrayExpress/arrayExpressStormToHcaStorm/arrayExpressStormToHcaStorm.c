/* arrayExpressStormToHcaStorm - Convert output of arrayExpressToTagStorm to somethng closer to 
 * what the Human Cell Atlas wants.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "tagStorm.h"
#include "csv.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "arrayExpressStormToHcaStorm - Convert output of arrayExpressToTagStorm to somethng closer to\n"
  "what the Human Cell Atlas wants.\n"
  "usage:\n"
  "   arrayExpressStormToHcaStorm in.tags out.tags\n"
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
    "idf.SDRF_File",			// This file is embedded in tagStorm
    "idf.Comment_GEOReleaseDate",	// Redundant with Public_Release_DATE
    "idf.Comment_Submitted_Name",	// Redundant with Investigation_Title
    "idf.Comment_SequenceDataURI",	// Takes you to web page can get to other ways
    "idf.Experimental_Design_Term_Source_REF",  // Can be deduced from related field
    "idf.Experimental_Factor_Term_Source_REF",	// Can be deduced from related field
    "idf.Experimental_Factor_Term_Accession_Number", // Sparsely used and uninformative
    "idf.Experimental_Factor_Type",  // Redundant with idf.Experimental_Factor_Name
    "sdrf.Characteristics_organism_Term_Source_REF",  // Can be deduced from related field
    "sdrf.Derived_Array_Data_File",  // Redundant with sdrf.Comment_Derived_ArrayExpress_FTP_file
    "sdrf.Scan_Name",		     // Redundant with sdrf.Comment_SUBMITTED_FILE_NAME
    "sdrf.Extract_Name",	     // Redundant with sdrf.Source_Name
    "sdrf.Technology_Type",          // Always "sequencing assay" which is captured elsewhere
    "sdrf.Characteristics_age_Unit_Term_Accession_Number",  // Overkill for day/week/month/year
    "sdrf.Characteristics_age_Unit_Term_Source_REF",  // Overkill for day/week/month/year
    "idf.Comment_SecondaryAccession", // Actually we do capture this before weeding
    };


char *cellTypeTags[] =
    {
    "sdrf.Characteristics_cell_type", 
    "sdrf.Factor_Value_cell_type",   // Redundant with Characteristics_cell_type
    };

char *diseaseTags[] =
    {
    "sdrf.Characteristics_disease", 
    "sdrf.Factor_Value_disease",     // Redundant with Characteristics_disease
    };

char *machineTags[] = 
/* Various fields array.seq.machine might be found in. */
     {
    "sdrf.Comment_INSTRUMENT_MODEL",
    "idf.Protocol_Hardware", 
    "sdrf.Comment_Platform_title",   
    };

char *substitutions[][2] = 
/* Tags we'll rename */
{
{"idf.Comment_AEExperimentType", "project.array_express.experiment_type",}, 
{"idf.Comment_AdditionalFile_Data", "project.supplementary_files",}, 
{"idf.Comment_AdditionalFile_txt", "reformat.Comment_AdditionalFile_txt",}, 
{"idf.Comment_ArrayExpressAccession", "project.array_express.accession",}, 
{"idf.Comment_ArrayExpressSubmissionDate", "project.submission_date",}, 
{"idf.Comment_GEOLastUpdateDate", "project.last_update_date",}, 
{"idf.Comment_GEOReleaseDate", "project.release_status",}, 
{"idf.Comment_RelatedExperiment", "project.array_express.related_experiment",}, 
{"idf.Experiment_Description", "project.summary",}, 
{"idf.Experimental_Design", "project.overall_design",}, 
{"idf.Experimental_Design_Term_Accession_Number", "project.overall_design_accession",}, 
{"idf.Experimental_Factor_Name", "project.experimental_factor_name",}, 
{"idf.Investigation_Title", "project.title",}, 
{"idf.MAGE_TAB_Version", "project.mage_tab_version",}, 
{"idf.Protocol_Description", "project.extract_protocol",}, 
{"idf.Protocol_Name", "reformat.Protocol_Name",}, 
{"idf.Protocol_Term_Accession_Number", "reformat.Protocol_Term_Accession_Number",}, 
{"idf.Protocol_Term_Source_REF", "reformat.Protocol_Term_Source_REF",}, 
{"idf.Protocol_Type", "reformat.Protocol_Type",}, 
{"idf.PubMed_ID", "project.pmid",}, 
{"idf.PubMedID", "project.pmid",}, 
{"idf.Pubmed_ID", "project.pmid",}, 
{"idf.Public_Release_Date", "project.release_status",}, 
{"idf.Publication_Author_List", "project.publication_author_list",}, 
{"idf.Publication_DOI", "project.publication_doi",}, 
{"idf.Publication_Title", "project.publication_title",}, 
{"idf.Term_Source_File", "project.array_express.term_source_file",}, 
{"idf.Term_Source_Name", "project.array_express.term_source_name",}, 
{"sdrf.Assay_Name", "assay.array_express_assay_name",}, 
{"sdrf.Characteristics_age", "sample.donor.age",}, 
{"sdrf.Characteristics_age_Unit", "sample.donor.age_unit",}, 
{"sdrf.Characteristics_body_mass_index", "sample.donor.body_mass_index",}, 
{"sdrf.Characteristics_developmental_stage", "sample.donor.developmental_stage",}, 
{"sdrf.Characteristics_genotype", "sample.donor.genotype",}, 
{"sdrf.Characteristics_individual", "sample.donor.id",}, 
{"sdrf.Characteristics_organism", "sample.donor.species",}, 
{"sdrf.Characteristics_organism_Term_Accession_Number", "sample.donor.ncbi_taxon",}, 
{"sdrf.Characteristics_organism_part", "sample.body_part.name",}, 
{"sdrf.Characteristics_organism_part_Term_Accession_Number", "sample.body_part.ontology",}, 
{"sdrf.Characteristics_organism_part_Term_Source_REF", "sample.body_part.ontology_source",}, 
{"sdrf.Characteristics_sex", "sample.donor.sex",}, 
{"sdrf.Characteristics_single_cell_well_quality", "cell.well_quality",}, 
{"sdrf.Comment_BioSD_SAMPLE", "sample.biosd_sample",}, 
{"sdrf.Comment_Derived_ArrayExpress_FTP_file", "sample.supplementary_files",}, 
{"sdrf.Comment_ENA_EXPERIMENT", "assay.ena_experiment",}, 
{"sdrf.Comment_ENA_RUN", "assay.ena_run",}, 
{"sdrf.Comment_ENA_SAMPLE", "sample.ena_sample",}, 
{"sdrf.Comment_FASTQ_URI", "sample.files",}, 
{"sdrf.Comment_LIBRARY_LAYOUT", "reformat.Comment_LIBRARY_LAYOUT",}, 
{"sdrf.Comment_LIBRARY_SELECTION", "reformat.Comment_LIBRARY_SELECTION",}, 
{"sdrf.Comment_LIBRARY_SOURCE", "reformat.Comment_LIBRARY_SOURCE",}, 
{"sdrf.Comment_LIBRARY_STRAND", "reformat.Comment_LIBRARY_STRAND",}, 
{"sdrf.Comment_LIBRARY_STRATEGY", "assay.seq.molecule",}, 
{"sdrf.Comment_SUBMITTED_FILE_NAME", "assay.submitted_file_names",}, 
{"sdrf.Comment_Sample_description", "sample.short_label",}, 
{"sdrf.Comment_Sample_source_name", "sample.body_part.source_name",}, 
{"sdrf.Comment_Sample_title", "sample.long_label",}, 
{"sdrf.Material_Type", "reformat.Material_Type",}, 
{"sdrf.Performer", "reformat.Performer",}, 
{"sdrf.Protocol_REF", "reformat.Protocol_REF",}, 
{"sdrf.Protocol_REF_Term_Source_REF", "reformat.Protocol_REF_Term_Source_REF",}, 
{"sdrf.Source_Name", "sample.submitted_id",}, 
};

void replaceWithFirstChoice(struct tagStorm *storm, struct tagStanzaRef *leafList, 
    char *choices[], int choiceCount, char *replacement)
/* On each leaf stanza, look for a value in list, which is ordered with
 * what is likely to be best tag first.  Choose the first one that has
 * a value, and enter it in stanza as a tag with the name of replacement.
 * Finally delete all of the choices. */
{
struct tagStanzaRef *ref;
for (ref = leafList; ref != NULL; ref = ref->next)
    {
    struct tagStanza *stanza = ref->stanza;
    char *choice = NULL;
    int i;
    for (i=0; i<choiceCount; ++i)
        {
	if ((choice = tagFindVal(stanza, choices[i])) != NULL)
	    {
	    tagStanzaAppend(storm, stanza, replacement, choice);
	    break;
	    }
	}
    }
tagStormWeedArray(storm, choices, choiceCount);
}

void addFirstWithVal(struct tagStorm *storm, struct tagStanza *stanza, char *oldTag, char *newTag)
/* Look up old tag in stanza and pick first real value out of it.  Store this in newTag.
 * If there are no real values then don't make newTag. */
{
struct slName *valList = csvParse(tagFindVal(stanza, oldTag));
struct slName *val = valList;
while (val != NULL && isEmpty(val->name))	// Skip over any with no value
    val = val->next;
if (val != NULL)
    tagStanzaAppend(storm, stanza, newTag, val->name);
slFreeList(&valList);
}

void reformatPersonTags(struct tagStorm *storm)
/* Convert idf.Person_* tags to project.contact_* and project.contributor tag */
{
static char *personTags[] = 
    {
    "idf.Person_Address",
    "idf.Person_Affiliation",
    "idf.Person_Email",
    "idf.Person_First_Name",
    "idf.Person_Last_Name",
    "idf.Person_Mid_Initials",
    "idf.Person_Phone",
    "idf.Person_Roles",	    // Gets weeded but not replaced. Difficult not super informative.
    };

struct tagStanza *stanza = storm->forest;  // Project tags/idf tags live on top

/* Fetch the components of names.  We'll be converting them to first,mid,last */
struct slName *firstList = csvParse(tagFindVal(stanza, "idf.Person_First_Name"));
struct slName *midList = csvParse(tagFindVal(stanza, "idf.Person_Mid_Initials"));
struct slName *lastList= csvParse(tagFindVal(stanza, "idf.Person_Last_Name"));
char combinedName[256], lastCombinedName[256] = "";

/* We'll build up contributor string.  First element of it will be contact_name as well. */
struct dyString *contributorOut = dyStringNew(0);
struct slName *firstEl = firstList, *midEl = midList, *lastEl = lastList;
boolean firstTime = TRUE;
for (;;)
    {
    char *first = NULL, *mid = NULL, *last = NULL;
    if (firstEl != NULL)
        {
	first = firstEl->name;
	firstEl = firstEl->next;
	}
    if (midEl != NULL)
        {
	mid = midEl->name;
	midEl = midEl->next;
	}
    if (lastEl != NULL)
        {
	last = lastEl->name;
	lastEl = lastEl->next;
	}
    if (first == NULL && mid == NULL && last == NULL)
        break;
    safef(combinedName, sizeof(combinedName), "%s,%s,%s", 
	emptyForNull(first), emptyForNull(mid), emptyForNull(last));
    if (differentString(combinedName, lastCombinedName))	  // Work around for AE dupe problem
	csvEscapeAndAppend(contributorOut, combinedName);
    if (firstTime)
        {
	tagStanzaAppend(storm, stanza, "project.contact_name", contributorOut->string);
	firstTime = FALSE;
	}
    strcpy(lastCombinedName, combinedName);
    }
tagStanzaAppend(storm, stanza, "project.contributor", contributorOut->string);

/* Ok, we are done with names, now for easy things: email and phone. We only store first one */
addFirstWithVal(storm, stanza, "idf.Person_Address", "project.contact_address");
addFirstWithVal(storm, stanza, "idf.Person_Email", "project.contact_email");
addFirstWithVal(storm, stanza, "idf.Person_Phone", "project.contact_phone");
addFirstWithVal(storm, stanza, "idf.Person_Affiliation", "project.contact_institute");

tagStormWeedArray(storm, personTags, ArraySize(personTags));
}

boolean prefixedNumerical(char *prefix, char *acc)
/* Return TRUE if acc starts with prefix and is followed by all numbers */
{
if (!startsWith(prefix, acc))
   return FALSE;
return isAllDigits(acc + strlen(prefix));
}

void reformatSecondaryAccessions(struct tagStorm *storm)
/* Reformat secondaryAccessions into individual accessions */
{
struct tagStanza *stanza = storm->forest;  // Project tags/idf tags live on top
struct slName *acc, *accList = csvParse(tagFindVal(stanza, "idf.Comment_SecondaryAccession"));
for (acc = accList; acc != NULL; acc = acc->next)
    {
    if (prefixedNumerical("GSE", acc->name))
       tagStanzaAppend(storm, stanza, "project.geo_series", acc->name); 
    else if (prefixedNumerical("SRP", acc->name))
       tagStanzaAppend(storm, stanza, "project.sra_project", acc->name);
    else
       errAbort("Unrecognized secondaryAccession %s", acc->name);
    }
}

void arrayExpressStormToHcaStorm(char *inTags, char *outTags)
/* arrayExpressStormToHcaStorm - Convert output of arrayExpressToTagStorm to somethng closer to 
 * what the Human Cell Atlas wants.. */
{
struct tagStorm *storm = tagStormFromFile(inTags);
tagStormSubArray(storm, substitutions, ArraySize(substitutions));

/* Deal with Person tags */
reformatPersonTags(storm);

/* Deal with secondary accessions */
reformatSecondaryAccessions(storm);

/* Deal with some tags that we just select one from a redundant set. */
struct tagStanzaRef *leafList = tagStormListLeaves(storm);
replaceWithFirstChoice(storm, leafList, machineTags,ArraySize(machineTags), "assay.seq.machine");
replaceWithFirstChoice(storm, leafList, cellTypeTags,ArraySize(cellTypeTags), "cell.type");
replaceWithFirstChoice(storm, leafList, diseaseTags,ArraySize(diseaseTags), "sample.donor.disease");

/* Remove tags that are useless or converted into other things. */
tagStormWeedArray(storm, weeds, ArraySize(weeds));

/* And write final result */
tagStormWrite(storm, outTags, 0);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
arrayExpressStormToHcaStorm(argv[1], argv[2]);
return 0;
}
