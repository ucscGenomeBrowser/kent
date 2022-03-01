/* togaClick - click handling for TOGA tracks */
#include "common.h"
#include "hgc.h"
#include "togaClick.h"
#include "string.h"
#include "htmshell.h"
#include "chromAlias.h"


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


void doHillerLabTOGAGeneBig(char *database, struct trackDb *tdb, char *item, char *table_name)
/* Put up TOGA Gene track info. */
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

printf("<h3>Projection %s</h3><BR>\n", item);
struct togaData       *info = togaDataLoad(&fields[11]);  
// fill HTML template:
printf("<B>Projected via: </B><A HREF=\"http://www.ensembl.org/Homo_sapiens/transview?transcript=%s\" target=_blank>%s</A><BR>",
       info->ref_trans_id, info->ref_trans_id);
printf("<B>Region in reference: </B>%s<BR>\n", info->ref_region);
printf("<B>Region in query: </B>%s<BR>\n", info->query_region);

printf("<B>Projection class: </B>%s<BR>\n", info->status);
printf("<B>Chain score: </B>%s<BR>\n", info->chain_score);
// list of chain features (for orthology classification)
printf("<a data-toggle=\"collapse\" href=\"#collapseChain\">Show chain features for classification</a>\n");
printf("<div id=\"collapseChain\" class=\"panel-collapse collapse\">\n");
printf("<ul>\n");
printf("<li>Synteny: %s</li>\n", info->chain_synteny);
printf("<li>Global CDS fraction: %s</li>\n", info->chain_gl_cds_fract);
printf("<li>Local CDS fraction: %s</li>\n", info->chain_loc_cds_fract);
printf("<li>Local intron fraction: %s</li>\n", info->chain_intron_cov);
printf("<li>Local CDS coverage: %s</li>\n", info->chain_exon_cov);
printf("<li>Flank fraction: %s</li>\n", info->chain_flank);
printf("</ul>\n</div>\n<BR>\n");
htmlHorizontalLine();

// show inact mut plot
printf("<h4>Inactivating mutations plot</h4>\n");
printf("%s<BR>\n", info->svg_line);

// GLP features
printf("<a data-toggle=\"collapse\" href=\"#collapseGLP\">Show GLP features</a>\n");
printf("<div id=\"collapseGLP\" class=\"panel-collapse collapse\">\n");
printf("<ul>\n");
printf("<li>Percent intact ignoring missing seq: %s</li>\n", info->perc_intact_ign_M);
printf("<li>Percent intact (miss == intact): %s</li>\n", info->perc_intact_int_M);
printf("<li>Intact codon proportion %s</li>\n", info->intact_codon_prop);
printf("<li>Out of chain proportion: %s</li>\n", info->ouf_prop);
if (sameWord(info->mid_intact, ONE_))
{
    printf("<li>Middle 80 percent intact: %s</li>\n", YES_);
} else {
    printf("<li>Middle 80 percent intact: %s</li>\n", NO_);
}
if (sameWord(info->mid_pres, ONE_))
{
    printf("<li>Middle 80 percent present: %s</li>\n", YES_);
} else {
    printf("<li>Middle 80 percent present: %s</li>\n", NO_);
}
printf("</ul>\n</div>\n<BR>\n");

// and show protein sequence
htmlHorizontalLine();
printf("<h4>Protein sequence</h4><BR>\n");
printf("<a data-toggle=\"collapse\" href=\"#collapseProt\">Show protein alignment</a>\n");
printf("<div id=\"collapseProt\" class=\"panel-collapse collapse\">\n");
printf("<TT>%s</TT><BR>\n", info->prot_alignment);
printf("</div>\n<BR><BR>\n");

// show inactivating mutations if required
printf("<h4>Inactivating mutations</h4><BR>\n");

printf("<a data-toggle=\"collapse\" href=\"#collapseMuts\">Show inactivating mutations</a>\n");
printf("<div id=\"collapseMuts\" class=\"panel-collapse collapse\">\n");
printf("<table border = \"1\" width = \"640\">\n");  // init table
printf("<tr><th>exon</th><th>pos</th><th>m_class</th><th>mut</th><th>is_inact</th><th>mut_id</th>\n");
printf("</tr>\n");
fileName = trackDbSetting(tdb, "inactMutUrl");
bbi =  bigBedFileOpenAlias(hReplaceGbdb(fileName), chromAliasFindAliases);
//struct lm *lm = lmInit(0);
bbList = bigBedIntervalQuery(bbi, chrom, start, end, 0, lm);
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
        struct togaInactMut *info = NULL;
        info = togaInactMutLoad(&fields[3]);
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
    //sqlFreeResult(&sr);
    printf("</table>\n");
    printf("</div>\n<BR>\n");

// show exons data
htmlHorizontalLine();
printf("<h4>Exons data</h4><BR>\n");

printf("<a data-toggle=\"collapse\" href=\"#collapseExons\">Show exon sequences and features</a>\n");
printf("<div id=\"collapseExons\" class=\"panel-collapse collapse\">\n");
fileName = trackDbSetting(tdb, "nuclUrl");
bbi =  bigBedFileOpenAlias(hReplaceGbdb(fileName), chromAliasFindAliases);
//struct lm *lm = lmInit(0);
bbList = bigBedIntervalQuery(bbi, chrom, start, end, 0, lm);
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
    struct togaNucl *info = NULL;
    info = togaNuclLoad(&fields[3]);
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
        printf("<B>Detected within expected region:</B> %s<BR>\n", YES_);
    } else {
        printf("<B>Detected within expected region:</B> %s<BR>\n", NO_);
    }
    printf("<B>Expected region:</B> %s<BR>\n", info->exp_region);
    printf("<BR>\n");
    printf("<B>Sequence alignment:</B><BR>\n");
    printf("%s<BR>\n", info->alignment);
    togaNuclFree(&info);
    }

    //sqlFreeResult(&sr);
    printf("</div>\n<BR><BR>\n");

htmlHorizontalLine();

// TODO: check whether I need this
printf("%s", hgTracksPathAndSettings());
hPrintf("<link rel=\"stylesheet\" href=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.4.1/css/bootstrap.min.css\">");
hPrintf("<script src=\"https://ajax.googleapis.com/ajax/libs/jquery/3.5.1/jquery.min.js\"></script>");
hPrintf("<script src=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.4.1/js/bootstrap.min.js\"></script>");


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
            printf("<B>Projected via: </B><A HREF=\"http://www.ensembl.org/Homo_sapiens/transview?transcript=%s\" target=_blank>%s</A><BR>",
                   info->ref_trans_id, info->ref_trans_id);
            printf("<B>Region in reference: </B>%s<BR>\n", info->ref_region);
            printf("<B>Region in query: </B>%s<BR>\n", info->query_region);

            printf("<B>Projection class: </B>%s<BR>\n", info->status);
            printf("<B>Chain score: </B>%s<BR>\n", info->chain_score);
            // list of chain features (for orthology classification)
            printf("<a data-toggle=\"collapse\" href=\"#collapseChain\">Show chain features for classification</a>\n");
            printf("<div id=\"collapseChain\" class=\"panel-collapse collapse\">\n");
            printf("<ul>\n");
            printf("<li>Synteny: %s</li>\n", info->chain_synteny);
            printf("<li>Global CDS fraction: %s</li>\n", info->chain_gl_cds_fract);
            printf("<li>Local CDS fraction: %s</li>\n", info->chain_loc_cds_fract);
            printf("<li>Local intron fraction: %s</li>\n", info->chain_intron_cov);
            printf("<li>Local CDS coverage: %s</li>\n", info->chain_exon_cov);
            printf("<li>Flank fraction: %s</li>\n", info->chain_flank);
            printf("</ul>\n</div>\n<BR>\n");
            htmlHorizontalLine();

            // show inact mut plot
            printf("<h4>Inactivating mutations plot</h4>\n");
            printf("%s<BR>\n", info->svg_line);

            // GLP features
            printf("<a data-toggle=\"collapse\" href=\"#collapseGLP\">Show GLP features</a>\n");
            printf("<div id=\"collapseGLP\" class=\"panel-collapse collapse\">\n");
            printf("<ul>\n");
            printf("<li>Percent intact ignoring missing seq: %s</li>\n", info->perc_intact_ign_M);
            printf("<li>Percent intact (miss == intact): %s</li>\n", info->perc_intact_int_M);
            printf("<li>Intact codon proportion %s</li>\n", info->intact_codon_prop);
            printf("<li>Out of chain proportion: %s</li>\n", info->ouf_prop);
            if (sameWord(info->mid_intact, ONE_))
            {
                printf("<li>Middle 80 percent intact: %s</li>\n", YES_);
            } else {
                printf("<li>Middle 80 percent intact: %s</li>\n", NO_);
            }
            if (sameWord(info->mid_pres, ONE_))
            {
                printf("<li>Middle 80 percent present: %s</li>\n", YES_);
            } else {
                printf("<li>Middle 80 percent present: %s</li>\n", NO_);
            }
            printf("</ul>\n</div>\n<BR>\n");

            // and show protein sequence
            htmlHorizontalLine();
            printf("<h4>Protein sequence</h4><BR>\n");
            printf("<a data-toggle=\"collapse\" href=\"#collapseProt\">Show protein alignment</a>\n");
            printf("<div id=\"collapseProt\" class=\"panel-collapse collapse\">\n");
            printf("<TT>%s</TT><BR>\n", info->prot_alignment);
            printf("</div>\n<BR><BR>\n");

            // do not forget to free toga data struct
            togaDataFree(&info);
        } else {
            // no data found, need to report this
            printf("Not found data for %s\n", item);
        }
        sqlFreeResult(&sr);
    }

    // show inactivating mutations if required
    printf("<h4>Inactivating mutations</h4><BR>\n");

    if (hTableExists(database, togaInactMutTableName))
    {
        char query[256];
        struct sqlResult *sr = NULL;
        char **row;
        sqlSafef(query, sizeof(query), "select * from %s where transcript='%s'", togaInactMutTableName, item);
        sr = sqlGetResult(conn, query);
        printf("<a data-toggle=\"collapse\" href=\"#collapseMuts\">Show inactivating mutations</a>\n");
        printf("<div id=\"collapseMuts\" class=\"panel-collapse collapse\">\n");
        printf("<table border = \"1\" width = \"640\">\n");  // init table
        printf("<tr><th>exon</th><th>pos</th><th>m_class</th><th>mut</th><th>is_inact</th><th>mut_id</th>\n");
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
    printf("<h4>Exons data</h4><BR>\n");

    if (hTableExists(database, togaNuclTableName))
    {
        char query[256];
        struct sqlResult *sr = NULL;
        char **row;
        printf("<a data-toggle=\"collapse\" href=\"#collapseExons\">Show exon sequences and features</a>\n");
        printf("<div id=\"collapseExons\" class=\"panel-collapse collapse\">\n");
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
                printf("<B>Detected within expected region:</B> %s<BR>\n", YES_);
            } else {
                printf("<B>Detected within expected region:</B> %s<BR>\n", NO_);
            }
            printf("<B>Expected region:</B> %s<BR>\n", info->exp_region);
            printf("<BR>\n");
            printf("<B>Sequence alignment:</B><BR>\n");
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
    hPrintf("<link rel=\"stylesheet\" href=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.4.1/css/bootstrap.min.css\">");
    hPrintf("<script src=\"https://ajax.googleapis.com/ajax/libs/jquery/3.5.1/jquery.min.js\"></script>");
    hPrintf("<script src=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.4.1/js/bootstrap.min.js\"></script>");


    printTrackHtml(tdb);  // and do I need this?
    hFreeConn(&conn);
}

