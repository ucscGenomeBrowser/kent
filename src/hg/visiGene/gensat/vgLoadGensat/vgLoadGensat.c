/* vgLoadGensat - Parse gensat XML file and turn it into VisiGene load files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "xp.h"
#include "xap.h"
#include "gs.h"

static char const rcsid[] = "$Id: vgLoadGensat.c,v 1.3 2005/11/19 03:25:03 kent Exp $";

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
struct gsGensatimageset *imageSet = gsGensatimagesetLoad(gensatXml);
struct gsGensatimage *image;
FILE *f = mustOpen(outTab, "w");

uglyf("Got %d images\n", slCount(imageSet->gsGensatimage));
for (image = imageSet->gsGensatimage; image != NULL; image = image->next)
    {
    /* Fish fields we want out of image and info. */
    struct gsGensatimageImageInfo *info = image->gsGensatimageImageInfo;	/** Single instance required. **/
    struct gsGensatimageinfo *fileInfo = info->gsGensatimageImageInfoFullImg->gsGensatimageinfo;	/** Single instance required. **/
    struct gsGensatimageAnnotations *annotations;
    char *fileName = fileInfo->gsGensatimageinfoFilename->text;	/** Single instance required. **/
    int width = fileInfo->gsGensatimageinfoWidth->text;
    int height = fileInfo->gsGensatimageinfoHeight->text;
    int id = image->gsGensatimageId->text;	/** Single instance required. **/
    char *symbol = image->gsGensatimageGeneSymbol->text;	/** Single instance required. **/
    char *name = image->gsGensatimageGeneName->text;	/** Single instance required. **/
    int geneId = 0;
    char *acc = "";
    char *section = image->gsGensatimageSectionPlane->value;	/** Single instance required. **/
    char *level = image->gsGensatimageSectionLevel->text;	/** Single instance required. **/
    if (image->gsGensatimageGeneId != NULL)
	geneId = image->gsGensatimageGeneId->text;	/** Optional (may be NULL). **/
    if (image->gsGensatimageGenbankAcc != NULL)
	acc = image->gsGensatimageGenbankAcc->text;	/** Optional (may be NULL). **/

    /* Print out fields */
    fprintf(f, "%d\t", id);
    fprintf(f, "%s\t", symbol);
    // fprintf(f, "%s\t", name);
    fprintf(f, "%d\t", geneId);
    fprintf(f, "%s\t", acc);
    fprintf(f, "%s\t", section);
    fprintf(f, "%s\t", level);
    fprintf(f, "%d\t", width);
    fprintf(f, "%d\t", height);
    fprintf(f, "%s\n", fileName);

    /* Print out expression info if any. */
    if ((annotations = image->gsGensatimageAnnotations) != NULL)
        {
	struct gsGensatannotation *ann ;
	for (ann = annotations->gsGensatannotation; ann != NULL; ann = ann->next)
	    {
	    char *level = ann->gsGensatannotationExpressionLevel->value;
	    char *pattern = ann->gsGensatannotationExpressionPattern->value;
	    char *region = "";
	    char *cellType = "";
	    char *cellSubtype = "";
	    if (ann->gsGensatannotationRegion != NULL)
	        region = ann->gsGensatannotationRegion->text;
	    if (ann->gsGensatannotationCellType != NULL)
	        cellType = ann->gsGensatannotationCellType->text;
	    if (ann->gsGensatannotationCellSubtype != NULL)
	        cellSubtype = ann->gsGensatannotationCellSubtype->text;
	    level = levelAsNumber(level);
	    cellType = blankOutNotDone(cellType);
	    cellSubtype = blankOutNotDone(cellSubtype);
	    if (level != NULL)
		fprintf(f, "\texpression\t%s\t%s\t%s\t%s\t%s\n", 
		    region, level, pattern, cellType, cellSubtype);
	    }
	}
    }
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
