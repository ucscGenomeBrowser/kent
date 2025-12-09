/* togaClick - click handling for TOGA tracks */
#ifndef TOGACLICK_H
#define TOGACLICK_H

#define YES_ "Yes"
#define NO_ "No"
#define ONE_ "1"

#define HLTOGA_BED_PREFIX "HLTOGAannot"
#define HLTOGA_BED_PREFIX_LEN 11
#define HLTOGA_DATA_PREFIX "HLTOGAData"
#define HLTOGA_NUCL_PREFIX "HLTOGANucl"
#define HLTOGA_INACT_PREFIX "HLTOGAInactMut"

#define HLTOGA_MAXCHAR 255


struct togaDataBB
{
    char *projection;
    char *ref_trans_id;
    char *ref_region;
    char *query_region;
    char *chain_score;
    char *chain_synteny;
    char *chain_flank;
    char *chain_gl_cds_fract;
    char *chain_loc_cds_fract;
    char *chain_exon_cov;
    char *chain_intron_cov;
    char *status;
    char *perc_intact_ign_M;
    char *perc_intact_int_M;
    char *intact_codon_prop;
    char *ouf_prop;
    char *mid_intact;
    char *mid_pres;
    char *prot_alignment;
    char *svg_line;
    char *ref_link;
    char *inact_mut_html_table;
    char *exon_ali_html;
    char *CDSseq;
    char *protseqFrameCorrected;
    char *numExonsMutated;
    char *percentExonsMutated;
};



struct togaData
{
    char *projection;
    char *ref_trans_id;
    char *ref_region;
    char *query_region;
    char *chain_score;
    char *chain_synteny;
    char *chain_flank;
    char *chain_gl_cds_fract;
    char *chain_loc_cds_fract;
    char *chain_exon_cov;
    char *chain_intron_cov;
    char *status;
    char *perc_intact_ign_M;
    char *perc_intact_int_M;
    char *intact_codon_prop;
    char *ouf_prop;
    char *mid_intact;
    char *mid_pres;
    char *prot_alignment;
    char *svg_line;
};


struct togaNucl
{
    char *transcript;
    char *exon_num;
    char *exon_region;
    char *pid;
    char *blosum;
    char *gaps;
    char *ali_class;
    char *exp_region;
    char *in_exp_region;
    char *alignment;
};


struct togaInactMut
{
    char *transcript;
    char *exon_num;
    char *position;
    char *mut_class;
    char *mutation;
    char *is_inact;
    char *mut_id;
};


struct togaDataBB *togaDataBBLoad(char **row, bits16 fieldCount);
/* Load a togaData from row fetched with select * from togaData
 * from database.  Dispose of this with togaDataFree(). */


struct togaData *togaDataLoad(char **row);
/* Load a togaData from row fetched with select * from togaData
 * from database.  Dispose of this with togaDataFree(). */

void togaDataBBFree(struct togaDataBB **pEl);
/* Free a single dynamically allocated togaDatasuch as created
 * with togaDataLoad(). */

void togaDataFree(struct togaData **pEl);
/* Free a single dynamically allocated togaDatasuch as created
 * with togaDataLoad(). */

struct togaNucl *togaNuclLoad(char **row);
/* Load a togaNucl from row fetched with select * from togaNucl
 * from database.  Dispose of this with togaNuclFree(). */

void togaNuclFree(struct togaNucl **pEl);
/* Free a single dynamically allocated togaNucl such as created
 * with togaNuclLoad(). */

struct togaInactMut *togaInactMutLoad(char **row);
/* Load a togaInactMut from row fetched with select * from togaInactMut
 * from database.  Dispose of this with togaInactMutFree(). */

void togaInactMutFree(struct togaInactMut **pEl);
/* Free a single dynamically allocated togaInactMut such as created
 * with togaInactMutLoad(). */

void extractHLTOGAsuffix(char *suffix);
/* Extract suffix from TOGA table name.
Prefix must be HLTOGAannot */

// void doHillerLabTOGAGene(struct trackDb *tdb, char *item, char *table_name);  // old
void doHillerLabTOGAGene(char *database, struct trackDb *tdb, char *item, char *table_name);
/* Put up TOGA Gene track info. */

#endif  // TOGACLICK_H
