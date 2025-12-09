/* togaClick - click handling for TOGA tracks */
#include "common.h"
#include "hgc.h"
#include "togaClick.h"
#include "string.h"
#include "htmshell.h"
#include "chromAlias.h"


struct togaDataBB *togaDataBBLoad(char **row, bits16 fieldCount)
/* Load a togaData from row fetched with select * from togaData
 * from database.  Dispose of this with togaDataFree(). */
{
    struct togaDataBB *ret;
    AllocVar(ret);
    ret->projection = cloneString(row[0]);
    ret->ref_trans_id = cloneString(row[1]);
    ret->ref_region = cloneString(row[2]);
    ret->query_region = cloneString(row[3]);
    ret->chain_score = cloneString(row[4]);

    ret->chain_synteny = cloneString(row[5]);
    ret->chain_flank = cloneString(row[6]);
    ret->chain_gl_cds_fract = cloneString(row[7]);
    ret->chain_loc_cds_fract = cloneString(row[8]);
    ret->chain_exon_cov = cloneString(row[9]);

    ret->chain_intron_cov = cloneString(row[10]);
    ret->status = cloneString(row[11]);
    ret->perc_intact_ign_M = cloneString(row[12]);
    ret->perc_intact_int_M = cloneString(row[13]);
    ret->intact_codon_prop = cloneString(row[14]);

    ret->ouf_prop = cloneString(row[15]);
    ret->mid_intact = cloneString(row[16]);
    ret->mid_pres = cloneString(row[17]);
    ret->prot_alignment = cloneString(row[18]);
    ret->svg_line = cloneString(row[19]);
    ret->ref_link = cloneString(row[20]);
    ret->inact_mut_html_table = cloneString(row[21]);
    ret->exon_ali_html = cloneString(row[22]);

    /* read two optional new items, CDSseq and frame-corrected protein */
    ret->CDSseq = NULL;
    if (fieldCount >= 35)   /* 0-11 are bed core fields. This fct gets a pointer starting with element 12. 12+23 = 35 */
       ret->CDSseq = cloneString(row[23]);
    ret->protseqFrameCorrected = NULL;
    if (fieldCount >= 36)   /* 12+24 = 36 */
       ret->protseqFrameCorrected = cloneString(row[24]);
    /* number and percent of mutated exons (additional data now shown for the gene loss status) */
    ret->numExonsMutated = NULL;
    ret->percentExonsMutated = NULL;
    if (fieldCount >= 37)   /* 12+25 = 37 */
       ret->numExonsMutated = cloneString(row[25]);
    if (fieldCount >= 38)   /* 12+26 = 38 */
       ret->percentExonsMutated = cloneString(row[26]);
    return ret;
}


struct togaData *togaDataLoad(char **row)
/* Load a togaData from row fetched with select * from togaData
 * from database.  Dispose of this with togaDataFree(). */
{
    struct togaData *ret;
    AllocVar(ret);
    ret->projection = cloneString(row[0]);
    ret->ref_trans_id = cloneString(row[1]);
    ret->ref_region = cloneString(row[2]);
    ret->query_region = cloneString(row[3]);
    ret->chain_score = cloneString(row[4]);

    ret->chain_synteny = cloneString(row[5]);
    ret->chain_flank = cloneString(row[6]);
    ret->chain_gl_cds_fract = cloneString(row[7]);
    ret->chain_loc_cds_fract = cloneString(row[8]);
    ret->chain_exon_cov = cloneString(row[9]);

    ret->chain_intron_cov = cloneString(row[10]);
    ret->status = cloneString(row[11]);
    ret->perc_intact_ign_M = cloneString(row[12]);
    ret->perc_intact_int_M = cloneString(row[13]);
    ret->intact_codon_prop = cloneString(row[14]);

    ret->ouf_prop = cloneString(row[15]);
    ret->mid_intact = cloneString(row[16]);
    ret->mid_pres = cloneString(row[17]);
    ret->prot_alignment = cloneString(row[18]);
    ret->svg_line = cloneString(row[19]);
    return ret;
}


void togaDataBBFree(struct togaDataBB **pEl)
/* Free a single dynamically allocated togaDatasuch as created
 * with togaDataLoad(). */
{
    struct togaDataBB *el;

    if ((el = *pEl) == NULL) return;
    freeMem(el->projection);
    freeMem(el->ref_trans_id);
    freeMem(el->ref_region);
    freeMem(el->query_region);
    freeMem(el->chain_score);

    freeMem(el->chain_synteny);
    freeMem(el->chain_flank);
    freeMem(el->chain_gl_cds_fract);
    freeMem(el->chain_loc_cds_fract);
    freeMem(el->chain_exon_cov);

    freeMem(el->chain_intron_cov);
    freeMem(el->status);
    freeMem(el->perc_intact_ign_M);
    freeMem(el->perc_intact_int_M);
    freeMem(el->intact_codon_prop);

    freeMem(el->ouf_prop);
    freeMem(el->mid_intact);
    freeMem(el->mid_pres);
    freeMem(el->prot_alignment);
    freeMem(el->svg_line);
    freeMem(el->ref_link);
    freeMem(el->inact_mut_html_table);
    freeMem(el->exon_ali_html);
    freez(pEl);
}


void togaDataFree(struct togaData **pEl)
/* Free a single dynamically allocated togaDatasuch as created
 * with togaDataLoad(). */
{
    struct togaData *el;

    if ((el = *pEl) == NULL) return;
    freeMem(el->projection);
    freeMem(el->ref_trans_id);
    freeMem(el->ref_region);
    freeMem(el->query_region);
    freeMem(el->chain_score);

    freeMem(el->chain_synteny);
    freeMem(el->chain_flank);
    freeMem(el->chain_gl_cds_fract);
    freeMem(el->chain_loc_cds_fract);
    freeMem(el->chain_exon_cov);

    freeMem(el->chain_intron_cov);
    freeMem(el->status);
    freeMem(el->perc_intact_ign_M);
    freeMem(el->perc_intact_int_M);
    freeMem(el->intact_codon_prop);

    freeMem(el->ouf_prop);
    freeMem(el->mid_intact);
    freeMem(el->mid_pres);
    freeMem(el->prot_alignment);
    freeMem(el->svg_line);
    freez(pEl);
}

struct togaNucl *togaNuclLoad(char **row)
/* Load a togaNucl from row fetched with select * from togaNucl
 * from database.  Dispose of this with togaNuclFree(). */
{
    struct togaNucl *ret;
    AllocVar(ret);
    ret->transcript = cloneString(row[0]);
    ret->exon_num = cloneString(row[1]);
    ret->exon_region = cloneString(row[2]);
    ret->pid = cloneString(row[3]);
    ret->blosum = cloneString(row[4]);

    ret->gaps = cloneString(row[5]);
    ret->ali_class = cloneString(row[6]);
    ret->exp_region = cloneString(row[7]);
    ret->in_exp_region = cloneString(row[8]);
    ret->alignment = cloneString(row[9]);
    return ret;
}


void togaNuclFree(struct togaNucl **pEl)
/* Free a single dynamically allocated togaNucl such as created
 * with togaNuclLoad(). */
{
    struct togaNucl *el;

    if ((el = *pEl) == NULL) return;
    freeMem(el->transcript);
    freeMem(el->exon_num);
    freeMem(el->exon_region);
    freeMem(el->pid);
    freeMem(el->blosum);

    freeMem(el->gaps);
    freeMem(el->ali_class);
    freeMem(el->exp_region);
    freeMem(el->in_exp_region);
    freeMem(el->alignment);
    freez(pEl);
}


struct togaInactMut *togaInactMutLoad(char **row)
/* Load a togaInactMut from row fetched with select * from togaInactMut
 * from database.  Dispose of this with togaInactMutFree(). */
{
    struct togaInactMut *ret;
    AllocVar(ret);
    ret->transcript = cloneString(row[0]);
    ret->exon_num = cloneString(row[1]);
    ret->position = cloneString(row[2]);
    ret->mut_class = cloneString(row[3]);
    ret->mutation = cloneString(row[4]);

    ret->is_inact = cloneString(row[5]);
    ret->mut_id = cloneString(row[6]);
    return ret;
}


void togaInactMutFree(struct togaInactMut **pEl)
/* Free a single dynamically allocated togaInactMut such as created
 * with togaInactMutLoad(). */
{
    struct togaInactMut *el;
    if ((el = *pEl) == NULL) return;
    freeMem(el->transcript);
    freeMem(el->exon_num);
    freeMem(el->position);
    freeMem(el->mut_class);
    freeMem(el->mutation);

    freeMem(el->is_inact);
    freeMem(el->mut_id);
    freez(pEl);
}


void extractHLTOGAsuffix(char *suffix)
/* Extract suffix from TOGA table name.
Prefix must be HLTOGAannot */
{
    int suff_len = strlen(suffix);
    if (suff_len <= HLTOGA_BED_PREFIX_LEN)
    // we cannot chop first PREFIX_LEN characters
    {
        // TODO: NOT SURE IF IT WORKS; but this must not happen
        char empty[5] = { '\0' };
        strcpy(suffix, empty);
    } else {
        // just start the string 11 characters upstream
        memmove(suffix, suffix + HLTOGA_BED_PREFIX_LEN, suff_len - HLTOGA_BED_PREFIX_LEN + 1);
    }
}


void HLprintQueryProtSeqForAli(char *proteinAlignment) {
    // take protein sequence alignment
    // print only the query sequence
    char *str = proteinAlignment;
    int printed_char_num = 0;
    while ((str = strstr(str, "que:")) != NULL)
    {
        str += 10;
        char ch;
        while ((ch = *str++) != '<') {
            if (ch != '-') {
                putchar(ch);
                ++printed_char_num;
            }
            if (printed_char_num == 80) {
                printed_char_num = 0;
                printf("<BR>");
            }
        }
    }
}

void print_with_newlines(const char *str) {
    int line_length = 80; // Number of characters per line
    int length = strlen(str);
    int i = 0;

    while (i < length) {
        /* Print up to 80 characters or the remainder of the string */
        int chars_to_print = (length - i < line_length) ? (length - i) : line_length;
        printf("%.*s<BR>", chars_to_print, &str[i]);
        i += chars_to_print;
    }
}


void doHillerLabTOGAGeneBig(char *database, struct trackDb *tdb, char *item, char *table_name)
/* Put up TOGA Gene track info. */
// To think about -> put into a single bigBed
// string: HTML formatted inact mut
// string: HTML formatted exon ali section
{
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
char *chrom = cartString(cart, "c");
char *fileName = bbiNameFromSettingOrTable(tdb, NULL, tdb->table);
struct bbiFile *bbi =  bigBedFileOpenAlias(hReplaceGbdb(fileName), chromAliasFindAliases);
struct lm *lm = lmInit(0);
struct bigBedInterval *bbList = bigBedIntervalQuery(bbi, chrom, start, end, 0, lm);
struct bigBedInterval *bb;
char *fields[bbi->fieldCount];
for (bb = bbList; bb != NULL; bb = bb->next)
    {
    if (!(bb->start == start && bb->end == end))
	continue;

    // our names are unique
    char *name = cloneFirstWordByDelimiterNoSkip(bb->rest, '\t');
    boolean match = (isEmpty(name) && isEmpty(item)) || sameOk(name, item);
    if (!match)
        continue;

    char startBuf[16], endBuf[16];
    bigBedIntervalToRow(bb, chrom, startBuf, endBuf, fields, bbi->fieldCount);
    break;
    }

printf("<h3>Projection v2 %s</h3>\n", item);
struct togaDataBB *info = togaDataBBLoad(&fields[11], bbi->fieldCount);  // Bogdan: why 11? 0-11 are bed-like fields likely

printf("<B>Reference transcript: </B>%s<BR>", info->ref_link);
printf("<B>Genomic locus in reference: </B>%s<BR>\n", info->ref_region);
printf("<B>Genomic locus in query: </B>%s<BR>\n", info->query_region);

printf("<B>Projection classification: </B>%s<BR>\n", info->status);
printf("<B>Probability that query locus is orthologous: </B>%s<BR>\n", info->chain_score);
// list of chain features (for orthology classification)
printf("<span class='hideToggle' data-target='collapseChain' style='cursor: pointer;'>Show features used for ortholog probability:</span>\n");
printf("<div id='collapseChain' style='display: none;'>\n");
printf("<p><ul>\n");
printf("<li>Synteny (log10 value): %s</li>\n", info->chain_synteny);
printf("<li>Global CDS fraction: %s</li>\n", info->chain_gl_cds_fract);
printf("<li>Local CDS fraction: %s</li>\n", info->chain_loc_cds_fract);
printf("<li>Local intron fraction: %s</li>\n", info->chain_intron_cov);
printf("<li>Local CDS coverage: %s</li>\n", info->chain_exon_cov);
printf("<li>Flank fraction: %s</li>\n", info->chain_flank);
printf("</ul></p>\n");

printf("<p>\n<b>Feature description:</b>\n");
printf("For each projection (one reference transcript and one overlapping chain),\n");
printf("TOGA computes the following features by intersecting the reference coordinates of aligning\n");
printf("blocks in the chain with different gene parts (coding exons, UTR (untranslated region) exons, introns)\n");
printf("and the respective intergenic regions.\n</p>\n");

printf("<p>We define the following variables:\n<ul>\n");
printf("<li>c: number of reference bases in the intersection between chain blocks and coding exons of the gene under consideration.</li>\n");
printf("<li>C: number of reference bases in the intersection between chain blocks and coding exons of all genes. </li>\n");
printf("<li>a: number of reference bases in the intersection between chain blocks and coding exons and introns of the gene under consideration. </li>\n");
printf("<li>A: number of reference bases in the intersection between chain blocks and coding exons and introns of all genes and the intersection\n");
printf("between chain blocks and intergenic regions (excludes UTRs). </li>\n");
printf("<li>f: number of reference bases in chain blocks overlapping the 10 kb flanks of the gene under consideration.\n");
printf("Alignment blocks overlapping exons of another gene that is located in these 10 kb flanks are ignored. </li>\n");
printf("<li>i: number of reference bases in the intersection between chain blocks and introns of the gene under consideration. </li>\n");
printf("<li>CDS (coding sequence): length of the coding region of the gene under consideration. </li>\n");
printf("<li>I: sum of all intron lengths of the gene under consideration. </li>\n");
printf("</ul></p>\n");
printf("<p>Using these variables, TOGA computes the following features:\n");
printf("<ul>\n");
printf("<li>&quot;global CDS fraction&quot; as C / A. Chains with a high value have alignments that largely overlap coding exons,");
printf("which is a hallmark of paralogous or processed pseudogene chains. In contrast, chains with a low value also align many ");
printf("intronic and intergenic regions, which is a hallmark of orthologous chains. </li>\n");
printf("<li>&quot;local CDS fraction&quot; as c / a. Orthologous chains tend to have a lower value, as intronic ");
printf("regions partially align. This feature is not computed for single-exon genes. </li>\n");
printf("<li>&quot;local intron fraction&quot; as i / I. Orthologous chains tend to have a higher value.");
printf("This feature is not computed for single-exon genes. </li>\n");
printf("<li>&quot;flank fraction&quot; as f / 20,000. Orthologous chains tend to have higher values,");
printf("as flanking intergenic regions partially align. This feature is important to detect orthologous loci of single-exon genes. </li>\n");
printf("<li>&quot;synteny&quot; as log10 of the number of genes, whose coding exons overlap by at least one base aligning");
printf("blocks of this chain. Orthologous chains tend to cover several genes located in a conserved order, resulting in higher synteny values. </li>\n");
printf("<li>&quot;local CDS coverage&quot; as c / CDS, which is only used for single-exon genes. </li>\n");
printf("</ul></p>\n");


printf("</div>\n");
printf("<hr style='margin-bottom:-0.5em;color:black;'>\n");

// show inact mut plot
printf("<h4>Visualization of inactivating mutations on exon-intron structure</h4>\n");
printf("%s\n", info->svg_line);
printf("<BR>Exons shown in grey are missing (often overlap assembly gaps).\nExons shown in");
printf(" red or blue are deleted or do not align at all.\nRed indicates that the exon deletion ");
printf("shifts the reading frame, while blue indicates that exon deletion(s) are framepreserving.<br>\n");

// GLP features
printf("<span class='hideToggle data-target='collapseGLP' style='cursor: pointer;'>Show features used for transcript classification</span>\n");
printf("<div id='collapseGLP' style='display: none;'>\n");
printf("<p><ul>\n");
printf("<li>Percent intact, ignoring missing sequence: %s</li>\n", info->perc_intact_ign_M);
printf("<li>Percent intact, treating missing as intact sequence: %s</li>\n", info->perc_intact_int_M);
printf("<li>Proportion of intact codons: %s</li>\n", info->intact_codon_prop);
printf("<li>Percent of CDS not covered by this chain (0 unless the chain covers only a part of the gene): %s</li>\n", info->ouf_prop);
if (sameWord(info->mid_intact, ONE_))
{
    printf("<li>Middle 80 percent of CDS intact: %s</li>\n", YES_);
} else {
    printf("<li>Middle 80 percent of CDS intact: %s</li>\n", NO_);
}
if (sameWord(info->mid_pres, ONE_))
{
    printf("<li>Middle 80 percent of CDS present: %s</li>\n", YES_);
} else {
    printf("<li>Middle 80 percent of CDS present: %s</li>\n", NO_);
}
if (info->numExonsMutated != NULL && info->percentExonsMutated != NULL) {
    printf("<li>Number of exons with inactivating mutations: %s (%s%% of the present exons; threshold is 20%%)</li>\n", info->numExonsMutated, info->percentExonsMutated);
}
printf("</ul></p>\n</div>\n");


printf("<hr style='margin-bottom:-0.5em;color:black;'>\n");
printf("<h4>Predicted protein sequence</h4>\n");

printf("<span class='hideToggle' data-target='collapseProt' style='cursor: pointer;'>Show protein sequence of query</span>\n");
printf("<div id='collapseProt' style='display: none;'>\n");
printf("<p><TT>");
HLprintQueryProtSeqForAli(info->prot_alignment);
printf("\n</TT></p>\n</div><BR>\n");

if (info->protseqFrameCorrected != NULL) {
  printf("<span class='hideToggle' data-target='collapseProtFrameCorrected' style='cursor: pointer;'>Show frame-corrected protein sequence of query (potential frameshifts are masked)</span>\n");
  printf("<div id='collapseProtFrameCorrected' style='display: none;'>\n");
  printf("<p><TT>");
  print_with_newlines(info->protseqFrameCorrected);
  printf("\n</TT></p>\n</div>\n");
}

if (info->CDSseq != NULL) {
  printf("<hr style='margin-bottom:-0.5em;color:black;'>\n");
  printf("<h4>Predicted coding (DNA) sequence</h4>\n");
  printf("<span class='hideToggle' data-target='collapseCDS' style='cursor: pointer;'>Show coding sequence of query</span>\n");
  printf("<div id='collapseCDS' style='display: none;'>\n");
  printf("<p><TT>");
  print_with_newlines(info->CDSseq);
  printf("\n</TT></p>\n</div>\n");
}

// and show protein sequence
printf("<hr style='margin-bottom:-0.5em;color:black;'>\n");
printf("<h4>Protein sequence alignment</h4>\n");
printf("<span class='hideToggle' data-target='collapseProtAli' style='cursor: pointer;'>Show alignment between reference and query</span>\n");
printf("<div id='collapseProtAli' style='display: none;'>\n");
printf("<p><TT>%s</TT>\n", info->prot_alignment);
printf("</p></div>\n");

// show inactivating mutations if required
printf("<hr style='margin-bottom:-0.5em;color:black;'>\n");
printf("<h4>List of inactivating mutations</h4>\n");
printf("<span class='hideToggle' data-target='collapseMuts' style='cursor: pointer;'>Show inactivating mutations</span>\n");
printf("<div id='collapseMuts' style='display: none;'>\n");
printf("<p><table border = \"1\" width = \"640\">\n");  // init table
printf("<tr><th>Exon number</th><th>Codon number</th><th>Mutation class</th><th>Mutation</th><th>Treated as inactivating</th><th>Mutation ID</th>\n");
printf("</tr>\n");
printf("%s\n", info->inact_mut_html_table);
printf("</table></p>\n");
printf("</div>\n\n");

// show exons data
printf("<hr style='margin-bottom:-0.5em;color:black;'>\n");
printf("<h4>Exon alignments</h4>\n");

printf("<span class='hideToggle' data-target'collapseExons' style='cursor: pointer;'>Show exon sequences and features</span>\n");
printf("<div id='collapseExons' style='display: none;'>\n");
printf("<p>%s</p>\n", info->exon_ali_html);

printf("<hr style='margin-bottom:-0.5em;color:black;'>\n");
printf("</div>\n<BR><BR>\n");

printTrackHtml(tdb);  // and do I need this?
}


void doHillerLabTOGAGene(char *database, struct trackDb *tdb, char *item, char *table_name)
/* Put up TOGA Gene track info. */
{
    //int start = cartInt(cart, "o");
    char headerTitle[512];
    char suffix[512];
    strcpy(suffix, table_name);
    extractHLTOGAsuffix(suffix);
    safef(headerTitle, sizeof(headerTitle), "%s", item);
    genericHeader(tdb, headerTitle);
    printf("<h2>TOGA gene annotation</h2>\n");
    // htmlHorizontalLine();

    if (startsWith("bigBed", tdb->type))
        {
        doHillerLabTOGAGeneBig(database, tdb, item, table_name);
        return;
        }

    struct sqlConnection *conn = hAllocConn(database);

    // define TOGA table names: initate with pre-defined prefixes
    char togaDataTableName[256];
    char togaNuclTableName[256];
    char togaInactMutTableName[256];
    strcpy(togaDataTableName, HLTOGA_DATA_PREFIX);
    strcpy(togaNuclTableName, HLTOGA_NUCL_PREFIX);
    strcpy(togaInactMutTableName, HLTOGA_INACT_PREFIX);
    // add suffix
    strcat(togaDataTableName, suffix);
    strcat(togaNuclTableName, suffix);
    strcat(togaInactMutTableName, suffix);


    if (hTableExists(database, togaDataTableName))
    {
        printf("<h3>Projection %s</h3><BR>\n", item);
        char query[256];
        struct sqlResult *sr = NULL;
        char **row;
        struct togaData *info = NULL;

        sqlSafef(query, sizeof(query), "select * from %s where transcript='%s'", togaDataTableName, item);
        sr = sqlGetResult(conn, query);

        if ((row = sqlNextRow(sr)) != NULL) {
            info = togaDataLoad(row);  // parse sql output
            // fill HTML template:
            printf("<B>Reference transcript: </B><A HREF=\"http://www.ensembl.org/Homo_sapiens/transview?transcript=%s\" target=_blank>%s</A><BR>",
                   info->ref_trans_id, info->ref_trans_id);
            printf("<B>Genomic locus in reference: </B>%s<BR>\n", info->ref_region);
            printf("<B>Genomic locus in query: </B>%s<BR>\n", info->query_region);

            printf("<B>Projection classification: </B>%s<BR>\n", info->status);
            printf("<B>Probability that query locus is orthologous: </B>%s<BR>\n", info->chain_score);
            // list of chain features (for orthology classification)
            printf("<span class='hideToggle' data-target='collapseChain' style='cursor: pointer;'>Show features used for ortholog probability</span>\n");
            printf("<div id='collapseChain' style='display: none;'");
            printf("<p><ul>\n");
            printf("<li>Synteny (log10 value): %s</li>\n", info->chain_synteny);
            printf("<li>Global CDS fraction: %s</li>\n", info->chain_gl_cds_fract);
            printf("<li>Local CDS fraction: %s</li>\n", info->chain_loc_cds_fract);
            printf("<li>Local intron fraction: %s</li>\n", info->chain_intron_cov);
            printf("<li>Local CDS coverage: %s</li>\n", info->chain_exon_cov);
            printf("<li>Flank fraction: %s</li>\n", info->chain_flank);
            printf("</ul></p>\n");

            printf("<br>\n<b>Feature description:</b>\n");
            printf("For each projection (one reference transcript and one overlapping chain),\n");
            printf("TOGA computes the following features by intersecting the reference coordinates of aligning\n");
            printf("blocks in the chain with different gene parts (coding exons, UTR (untranslated region) exons, introns)\n");
            printf("and the respective intergenic regions.\n<br>\n");

            printf("We define the following variables:\n<ul>\n");
            printf("<li>c: number of reference bases in the intersection between chain blocks and coding exons of the gene under consideration.</li>\n");
            printf("<li>C: number of reference bases in the intersection between chain blocks and coding exons of all genes. </li>\n");
            printf("<li>a: number of reference bases in the intersection between chain blocks and coding exons and introns of the gene under consideration. </li>\n");
            printf("<li>A: number of reference bases in the intersection between chain blocks and coding exons and introns of all genes and the intersection\n");
            printf("between chain blocks and intergenic regions (excludes UTRs). </li>\n");
            printf("<li>f: number of reference bases in chain blocks overlapping the 10 kb flanks of the gene under consideration.\n");
            printf("Alignment blocks overlapping exons of another gene that is located in these 10 kb flanks are ignored. </li>\n");
            printf("<li>i: number of reference bases in the intersection between chain blocks and introns of the gene under consideration. </li>\n");
            printf("<li>CDS (coding sequence): length of the coding region of the gene under consideration. </li>\n");
            printf("<li>I: sum of all intron lengths of the gene under consideration. </li>\n");
            printf("</ul>\n");
            printf("Using these variables, TOGA computes the following features:\n");
            printf("<ul>\n");
            printf("<li>&quot;global CDS fraction&quot; as C / A. Chains with a high value have alignments that largely overlap coding exons,");
            printf("which is a hallmark of paralogous or processed pseudogene chains. In contrast, chains with a low value also align many ");
            printf("intronic and intergenic regions, which is a hallmark of orthologous chains. </li>\n");
            printf("<li>&quot;local CDS fraction&quot; as c / a. Orthologous chains tend to have a lower value, as intronic ");
            printf("regions partially align. This feature is not computed for single-exon genes. </li>\n");
            printf("<li>&quot;local intron fraction&quot; as i / I. Orthologous chains tend to have a higher value.");
            printf("This feature is not computed for single-exon genes. </li>\n");
            printf("<li>&quot;flank fraction&quot; as f / 20,000. Orthologous chains tend to have higher values,");
            printf("as flanking intergenic regions partially align. This feature is important to detect orthologous loci of single-exon genes. </li>\n");
            printf("<li>&quot;synteny&quot; as log10 of the number of genes, whose coding exons overlap by at least one base aligning");
            printf("blocks of this chain. Orthologous chains tend to cover several genes located in a conserved order, resulting in higher synteny values. </li>\n");
            printf("<li>&quot;local CDS coverage&quot; as c / CDS, which is only used for single-exon genes. </li>\n");
            printf("</ul>\n");


            printf("</ul>\n</div>\n<BR>\n");
            htmlHorizontalLine();

            // show inact mut plot
            printf("<h4>Visualization of inactivating mutations on exon-intron structure</h4>\n");
            printf("%s<BR>\n", info->svg_line);
            printf("<BR>Exons shown in grey are missing (often overlap assembly gaps).\nExons shown in");
            printf(" red or blue are deleted or do not align at all.\nRed indicates that the exon deletion ");
            printf("shifts the reading frame, while blue indicates that exon deletion(s) are framepreserving.<br>\n");

            // GLP features
            printf("<span class='hideToggle' data-target='collapseGLP' style='cursor: pointer;'>Show features used for transcript classification</span>\n");
            printf("<div id='collapseGLP' style='display:none;'>\n");
            printf("<p><ul>\n");
            printf("<li>Percent intact, ignoring missing sequence: %s</li>\n", info->perc_intact_ign_M);
            printf("<li>Percent intact, treating missing as intact sequence: %s</li>\n", info->perc_intact_int_M);
            printf("<li>Proportion of intact codons: %s</li>\n", info->intact_codon_prop);
            printf("<li>Percent of CDS not covered by this chain (0 unless the chain covers only a part of the gene): %s</li>\n", info->ouf_prop);
            if (sameWord(info->mid_intact, ONE_))
            {
                printf("<li>Middle 80 percent of CDS intact: %s</li>\n", YES_);
            } else {
                printf("<li>Middle 80 percent of CDS intact: %s</li>\n", NO_);
            }
            if (sameWord(info->mid_pres, ONE_))
            {
                printf("<li>Middle 80 percent of CDS present: %s</li>\n", YES_);
            } else {
                printf("<li>Middle 80 percent of CDS present: %s</li>\n", NO_);
            }
            printf("</ul></p>\n</div>\n<BR>\n");
            printf("<HR ALIGN=\"CENTER\"><h4>Query protein sequence</h4><BR>");

            printf("<span class='hideToggle' data-target='collapseProt' style='cursor: pointer;'>Show protein sequence of query</span>\n");
            printf("<div id='collapseProt' style='display:none;'>\n");
            printf("<p><TT>{protein seq of the query without dashes or other things. Should end with *}\n");
            printf("<BR>\n</TT></p>\n</div>\n");

            // and show protein sequence
            htmlHorizontalLine();
            printf("<h4>Protein sequence alignment</h4><BR>\n");
            printf("<span class='hideToggle' data-target='collapseProtAli' style='cursor: pointer;'>Show alignment between reference and query</span>\n");
            printf("<div id='collapseProtAli' style='display: none;'>\n");
            printf("<p><TT>%s</TT></p><BR>\n", info->prot_alignment);
            printf("</div>\n<BR><BR>\n");

            // do not forget to free toga data struct
            togaDataFree(&info);
        } else {
            // no data found, need to report this
            printf("<h3>No found data for %s</h3>\n", item);
        }
        sqlFreeResult(&sr);
    }

    // show inactivating mutations if required
    printf("<h4>List of inactivating mutations</h4><BR>\n");

    if (hTableExists(database, togaInactMutTableName))
    {
        char query[256];
        struct sqlResult *sr = NULL;
        char **row;
        sqlSafef(query, sizeof(query), "select * from %s where transcript='%s'", togaInactMutTableName, item);
        sr = sqlGetResult(conn, query);
        printf("<span class='hideToggle' data-target='collapseMuts' style='cursor: pointer;'>Show inactivating mutations</span>\n");
        printf("<div id='collapseMuts' style='display: none;'>\n");
        printf("<table border = \"1\" width = \"640\">\n");  // init table
        printf("<tr><th>Exon number</th><th>Codon number</th><th>Mutation class</th><th>Mutation</th><th>Treated as inactivating</th><th>Mutation ID</th>\n");
        printf("</tr>\n");
        while ((row = sqlNextRow(sr)) != NULL)
        {
            struct togaInactMut *info = NULL;
            info = togaInactMutLoad(row);
            printf("<tr>\n");
            printf("<td>%s</td>\n", info->exon_num);
            printf("<td>%s</td>\n", info->position);
            printf("<td>%s</td>\n", info->mut_class);
            printf("<td>%s</td>\n", info->mutation);
            if (sameWord(info->is_inact, ONE_)){
                printf("<td>%s</td>\n", YES_);
            } else {
                printf("<td>%s</td>\n", NO_);
            }
            printf("<td>%s</td>\n", info->mut_id);
            printf("</tr>\n");
            togaInactMutFree(&info);
        }
        sqlFreeResult(&sr);
        printf("</table>\n");
        printf("</div>\n<BR>\n");
    } else {
        printf("<B>Sorry, cannot find TOGAInactMut table.</B><BR>\n");
    }

    // show exons data
    htmlHorizontalLine();
    printf("<h4>Exon alignments</h4><BR>\n");

    if (hTableExists(database, togaNuclTableName))
    {
        char query[256];
        struct sqlResult *sr = NULL;
        char **row;
        printf("<span class='hideToggle' data-target='collapseExons' style='cursor: pointer;'>Show exon sequences and features</span>\n");
        printf("<div id='collapseExons' style='display: none;'>\n");
        sqlSafef(query, sizeof(query), "select * from %s where transcript='%s'", togaNuclTableName, item);
        sr = sqlGetResult(conn, query);

        while ((row = sqlNextRow(sr)) != NULL)
        {
            struct togaNucl *info = NULL;
            info = togaNuclLoad(row);
            printf("<h5>Exon number: %s</h5><BR>\n", info->exon_num);
            printf("<B>Exon region:</B> %s<BR>\n", info->exon_region);
            printf("<B>Nucleotide percent identity:</B> %s | <B>BLOSUM:</B> %s <BR>\n", info->pid, info->blosum);
            if (sameWord(info->gaps, ONE_)){
                printf("<B>Intersects assembly gaps:</B> %s<BR>\n", YES_);
            } else {
                printf("<B>Intersects assembly gaps:</B> %s<BR>\n", NO_);
            }
            printf("<B>Exon alignment class:</B> %s<BR>\n", info->ali_class);
            if (sameWord(info->in_exp_region, ONE_)){
                printf("<B>Detected within expected region (%s):</B> %s<BR>\n", info->exp_region, YES_);
            } else {
                printf("<B>Detected within expected region (%s):</B> %s<BR>\n", info->exp_region, NO_);
            }
            // printf("<B>Expected region:</B> %s<BR>\n", info->exp_region);
            printf("<BR>\n");
            printf("<B>Sequence alignment between reference and query exon:</B><BR>\n");
            printf("%s<BR>\n", info->alignment);
            togaNuclFree(&info);
        }
        sqlFreeResult(&sr);
        printf("</div>\n<BR><BR>\n");
    } else {
        printf("<B>Sorry, cannot find TOGANucl table.</B><BR>\n");
    }

    htmlHorizontalLine();

    // TODO: check whether I need this
    printf("%s", hgTracksPathAndSettings());

    printTrackHtml(tdb);  // and do I need this?
    hFreeConn(&conn);
}
