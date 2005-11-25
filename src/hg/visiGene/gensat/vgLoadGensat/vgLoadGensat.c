/* vgLoadGensat - Parse gensat XML file and turn it into VisiGene load files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "xp.h"
#include "xap.h"
#include "../lib/gs.h"

static char const rcsid[] = "$Id: vgLoadGensat.c,v 1.5 2005/11/25 05:44:50 kent Exp $";

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

void makeTabFile(char *gensatXml, char *tabFile)
/* Parse XML file and turn it into tab-separated file. */
{
struct xap *xap = xapListOpen(gensatXml, "GensatImageSet",
    gsStartHandler, gsEndHandler);
struct gsGensatImage *image;
struct hash *tabFileHash = hashNew(0);
FILE *f = mustOpen(tabFile, "w");
int i=0;
char *headDir = "Institutions/";
int headDirLen = strlen(headDir);

fprintf(f, "#");
fprintf(f, "submitId\t");
fprintf(f, "gene\t");
fprintf(f, "locusLink\t");
fprintf(f, "genbank\t");
fprintf(f, "bac\t");
fprintf(f, "sex\t");
fprintf(f, "age\t");
fprintf(f, "sliceType\t");
fprintf(f, "imageWidth\t");
fprintf(f, "imageHeight\t");
fprintf(f, "fileName\n");

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
    int locusLinkId = 0;
    char *acc = "";
    char *rawAge = NULL;
    double age = 0;
    char *sex = "";
    char *bac = "";
    int daysToBirth = 19;
    int daysToAdult = 61;
    int averageAdult = 100;

    struct gsGensatImageSeqInfo *gsGensatImageSeqInfo;	/** Single instance required. **/
    char *sliceType = image->gsGensatImageSectionPlane->value;	/** Single instance required. **/
    char *level = image->gsGensatImageSectionLevel->text;	/** Single instance required. **/

    if (image->gsGensatImageGeneInfo->gsGensatGeneInfo->gsGensatGeneInfoGeneSymbol != NULL)
	symbol = image->gsGensatImageGeneInfo->gsGensatGeneInfo->gsGensatGeneInfoGeneSymbol->text;	
    if (image->gsGensatImageGeneInfo->gsGensatGeneInfo->gsGensatGeneInfoGeneName != NULL)
	name = image->gsGensatImageGeneInfo->gsGensatGeneInfo->gsGensatGeneInfoGeneName->text;	
    if (image->gsGensatImageGeneInfo->gsGensatGeneInfo->gsGensatGeneInfoGeneId != NULL)
	locusLinkId = image->gsGensatImageGeneInfo->gsGensatGeneInfo->gsGensatGeneInfoGeneId->text;	
    if (image->gsGensatImageGeneInfo->gsGensatGeneInfo->gsGensatGeneInfoBacAddress != NULL)
        bac = image->gsGensatImageGeneInfo->gsGensatGeneInfo->gsGensatGeneInfoBacAddress->text;
    if (image->gsGensatImageSeqInfo->gsGensatSequenceInfo->gsGensatSequenceInfoAccession != NULL)
	acc = image->gsGensatImageSeqInfo->gsGensatSequenceInfo->gsGensatSequenceInfoAccession->text;	
    if (image->gsGensatImageAge != NULL)
        rawAge = image->gsGensatImageAge->value;
    if (image->gsGensatImageSex != NULL)
        sex = image->gsGensatImageSex->value;
    if (image->gsGensatImageGeneInfo->gsGensatGeneInfo->gsGensatGeneInfoGeneSymbol != NULL)
	symbol = image->gsGensatImageGeneInfo->gsGensatGeneInfo->gsGensatGeneInfoGeneSymbol->text;	

    /* Process a few fields. */
    tolowers(sex);
    if (sameString(rawAge, "adult"))
        age = averageAdult;
    else 
	{
	if (!isdigit(rawAge[1]))
	    errAbort("Don't understand age %s", rawAge);
	if (rawAge[0] == 'p')
	    age = daysToBirth + atof(rawAge+1);
	else if (rawAge[0] == 'e')
	    age = atof(rawAge+1);
	else
	    errAbort("Don't understand age %s", rawAge);
	}
    if (!startsWith(headDir, fileName))
        errAbort("Expecting %s to begin with %s", fileName, headDir);
    fileName += headDirLen;

    /* Print out fields */
    fprintf(f, "%d\t", id);
    fprintf(f, "%s\t", symbol);
    fprintf(f, "%d\t", locusLinkId);
    fprintf(f, "%s\t", acc);
    fprintf(f, "%s\t", bac);
    fprintf(f, "%s\t", sex);
    if (rawAge != NULL)
	fprintf(f, "%f\t", age);
    else
        fprintf(f, "\t");
    fprintf(f, "%s\t", sliceType);
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

void makeRaFile(char *fileName)
/* Make up ra file - that describes whole gensat set. */
{
FILE *f = mustOpen(fileName, "w");

fprintf(f, "submissionSet gensat1\n");
fprintf(f, "fullDir ../visiGene/full/inSitu/Mouse/gensat\n");
fprintf(f, "thumbDir ../visiGene/200/inSitu/Mouse/gensat\n");
fprintf(f, "priority 2000\n");
fprintf(f, "submissionSource gensat\n");
fprintf(f, "acknowledgement Thanks to Michael Dicuccio at NCBI for helping "
	   "load these images into VisiGene.\n");
fprintf(f, "contributor Heintz N., Curran T., Hatten M., Magdaleno S., Jensen P., Gong S., Mehta S., Wang C., Concepcion A., Kowalic E., Losos K., Feng H., Thompson T., Ford B., Baker S., Doughty M., Dinzey J., Dyer A., Grevstad C., Hinkle A., Kizima L., Madden C., Pariser E., Zheng C., Kus L., Milosevic A., Didkovsky N., Nowak N., Joyner A., Lehman K., Cheung T., Asbury A., Eden C., Batten D.,\n");
fprintf(f, "publication A gene expression atlas of the central nervous system based on bacterial artificial chromosomes\n");
fprintf(f, "journal Nature\n");
fprintf(f, "journalUrl http://www.nature.com\n");
fprintf(f, "year 2003\n");
fprintf(f, "setUrl http://www.ncbi.nlm.nih.gov/projects/gensat/\n");
fprintf(f, "itemUrl http://www.ncbi.nlm.nih.gov/entrez/query.fcgi?CMD=search&DB=gensat&term=%%s\n");


carefulClose(&f);
}


void vgLoadGensat(char *gensatXml, char *outDir)
/* vgLoadGensat - Parse gensat XML file and turn it into VisiGene load files. */
{
char tabFile[PATH_LEN], raFile[PATH_LEN];;

makeDir(outDir);
safef(tabFile, sizeof(tabFile), "%s/gensat.tab", outDir);
safef(raFile, sizeof(raFile), "%s/gensat.ra", outDir);
makeRaFile(raFile);
makeTabFile(gensatXml, tabFile);
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
