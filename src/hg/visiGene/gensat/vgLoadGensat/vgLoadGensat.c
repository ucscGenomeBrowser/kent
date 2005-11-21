/* vgLoadGensat - Parse gensat XML file and turn it into VisiGene load files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "xp.h"
#include "xap.h"
#include "../lib/gs.h"

static char const rcsid[] = "$Id: vgLoadGensat.c,v 1.4 2005/11/21 17:38:46 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "vgLoadGensat - Parse gensat XML file and turn it into VisiGene load files\n"
  "usage:\n"
  "   vgLoadGensat gensat.xml out.tab\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

char *levelAsNumber(char *level)
/* Return ascii version of floating point number between
 * 0 and 1, or NULL, depending on level string. */
{
if (sameString(level, "undetectable"))
    return "0";
else if (sameString(level, "weak"))
    return "0.5";
else if (sameString(level, "strong"))
    return "1.0";
else
    return NULL;
}

char *blankOutNotDone(char *s)
/* Return s, unless it's "not-done", in which
 * case return "". */
{
if (sameString(s, "not-done"))
    s = "";
return s;
}


void vgLoadGensat(char *gensatXml, char *outTab)
/* vgLoadGensat - Parse gensat XML file and turn it into VisiGene load files. */
{
struct xap *xap = xapListOpen(gensatXml, "GensatImageSet",
    gsStartHandler, gsEndHandler);
struct gsGensatImage *image;
FILE *f = mustOpen(outTab, "w");
int i=0;

while ((image = xapListNext(xap, "GensatImage")) != NULL)
    {
    /* Fish fields we want out of image and info. */
    struct gsGensatImageImageInfo *info = image->gsGensatImageImageInfo;	/** Single instance required. **/
    struct gsGensatImageInfo *fileInfo = info->gsGensatImageImageInfoFullImg->gsGensatImageInfo;	/** Single instance required. **/
    struct gsGensatImageAnnotations *annotations;
    char *fileName = fileInfo->gsGensatImageInfoFilename->text;	/** Single instance required. **/
    int width = fileInfo->gsGensatImageInfoWidth->text;
    int height = fileInfo->gsGensatImageInfoHeight->text;
    int id = image->gsGensatImageId->text;	/** Single instance required. **/
    char *symbol = "";
    char *name = "";
    int geneId = 0;
    char *acc = "";
    char *bac = NULL;
    struct gsGensatImageSeqInfo *gsGensatImageSeqInfo;	/** Single instance required. **/
    char *section = image->gsGensatImageSectionPlane->value;	/** Single instance required. **/
    char *level = image->gsGensatImageSectionLevel->text;	/** Single instance required. **/

    if (image->gsGensatImageGeneInfo->gsGensatGeneInfo->gsGensatGeneInfoGeneSymbol != NULL)
	symbol = image->gsGensatImageGeneInfo->gsGensatGeneInfo->gsGensatGeneInfoGeneSymbol->text;	
    if (image->gsGensatImageGeneInfo->gsGensatGeneInfo->gsGensatGeneInfoGeneName != NULL)
	name = image->gsGensatImageGeneInfo->gsGensatGeneInfo->gsGensatGeneInfoGeneName->text;	
    if (image->gsGensatImageGeneInfo->gsGensatGeneInfo->gsGensatGeneInfoGeneId != NULL)
	geneId = image->gsGensatImageGeneInfo->gsGensatGeneInfo->gsGensatGeneInfoGeneId->text;	
    if (image->gsGensatImageGeneInfo->gsGensatGeneInfo->gsGensatGeneInfoBacAddress != NULL)
        bac = image->gsGensatImageGeneInfo->gsGensatGeneInfo->gsGensatGeneInfoBacAddress->text;
    if (image->gsGensatImageSeqInfo->gsGensatSequenceInfo->gsGensatSequenceInfoAccession != NULL)
	acc = image->gsGensatImageSeqInfo->gsGensatSequenceInfo->gsGensatSequenceInfoAccession->text;	
    /* Print out fields */
    fprintf(f, "%d\t", id);
    fprintf(f, "%s\t", symbol);
    // fprintf(f, "%s\t", name);
    fprintf(f, "%d\t", geneId);
    fprintf(f, "%s\t", acc);
    fprintf(f, "%s\t", bac);
    fprintf(f, "%s\t", section);
    fprintf(f, "%s\t", level);
    fprintf(f, "%d\t", width);
    fprintf(f, "%d\t", height);
    fprintf(f, "%s\n", fileName);

    /* Print out expression info if any. */
    if ((annotations = image->gsGensatImageAnnotations) != NULL)
        {
	struct gsGensatAnnotation *ann ;
	for (ann = annotations->gsGensatAnnotation; ann != NULL; ann = ann->next)
	    {
	    char *level = ann->gsGensatAnnotationExpressionLevel->value;
	    char *pattern = ann->gsGensatAnnotationExpressionPattern->value;
	    char *region = "";
	    char *cellType = "";
	    char *cellSubtype = "";
	    if (ann->gsGensatAnnotationRegion != NULL)
	        region = ann->gsGensatAnnotationRegion->text;
	    if (ann->gsGensatAnnotationCellType != NULL)
	        cellType = ann->gsGensatAnnotationCellType->text;
	    if (ann->gsGensatAnnotationCellSubtype != NULL)
	        cellSubtype = ann->gsGensatAnnotationCellSubtype->text;
	    level = levelAsNumber(level);
	    cellType = blankOutNotDone(cellType);
	    cellSubtype = blankOutNotDone(cellSubtype);
	    if (level != NULL)
		fprintf(f, "\texpression\t%s\t%s\t%s\t%s\t%s\n", 
		    region, level, pattern, cellType, cellSubtype);
	    }
	}
    gsGensatImageFree(&image);
    }
xapFree(&xap);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
vgLoadGensat(argv[1], argv[2]);
return 0;
}
