/* printCaption - query database for info and print it out
 * in a caption. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hash.h"
#include "dystring.h"
#include "jksql.h"
#include "cart.h"
#include "htmshell.h"
#include "hdb.h"
#include "visiGene.h"
#include "hgVisiGene.h"
#include "captionElement.h"
#include "printCaption.h"

struct probeAndColor
/* Just a little structure to store probe and probeColor. */
    {
    struct probeAndColor *next;
    int probe;	/* Probe id. */
    int probeColor;  /* ProbeColor id. */
    };


char *getKnownGeneUrl(struct sqlConnection *conn, int geneId)
/* Given gene ID, try and find known gene on browser in same
 * species. */
{
char query[256];
char tableName[256];
int taxon;
char *url = NULL;
char *genomeDb = NULL;

/* Figure out taxon. */
sqlSafef(query, sizeof(query), 
    "select taxon from gene where id = %d", geneId);
taxon = sqlQuickNum(conn, query);

genomeDb = hDbForTaxon(taxon);
if (genomeDb != NULL)
    {
    /* Make sure known genes track exists - we may need
     * to tweak this at some point for model organisms. */
    safef(tableName, sizeof(tableName), "%s.knownToVisiGene", genomeDb);
    if (!sqlTableExists(conn, tableName))
	genomeDb = NULL;
    }

/* If no db for that organism revert to human. */
if (genomeDb == NULL)
    genomeDb = hDefaultDb();

safef(tableName, sizeof(tableName), "%s.knownToVisiGene", genomeDb);
if (sqlTableExists(conn, tableName))
    {
    struct dyString *dy = dyStringNew(0);
    char *knownGene = NULL;
    if (sqlCountColumnsInTable(conn, tableName) == 3)
	{
	sqlDyStringPrintf(dy, 
	   "select name from %s.knownToVisiGene where geneId = %d", genomeDb, geneId);
	}
    else
	{
	struct slName *imageList, *image;
	sqlSafef(query, sizeof(query), 
	    "select imageProbe.image from probe,imageProbe "
	    "where probe.gene=%d and imageProbe.probe=probe.id", geneId);
	imageList = sqlQuickList(conn, query);
	if (imageList != NULL)
	    {
	    sqlDyStringPrintf(dy, 
	       "select name from %s.knownToVisiGene ", genomeDb);
	    dyStringAppend(dy,
	       "where value in(");
	    for (image = imageList; image != NULL; image = image->next)
		{
		sqlDyStringPrintf(dy, "'%s'", image->name);
		if (image->next != NULL)
		    dyStringAppendC(dy, ',');
		}
	    dyStringAppend(dy, ")");
	    slFreeList(&imageList);
	    }
	}
    if (dy->stringSize > 0)
	{
	knownGene = sqlQuickString(conn, dy->string);
	if (knownGene != NULL)
	    {
	    char temp[1024];
	    safef(temp, sizeof temp, "../cgi-bin/hgGene?db=%s&hgg_gene=%s&hgg_chrom=none",
		genomeDb, knownGene);
	    url = cloneString(temp);
	    }
	}
    dyStringFree(&dy);
    }
freez(&genomeDb);
return url;
}

static struct slName *geneProbeList(struct sqlConnection *conn, int id)
/* Get list of gene names with hyperlinks to probe info page. */
{
struct slName *returnList = NULL;
char query[256], **row;
struct sqlResult *sr;
struct dyString *dy = dyStringNew(0);
struct probeAndColor *pcList = NULL, *pc;
int probeCount = 0;

/* Make up a list of all probes in this image. */
sqlSafef(query, sizeof(query),
   "select probe,probeColor from imageProbe where image=%d", id);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(pc);
    pc->probe = sqlUnsigned(row[0]);
    pc->probeColor = sqlUnsigned(row[1]);
    slAddHead(&pcList, pc);
    ++probeCount;
    }
slReverse(&pcList);

for (pc = pcList; pc != NULL; pc = pc->next)
    {
    int geneId;
    char *geneName;
    int probe = pc->probe;
    char *geneUrl = NULL;

    /* Get gene ID and name. */
    sqlSafef(query, sizeof(query), 
    	"select gene from probe where id = %d", probe);
    geneId = sqlQuickNum(conn, query);
    geneName = vgGeneNameFromId(conn, geneId);
    
    /* Get url for known genes page if any. */
    geneUrl = getKnownGeneUrl(conn, geneId);

    /* Print gene name, surrounded by hyperlink to known genes
     * page if possible. */
    dyStringClear(dy);
    if (geneUrl != NULL)
	dyStringPrintf(dy, "<A HREF=\"%s\" target=_parent>",
	    geneUrl);
    dyStringPrintf(dy, "%s", geneName);
    if (geneUrl != NULL)
	dyStringAppend(dy, "</A>");
    freez(&geneName);

    /* Add color if there's more than one probe for this image. */
    if (probeCount > 1)
        {
	char *color;
	sqlSafef(query, sizeof(query), 
	    "select probeColor.name from probeColor "
	    "where probeColor.id = %d"
	    , pc->probeColor);
	color = sqlQuickString(conn, query);
	if (color != NULL)
	    dyStringPrintf(dy, " (%s)", color);
	freez(&color);
	}

    /* Add to return list. */
    slNameAddTail(&returnList, dy->string);
    }

slFreeList(&pcList);
slReverse(&returnList);
return returnList;
}

static struct slName *getProbeList(struct sqlConnection *conn, int id)
/* Get list of probes with hyperlinks to probe info page. */
{
struct slName *returnList = NULL;
char query[256];
char *sidUrl = cartSidUrlString(cart);
struct dyString *dy = dyStringNew(0);
struct slInt *probeList = NULL, *probe;
int submissionSource = 0;

/* Make up a list of all probes in this image. */
sqlSafef(query, sizeof(query),
   "select probe from imageProbe where image=%d", id);
probeList = sqlQuickNumList(conn, query);

sqlSafef(query, sizeof(query),
   "select submissionSet.submissionSource from image, submissionSet"
   " where image.submissionSet = submissionSet.id and image.id=%d", id);
submissionSource = sqlQuickNum(conn, query);

for (probe = probeList; probe != NULL; probe = probe->next)
    {
    char *type;

    /* Create hyperlink to probe page around gene name. */
    dyStringClear(dy);
    dyStringPrintf(dy, "<A HREF=\"%s?%s&%s=%d&%s=%d\" target=_parent>",
    	hgVisiGeneCgiName(), sidUrl, hgpDoProbe, probe->val, hgpSs, submissionSource);
    sqlSafef(query, sizeof(query), 
    	"select probeType.name from probeType,probe where probe.id = %d "
	"and probe.probeType = probeType.id", 
	probe->val);
    type = sqlQuickString(conn, query);
    dyStringPrintf(dy, "%s", naForEmpty(type));
    if (sameWord(type, "antibody"))
        {
	char *abName;
	sqlSafef(query, sizeof(query), 
	   "select antibody.name from probe,antibody "
	   "where probe.id = %d and probe.antibody = antibody.id"
	   , probe->val);
	abName = sqlQuickString(conn, query);
	if (abName != NULL)
	    {
	    dyStringPrintf(dy, " %s", abName);
	    freeMem(abName);
	    }
	}
    else if (sameWord(type, "RNA"))
        {
	sqlSafef(query, sizeof(query),
	    "select length(seq) from probe where id=%d", probe->val);
	if (sqlQuickNum(conn, query) > 0)
	    dyStringPrintf(dy, " sequenced");
	else
	    {
	    sqlSafef(query, sizeof(query),
		"select length(fPrimer) from probe where id=%d", probe->val);
	    if (sqlQuickNum(conn, query) > 0)
	        dyStringPrintf(dy, " from primers");
	    }
	}
    else if (sameWord(type, "BAC"))
        {
	char *name;
	sqlSafef(query, sizeof(query), 
	   "select bac.name from probe,bac "
	   "where probe.id = %d and probe.bac = bac.id"
	   , probe->val);
	name = sqlQuickString(conn, query);
	if (name != NULL)
	    {
	    dyStringPrintf(dy, " %s", name);
	    freeMem(name);
	    }
	}
    dyStringPrintf(dy, "</A>");
    freez(&type);

    /* Add to return list. */
    slNameAddTail(&returnList, dy->string);
    }

slFreeList(&probeList);
slReverse(&returnList);
return returnList;
}

#ifdef UNUSED
static char *genomeDbForImage(struct sqlConnection *conn, int imageId)
/* Return the genome database to associate with image or NULL if none. */
{
char *db;
char *scientificName = visiGeneOrganism(conn, imageId);
struct sqlConnection *cConn = hConnectCentral();
char query[256];

sqlSafef(query, sizeof(query), 
	"select defaultDb.name from defaultDb,dbDb where "
	"defaultDb.genome = dbDb.organism and "
	"dbDb.active = 1 and "
	"dbDb.scientificName = '%s'" 
	, scientificName);
db = sqlQuickString(cConn, query);
hDisconnectCentral(&cConn);
return db;
}
#endif /* UNUSED */

struct slName *vgImageFileGenes(struct sqlConnection *conn, int imageFile)
/* Return alphabetical list of all genes associated with image file. */
{
struct hash *uniqHash = hashNew(0);
struct slInt *imageList, *image;
struct slName *geneList = NULL, *gene;

imageList = visiGeneImagesForFile(conn, imageFile);
for (image = imageList; image != NULL; image = image->next)
    {
    struct slName *oneList, *next;
    oneList = visiGeneGeneName(conn, image->val);
    for (gene = oneList; gene != NULL; gene = next)
        {
	next = gene->next;
	if (hashLookup(uniqHash, gene->name))
	    freeMem(gene);
	else
	    {
	    hashAdd(uniqHash, gene->name, NULL);
	    slAddHead(&geneList, gene);
	    }
	}
    }
slFreeList(&imageList);
hashFree(&uniqHash);
slSort(&geneList, slNameCmp);
return geneList;
}

struct slName *vgImageFileGenotypes(struct sqlConnection *conn, int imageFile)
/* Return unique alphabetical list of all genotypes associated with image file. */
{
struct hash *uniqHash = hashNew(0);
struct slInt *imageList, *image;
struct slName *genoList=NULL,*genotype=NULL;

imageList = visiGeneImagesForFile(conn, imageFile);
for (image = imageList; image != NULL; image = image->next)
    {
    struct slName *oneList, *next;
    oneList = visiGeneGenotypes(conn, image->val);
    for (genotype = oneList; genotype != NULL; genotype = next)
        {
	next = genotype->next;
	if (hashLookup(uniqHash, genotype->name))
	    freeMem(genotype);
	else
	    {
	    hashAdd(uniqHash, genotype->name, NULL);
	    slAddHead(&genoList, genotype);
	    }
	}
    }
slFreeList(&imageList);
hashFree(&uniqHash);
slSort(&genoList, slNameCmp);
return genoList;
}

struct slName *vgImageFileOrganisms(struct sqlConnection *conn, int imageFile)
/* Return unique alphabetical list of all organisms associated with image file. */
{
struct hash *uniqHash = hashNew(0);
struct slInt *imageList, *image;
struct slName *orgList=NULL;

imageList = visiGeneImagesForFile(conn, imageFile);
for (image = imageList; image != NULL; image = image->next)
    {
    char *organism = visiGeneOrganism(conn, image->val);
    char *org = cloneString(shortOrgName(organism));
    freeMem(organism);
    if (hashLookup(uniqHash, org))
	freeMem(org);
    else
	{
	hashAdd(uniqHash, org, NULL);
	slNameAddHead(&orgList, org);
	}
    }
slFreeList(&imageList);
hashFree(&uniqHash);
slSort(&orgList, slNameCmp);
return orgList;
}

void smallCaption(struct sqlConnection *conn, int imageId)
/* Write out small format caption. */
{
struct slName *geneList, *gene;
struct slName *genotypeList, *genotype;
struct slName *orgList, *org;
int imageFile = visiGeneImageFile(conn, imageId);
boolean mutant = FALSE;
char *sep = "";

genotypeList = vgImageFileGenotypes(conn, imageFile);
for (genotype = genotypeList; genotype != NULL; genotype = genotype->next)
    {
    if (!sameString(genotype->name,"wild type"))
	mutant = TRUE;
    }
slFreeList(&genotypeList);
if (mutant)
    printf("Mutant ");

orgList = vgImageFileOrganisms(conn, imageFile);
for (org = orgList; org != NULL; org = org->next)
    {
    printf("%s%s", sep, org->name);
    sep = " ";
    }
slFreeList(&orgList);


geneList = vgImageFileGenes(conn, imageFile);
for (gene = geneList; gene != NULL; gene = gene->next)
    {
    printf(" %s", gene->name);
    }
slFreeList(&geneList);

}



static struct slName *expTissuesForProbeInImage(struct sqlConnection *conn, 
	int imageId,
	int probeId)
/* Get list of tissue where we have expression info in gene.
 * Put + or - depending on expression level. */
{
struct dyString *dy = dyStringNew(0);
struct slName *tissueList = NULL, *tissue;
char query[512], **row;
struct sqlResult *sr;
sqlSafef(query, sizeof(query),
   "select bodyPart.name,expressionLevel.level,expressionPattern.description "
   "from expressionLevel join bodyPart join imageProbe "
   "left join expressionPattern on expressionLevel.expressionPattern = expressionPattern.id "
   "where imageProbe.image = %d "
   "and imageProbe.probe = %d "
   "and imageProbe.id = expressionLevel.imageProbe "
   "and expressionLevel.bodyPart = bodyPart.id "
   "order by bodyPart.name"
   , imageId, probeId);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    double level = atof(row[1]);
    char *pattern = row[2];
    if (pattern)
    	tolowers(pattern);
    dyStringClear(dy);
    dyStringAppend(dy, row[0]);
    if (level == 1.0)
       dyStringAppend(dy, "(+)");
    else if (level == 0.0)
       dyStringAppend(dy, "(-)");
    else 
       dyStringPrintf(dy, "(%.2f)",level);
    if (pattern && !sameWord(pattern,"Not Applicable") && !sameWord(pattern,"Not Specified"))
    	dyStringPrintf(dy, " %s",pattern);
    tissue = slNameNew(dy->string);
    slAddHead(&tissueList, tissue);
    }
sqlFreeResult(&sr);
slReverse(&tissueList);
dyStringFree(&dy);
return tissueList;
}

struct expInfo 
/* Info on expression in various tissues for a particular gene. */
    {
    struct expInfo *next;	/* Next in list. */
    int probeId;		/* Id in probe table. */
    int geneId;			/* Id in gene table. */
    char *geneName;		/* Name of gene. */
    struct slName *tissueList;  /* List of tissues where gene expressed */
    };

#ifdef UNUSED
static void expInfoFree(struct expInfo **pExp)
/* Free up memory associated with expInfo. */
{
struct expInfo *exp = *pExp;
if (exp != NULL)
    {
    freeMem(exp->geneName);
    slFreeList(&exp->tissueList);
    freez(pExp);
    }
}

static void expInfoFreeList(struct expInfo **pList)
/* Free a list of dynamically allocated expInfo's */
{
struct expInfo *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    expInfoFree(&el);
    }
*pList = NULL;
}
#endif /* UNUSED */


static struct expInfo *expInfoForImage(struct sqlConnection *conn, int imageId)
/* Return list of expression info for a given pane. */
{
struct expInfo *expList = NULL, *exp;
char query[512], **row;
struct sqlResult *sr;

/* Get list of probes associated with image, and start building
 * expList around it. */
sqlSafef(query, sizeof(query), 
   "select imageProbe.probe,probe.gene from imageProbe,probe "
   "where imageProbe.image = %d "
   "and imageProbe.probe = probe.id", imageId);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(exp);
    exp->probeId = sqlUnsigned(row[0]);
    exp->geneId = sqlUnsigned(row[1]);
    slAddHead(&expList, exp);
    }
sqlFreeResult(&sr);
slReverse(&expList);

/* Finish filling in expInfo and return. */
for (exp = expList; exp != NULL; exp = exp->next)
    {
    exp->geneName = vgGeneNameFromId(conn, exp->geneId);
    exp->tissueList = expTissuesForProbeInImage(conn, imageId, exp->probeId);
    }

return expList;
}

static void addExpressionInfo(struct sqlConnection *conn, int imageId,
	struct captionElement **pCeList)
/* Add information about expression to caption element list.
 * Since the pane can contain multiple genes each with
 * separate caption information this is more complex than
 * the other caption elements.  We'll make one captionElement
 * for each gene that we have expression info on. */
{
int geneWithDataCount = 0, expCount = 0;
struct expInfo *exp, *expList = expInfoForImage(conn, imageId);
struct captionElement *ce;

/* Figure out how much data we really have.  */
for (exp = expList; exp != NULL; exp = exp->next)
    {
    if (exp->tissueList != NULL)
        ++geneWithDataCount;
    ++expCount;
    }

/* If no data just add n/a */
if (geneWithDataCount == 0)
    {
    ce = captionElementNew(imageId, "Expression", cloneString("n/a"));
    slAddHead(pCeList, ce);
    }
else
    {
    for (exp = expList; exp != NULL; exp = exp->next)
        {
	if (exp->tissueList != NULL)
	    {
	    char label[256];
	    if (expCount == 1)
	        safef(label, sizeof(label), "Expression");
	    else
	        safef(label, sizeof(label), "Expression of %s", exp->geneName);
	    ce = captionElementNew(imageId, label, 
	    	makeCommaSpacedList(exp->tissueList));
	    slAddHead(pCeList, ce);
	    }
	}
    }
}

static struct slName *wrapUrlList(struct slName *labelList, 
	struct slName *idList, char *url)
/* Return list that substitutes id into url and wraps
 * url as a hyperlink around label. */
{
struct slName *list = NULL, *label, *id;
struct dyString *dy = dyStringNew(0);
for (id = idList, label = labelList; id != NULL && label != NULL;
	id = id->next, label = label->next)
    {
    dyStringClear(dy);
    dyStringAppend(dy, "<A HREF=\"");
    dyStringPrintf(dy, url, id->name);
    dyStringAppend(dy, "\" target=_blank>");
    dyStringAppend(dy, label->name);
    dyStringAppend(dy, "</A>");
    slNameAddHead(&list, dy->string);
    }
slReverse(&list);
return list;
}

static char *getSimplifiedAllele(char *geneName, char *allele)
/* If allele is of form geneName<varient> then return just
 * varient.  Else return allele.  Do a freeMem on result when
 * done. */
{
int len = strlen(geneName);
if (startsWith(geneName, allele) && allele[len] == '<')
    {
    char *s = allele + len + 1;
    char *e = strrchr(s, '>');
    if (e == NULL) 
    	e = s + strlen(s);
    return cloneStringZ(s, e - s);
    }
else
    return cloneString(allele);
}

char *visiGeneHypertextGenotype(struct sqlConnection *conn, int id)
/* Return genotype of organism if any in nifty hypertext format. */
{
int genotypeId;
struct slName *geneIdList, *geneId;
char query[256];
struct dyString *html;

/* Look up genotype ID. */
sqlSafef(query, sizeof(query),
    "select specimen.genotype from image,specimen "
    "where image.id=%d and image.specimen = specimen.id", id);
genotypeId = sqlQuickNum(conn, query);
if (genotypeId == 0)
    return NULL;

/* Get list of genes involved. */
sqlSafef(query, sizeof(query),
    "select distinct allele.gene from genotypeAllele,allele "
    "where genotypeAllele.genotype=%d "
    "and genotypeAllele.allele = allele.id"
    , genotypeId);
geneIdList = sqlQuickList(conn, query);
if (geneIdList == NULL)
    return cloneString("wild type");

/* Loop through each gene adding information to html. */
html = dyStringNew(0);
for (geneId = geneIdList; geneId != NULL; geneId = geneId->next)
    {
    char *geneName;
    struct slName *alleleList, *allele;
    int alleleCount;
    boolean needsSlash = FALSE;

    /* Get gene name. */
    sqlSafef(query, sizeof(query), "select name from gene where id='%s'",
        geneId->name);
    geneName = sqlQuickString(conn, query);
    if (geneName == NULL)
        internalErr();

    /* Process each allele of gene. */
    sqlSafef(query, sizeof(query), 
    	"select allele.name from genotypeAllele,allele "
	"where genotypeAllele.genotype=%d "
	"and genotypeAllele.allele = allele.id "
	"and allele.gene=%s"
	, genotypeId, geneId->name);
    alleleList = sqlQuickList(conn, query);
    alleleCount = slCount(alleleList);
    for (allele = alleleList; allele != NULL; allele = allele->next)
        {
	char *simplifiedAllele = getSimplifiedAllele(geneName, allele->name);
	int repCount = 1, rep;
	if (alleleCount == 1)
	    repCount = 2;
	for (rep = 0; rep < repCount; ++rep)
	    {
	    if (needsSlash)
	        dyStringAppendC(html, '/');
	    else
	        needsSlash = TRUE;
	    dyStringAppend(html, geneName);
	    dyStringPrintf(html, "<SUP>%s</SUP>", simplifiedAllele);
	    }
	freeMem(simplifiedAllele);
	}

    if (geneId->next != NULL)
        dyStringAppendC(html, ' ');
    slFreeList(&alleleList);
    freeMem(geneName);
    }

slFreeList(&geneIdList);
return dyStringCannibalize(&html);
}


static struct captionElement *makePaneCaptionElements(
	struct sqlConnection *conn, struct slInt *imageList)
/* Make list of all caption elements */
{
struct slInt *image;
struct captionElement *ceList = NULL, *ce;
for (image = imageList; image != NULL; image = image->next)
    {
    int paneId = image->val;
    struct slName *geneList = geneProbeList(conn, paneId);
    struct slName *genbankRawList = visiGeneGenbank(conn, paneId);
    struct slName *genbankUrlList = wrapUrlList(genbankRawList, genbankRawList,
        "https://www.ncbi.nlm.nih.gov/entrez/query.fcgi?"
	"cmd=Search&db=Nucleotide&term=%s&doptcmdl=GenBank&tool=genome.ucsc.edu");
    struct slName *probeList = getProbeList(conn, paneId);
    ce = captionElementNew(paneId, "Gene", makeCommaSpacedList(geneList));
    ce->hasHtml = TRUE;
    slAddHead(&ceList, ce);
    ce = captionElementNew(paneId, "Probe", makeCommaSpacedList(probeList));
    ce->hasHtml = TRUE;
    slAddHead(&ceList, ce);
    ce = captionElementNew(paneId, "GenBank", makeCommaSpacedList(genbankUrlList));
    slAddHead(&ceList, ce);
    ce->hasHtml = TRUE;
    ce = captionElementNew(paneId, "Organism", visiGeneOrganism(conn, paneId));
    slAddHead(&ceList, ce);
    ce = captionElementNew(paneId, "Sex", 
    	naForNull(visiGeneSex(conn, paneId)));
    slAddHead(&ceList, ce);
    ce = captionElementNew(paneId, "Strain", 
    	naForNull(visiGeneStrain(conn, paneId)));
    slAddHead(&ceList, ce);
    ce = captionElementNew(paneId, "Genotype",
        naForNull(visiGeneHypertextGenotype(conn, paneId)));
    ce->hasHtml = TRUE;
    slAddHead(&ceList, ce);
    ce = captionElementNew(paneId, "Stage", visiGeneStage(conn, paneId, TRUE));
    slAddHead(&ceList, ce);
    ce = captionElementNew(paneId, "Body Part",
    	naForNull(visiGeneBodyPart(conn, paneId)));
    slAddHead(&ceList, ce);
    addExpressionInfo(conn, paneId, &ceList);
    ce = captionElementNew(paneId, "Section Type",
    	naForNull(visiGeneSliceType(conn, paneId)));
    slAddHead(&ceList, ce);
    ce = captionElementNew(paneId, "Permeablization",
    	naForNull(visiGenePermeablization(conn, paneId)));
    }
slReverse(&ceList);
return ceList;
}

static int nonTagStrlen(char *s)
/* Count up string length excluding everything between '<' and '>' 
 * Since this is a non-critical function just for layout purposes
 * it is simple, not dealing with corner cases like characters in
 * quotes. */
{
char c;
int count = 0;
boolean inTag = FALSE;
while ((c = *s++) != 0)
    {
    if (inTag)
        {
	if (c == '>')
	    inTag = FALSE;
	}
    else
        {
	if (c == '<')
	    inTag = TRUE;
	else
	    count += 1;
	}
    }
return count;
}

static void printCaptionElements(struct sqlConnection *conn, 
	struct captionElement *captionElements, struct slInt *imageList)
/* Print out caption elements - common elements first and then
 * pane-specific ones. */
{
struct captionBundle *bundleList, *bundle;
struct slRef *ref;
struct captionElement *ce;
int bundleCount;
int maxCharsInLine = 70;

bundleList = captionElementBundle(captionElements, imageList);
bundleCount = slCount(bundleList);
for (bundle = bundleList; bundle != NULL; bundle = bundle->next)
    {
    int charsInLine = 0, charsInEl;
    if (bundleCount > 1)
	printf("<HR>\n");
    if (bundle->image != 0)
        {
	char *label = visiGenePaneLabel(conn, bundle->image);
	if (label != NULL)
	   printf("<B>Pane %s</B><BR>\n", label);
	}
    else if (bundleCount > 1)
        {
	printf("<B>All Panes</B><BR>\n");
	}
    for (ref = bundle->elements; ref != NULL; ref = ref->next)
        {
	ce = ref->val;
	charsInEl = strlen(ce->type) + 2;
	if (ce->hasHtml)
	    charsInEl += nonTagStrlen(ce->value);
	else
	    charsInEl += strlen(ce->value);
	if (charsInLine != 0)
	    {
	    if (charsInLine + charsInEl > maxCharsInLine )
		{
		printf("<BR>\n");
		charsInLine = 0;
		}
	    else if (charsInLine != 0)
		printf(" ");
	    }
	printf("<B>%s:</B> ", ce->type);
	if (ce->hasHtml)
	    printf("%s", ce->value);
	else
	    htmTextOut(stdout, ce->value);
	charsInLine += charsInEl;
	}
    if (charsInLine != 0)
	printf("<BR>\n");
    }
if (bundleCount > 1)
    printf("<HR>\n");
}

static int visiGeneForwardedImageFile(struct sqlConnection *conn,
        int imageFile)
/* Given imageFile ID, return ID of imageFile with better
 * caption info.  If no such better info, just return zero. */
{
char query[256];
sqlSafef(query, sizeof(query),
        "select toIf from imageFileFwd where fromIf = %d "
        , imageFile);
return sqlQuickNum(conn, query);
}

static int vgForwardedImage(struct sqlConnection *conn, int image)
/* Return id of image with better caption information if available.
 * Otherwise return zero.  We are working in image ids instead of 
 * imageFile ids only because much of the software works off image ids.  */
{
int imageFile = visiGeneImageFile(conn, image);
int forwardedImage = 0;
int forwardedImageFile = visiGeneForwardedImageFile(conn, imageFile);
if (forwardedImageFile)
     {
     char query[256];
     sqlSafef(query, sizeof(query), 
     	"select id from image where imageFile=%d",
        forwardedImageFile);
     forwardedImage = sqlQuickNum(conn, query);
     }
return forwardedImage;
}

static void showSource(struct sqlConnection *conn, int id)
/* Put up source info for id if available. */
{
char *itemUrl = visiGeneItemUrl(conn, id);
if (itemUrl != NULL)
    {
    printf("<B>source:</B> ");
    char *source = visiGeneSubmissionSource(conn, id);
    if (sameString(itemUrl,""))
	printf("%s ", source);
    else
	{
	printf("<A HREF=\"");
	printf(itemUrl, visiGeneSubmitId(conn, id));
	printf("\" target=_blank>%s</A> ", source);
	}

    }
}

static void showAcknowledgement(struct sqlConnection *conn, int id)
/* Print acknowledgement info if any. */
{
char *acknowledgement = visiGeneAcknowledgement(conn, id);
if (acknowledgement != NULL)
    printf("<B>Acknowledgements:</B> %s<BR>\n", acknowledgement);
}

void fullCaption(struct sqlConnection *conn, int id)
/* Print information about image. */
{
char *publication, *copyright;
char *caption = NULL;
int imageFile = -1;
struct slInt *imageList;
int oldId = 0;
struct captionElement *captionElements;
int forwardedId = vgForwardedImage(conn, id);

if (forwardedId)
    {
    oldId = id;
    id = forwardedId;
    }

caption = visiGeneCaption(conn, id);
imageFile = visiGeneImageFile(conn, id);

showSource(conn, oldId);
showSource(conn, id);

publication = visiGenePublication(conn,id);
if (publication != NULL)
    {
    char *pubUrl = visiGenePubUrl(conn,id);
    printf("<B>Reference:</B> ");
    if (pubUrl != NULL && pubUrl[0] != 0)
        printf("<A HREF=\"%s\" target=_blank>%s</A>", pubUrl, publication);
    else
	{
        printf("%s", naForEmpty(publication));
	}
    printf("<BR>\n");
    }
printf("<B>Year:</B> %d ", visiGeneYear(conn,id));
printf("<B>Contributors:</B> %s<BR>\n", 
	naForNull(visiGeneContributors(conn,id)));
if (caption != NULL)
    {
    printf("<B>Notes:</B> %s<BR>\n", caption);
    freez(&caption);
    }
imageList = visiGeneImagesForFile(conn, imageFile);
captionElements = makePaneCaptionElements(conn, imageList);
printCaptionElements(conn, captionElements, imageList);

copyright = visiGeneCopyright(conn, id);
if (copyright != NULL)
    printf("<B>Copyright:</B> %s<BR>\n", copyright);
showAcknowledgement(conn, oldId);
showAcknowledgement(conn, id);

printf("<BR>\n");
}

