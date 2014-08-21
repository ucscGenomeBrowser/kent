/* dnaseHg38AddTreatments - Add treatments to dnase hg38 metadata. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "dnaseHg38AddTreatments - Add treatments to dnase hg38 metadata\n"
  "usage:\n"
  "   dnaseHg38AddTreatments batch/meta.tab meta.tab\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void dnaseHg38AddTreatments(char *inTab, char *outTab)
/* dnaseHg38AddTreatments - Add treatments to dnase hg38 metadata. */
{
struct sqlConnection *conn = sqlConnect("hgFixed");
struct lineFile *lf = lineFileOpen(inTab, TRUE);
FILE *f = mustOpen(outTab, "w");
char *line;
while (lineFileNext(lf, &line, NULL))
    {
    if (line[0] == '#')
        fprintf(f, "%s\ttreatment\tlabel\n", line);
    else
        {
	char *inRow[5];
	int wordCount = chopByWhite(line, inRow, ArraySize(inRow));
	lineFileExpectWords(lf, 4, wordCount);
	char *acc = inRow[0];
	char *biosample = inRow[1];
	char query[512];
	sqlSafef(query, sizeof(query), "select expVars from encodeExp where accession = '%s'", acc);
	char varBuf[1024];
	char *treatment = "n/a";
	char *label = biosample;
	char labelBuf[256];
	char *vars = sqlQuickQuery(conn, query, varBuf, sizeof(varBuf));
	if (!isEmpty(vars))
	     {
	     treatment = vars + strlen("treatment=");
	     if (sameString(treatment, "4OHTAM_20nM_72hr"))
	         safef(labelBuf, sizeof(labelBuf), "%s 40HTAM", biosample);
	     else if (sameString(treatment, "diffProtA_14d"))
	         safef(labelBuf, sizeof(labelBuf), "%s diff 14d", biosample);
	     else if (sameString(treatment, "diffProtA_5d"))
		safef(labelBuf, sizeof(labelBuf), "%s diff 5d", biosample);
	     else if (sameString(treatment, "DIFF_4d"))
		safef(labelBuf, sizeof(labelBuf), "%s diff 4d", biosample);
	     else if (sameString(treatment, "Estradiol_100nM_1hr"))
	        safef(labelBuf, sizeof(labelBuf), "%s estradi 1h", biosample);
	     else if (sameString(treatment, "Estradiol_ctrl_0hr"))
	        safef(labelBuf, sizeof(labelBuf), "%s estradi 0h", biosample);
	     else
	        errAbort("Unknown treatment %s", treatment);
	     label = labelBuf;
	     }
	fprintf(f, "%s\t%s\t%s\t%s\t%s\t%s\n", inRow[0], inRow[1], inRow[2], inRow[3], treatment, label);
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
dnaseHg38AddTreatments(argv[1], argv[2]);
return 0;
}
