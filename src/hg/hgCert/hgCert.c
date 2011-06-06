/* hgCert - hgCert parses cerfification data file and generates an html file and a .tab file */
#include "common.h"
#include "hCommon.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgCert - hgCert parses cerfification data file and generates an html file and a .tab file\n"
  "usage: hgCert certificate.txt >certificate.html\n");
}

char *doLine(char *lineType, char *lineIn)
    {
    char *result;
    char line[1000];

    strcpy(line, lineIn);
    *(line+strlen(line) - 1) = '\0';

    result = strstr(line, lineType);
    if (result == NULL)
	{
	return(result);
	}
    else
	{
	result = result + strlen(lineType);
	return(result);
	}
    }

char *doLine2(char *lineIn, char *lineType)
    {
    char *result;
    char *chp;
    char line[1000];

    strcpy(line, lineIn);

    *(line+strlen(line) - 1) = '\0';
    chp  = strstr(line, ":");

    if (chp == NULL) return(strdup("???"));

    *chp = '\0';
    chp ++;
    chp ++;

    chp = strdup(chp);
    strcpy(lineType, line);
    if (strcmp(chp, " ") == 0) chp = strdup("&nbsp");
    return(chp);
    }

int main(int argc, char *argv[])
{
char **p;
char *arg0, *infileName;

FILE *inf, *outf;
char line[1000], line2[1000];

char *id;
char *chp;

char *content;
char *contact="";

char *join;

char *join1, *join2;
char *spanner,
 *variation,
 *variationEvidence,
 *comment,
 *evaluation,
 *remark;
char lineType[100];

//int i,j;

if (argc != 2)
    {
    usage();
    }
else
    {
    infileName = argv[1];

    fprintf(stdout, "<HTML><HEAD><TITLE>Non-standard Join Certificates</TITLE>\n");
    fprintf(stdout, "<META http-equiv=Content-Type content=\"text/html; charset=windows-1252\">\n");
    fprintf(stdout, "<META content=\"MSHTML 6.00.2800.1106\" name=GENERATOR></HEAD>\n");
    fprintf(stdout, "<BODY><span style='font-family:Arial;'>\n");

    fprintf(stdout, "<TABLE cellspacing=3 cols=9 border cellPadding=2\n");

    fprintf(stdout, "  <TR BGCOLOR='#FFFEE8'><TH>Accession 1</TH>\n");
    fprintf(stdout, "  <TH>Accession 2</TH>\n");
    fprintf(stdout, "  <TH>Spanner</TH>\n");
    fprintf(stdout, "  <TH>Variation</TH>\n");
    fprintf(stdout, "  <TH>Variation Evidence</TH>\n");
    fprintf(stdout, "  <TH>Comment</TH>\n");
    fprintf(stdout, "  <TH>Evaluation</TH>\n");
    fprintf(stdout, "  <TH>Remark</TH>\n");
    fprintf(stdout, "  <TH>Contact</TH>\n");
    fprintf(stdout, "  </TR>\n");

    outf = fopen("cert.tab", "w");

    if ((inf = fopen(infileName, "r")) == NULL)
	{
	fprintf(stderr, "Can't open file %s.\n", infileName);
	exit(8);
	}

    while (fgets(line, 1000, inf) != NULL)
	{
	content = doLine("CONTACT: ", line);

	if (content != NULL)
	    {
	    contact = strdup(content);
	    fgets(line, 1000, inf);
	    fgets(line, 1000, inf);
	    }
	join = doLine("JOIN: ", line);
	if (join != NULL)
	    {
	    join1 = join;
	    join2 = strstr(join, " ");
	    *join2 = '\0';
	    join2++;
	    join1 = strdup(join1);
	    join2 = strdup(join2);

	    fgets(line, 1000, inf);
	    }
	else
	    {
	    fprintf(stderr, "No JOIN!\n");
	    exit(1);
	    }

	spanner=variation=variationEvidence=comment=evaluation = remark= strdup("&nbsp ");

	again:
	content = doLine2(line, lineType);
	if (strcmp(lineType, "VARIATION") == 0)
	    {
	    variation = content;
	    }

	content = doLine2(line, lineType);
	if (strcmp(lineType, "SPANNER") == 0)
	    {
	    spanner = content;
	    }

	content = doLine2(line, lineType);
	if (strcmp(lineType, "VARIATION EVIDENCE") == 0)
	    {
	    variationEvidence = content;
	    }
	content = doLine2(line, lineType);
	if (strcmp(lineType, "REMARK") == 0)
	    {
	    remark = content;
	    }
	content = doLine2(line, lineType);
	if (strcmp(lineType, "COMMENT") == 0)
	    {
	    comment = content;
	    }

	content = doLine2(line, lineType);
	if (strcmp(lineType, "EVALUATION") == 0)
	    {
	    evaluation = content;
	    }

	fgets(line, 1000, inf);
	if (strlen(line) > 3)
	    {
	    goto again;
	    }
	else
	    {
	    printf("<TR>");
	    printf("<TD>%s</TD>", join1);
	    printf("<TD>%s</TD>", join2);

	    printf("<TD>%s</TD>", spanner);
	    printf("<TD>%s</TD>", variation);
	    printf("<TD>%s</TD>", variationEvidence);
	    printf("<TD>%s</TD>", comment);

	    printf("<TD>%s</TD>", evaluation);
	    printf("<TD>%s</TD>", remark);

	    printf("<TD><A HREF=\"mailto:%s\">%s</A></TD>", contact, contact);
	    printf("</TR>\n");

	    fprintf(outf, "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n",
		    join1, join2, spanner, evaluation, variation, variationEvidence,
		    contact, remark, comment);
	    }
	}
    }
/*else
    {
    fprintf(stderr, "Usage: hgCert certificate.txt >certificate.html\n\n");
    return(1);
    }
*/
fprintf(stdout, "</table</span>\n");
fprintf(stdout, "</body></html>\n");
fclose(outf);
return(0);
}




