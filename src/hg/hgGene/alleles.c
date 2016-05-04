// alleles - common Gene haplotype alleles. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "web.h"
#include "genePredReader.h"
#include "vcf.h"
#include "haplotypes.h"
#include "hgGene.h"

static struct allelesInfo {
    struct genePred *gp;
    struct vcfFile *vcf;
    struct haploExtras *he;
    } allelesInfo;

static boolean allelesExists(struct section *section,
        struct sqlConnection *conn, char *geneId)
// Return TRUE if common haplotype alleles exist.
{
// If there was a reset then clear out settings
if (cartUsualBoolean(cart, HAPLO_RESET_ALL, FALSE))
    cartRemovePrefix(cart,HAPLO_TABLE "_" );

// Start with the default variables for haplotype retrieval
struct haploExtras *he = haplotypeExtrasDefault(database, 0);

// Need to get genePred struct from geneId
char where[128];
sqlSafefFrag(where, sizeof(where),"name = '%s'",geneId);
struct genePred *gp = genePredReaderLoadQuery(conn,he->geneTable, where);
if (gp == NULL || gp->cdsStart == gp->cdsEnd)  // Ain't interested in non-protein coding genes
    {
    haplotypeExtrasFree(&he);
    return FALSE;
    }

he->chrom = gp->chrom; // Probably not needed
he->justModel = lmCloneString(he->lm, geneId);
//he->growTree = FALSE; // Tree growing not needed here

// Need to determine the correct vcf file and open it
if (haplotypesDiscoverVcfFile(he, gp->chrom) == NULL)
    {
    haplotypeExtrasFree(&he);
    return FALSE;
    }
struct vcfFile *vcf = vcfTabixFileMayOpen(he->inFile, NULL, 0, 0,VCF_IGNORE_ERRS, 0);
if (vcf == NULL)
    {
    haplotypeExtrasFree(&he);
    return FALSE;
    }
allelesInfo.gp = gp;
allelesInfo.he = he;
allelesInfo.vcf = vcf;
section->items = &allelesInfo;
return TRUE;
}

static void allelesPrint(struct section *section, struct sqlConnection *conn, char *geneId)
// Print out common gene haplotype alleles.
{
struct allelesInfo *info = section->items;
struct vcfFile *vcf = info->vcf;
struct haploExtras *he = info->he;
struct genePred *gp = info->gp;

vcfFileMakeReusePool(vcf,1024 * 1024);

// All or Limit to the 99%
boolean rareVars =  cartUsualBoolean(cart, HAPLO_RARE_VAR, FALSE);
if (rareVars)
    he->synonymous = TRUE;
else
    he->variantMinPct = HAPLO_COMMON_VARIANT_MIN_PCT;

// Lets show the population distribution
he->populationsToo   = cartUsualBoolean(cart, HAPLO_MAJOR_DIST, FALSE);
he->populationsMinor = cartUsualBoolean(cart, HAPLO_MINOR_DIST, FALSE);
if (he->populationsToo)
    he->populationMinPct = 5;
else if (he->populationsMinor)
    {
    he->populationsMinor = FALSE;
    cartRemove(cart, HAPLO_MINOR_DIST );
    }

// Need to generate haplotypes
struct haplotypeSet *hapSet = geneHapSetForOneModel(he, vcf, gp, TRUE);
geneAllelesTableAndControls(cart, geneId, he, hapSet);
haplotypeExtrasFree(&he);
}

struct section *allelesSection(struct sqlConnection *conn, struct hash *sectionRa)
// Create UCSC KG Method section.
{
struct section *section = sectionNew(sectionRa, HGG_GENE_ALLELES);
if (section != NULL)
    {
    section->exists = allelesExists;
    section->print = allelesPrint;
    }
return section;
}

