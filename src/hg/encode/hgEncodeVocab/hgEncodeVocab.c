/* hgEncodeVocab - print table of controlled vocabulary from ENCODE configuration files */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "cheapcgi.h"
#include "hCommon.h"
#include "htmshell.h"
#include "ra.h"
#include "hui.h"

/* hgEncodeVocab - A CGI script to display the different types of encode controlled vocabulary.
 * usage:
 *   hgEncodeVocab [ra=cv.ra] type=[Antibody|"Cell Line"|localization|rnaExtract|"Gene Type"] [tier=(1|2|3)]
 * options:\n"
 *    ra=cv.ra       : Path to cv.ra file (default cv_file())
 *    type=TypeName  : Type to display
 *    tier=N         : If type="Cell Line" then this is the tier to display
 */

static char const rcsid[] = "$Id: hgEncodeVocab.c,v 1.6 2008/09/04 18:51:26 larrym Exp $";

static char *cv_file()
{
/* return default location of cv.ra (can specify as cgi var: ra=cv.ra) */

static char filePath[PATH_LEN];
safef(filePath, sizeof(filePath), "%s/encode/cv.ra", hCgiRoot());
if(!fileExists(filePath))
    errAbort("Error: can't locate cv.ra; %s doesn't exist\n", filePath);
return filePath;
}

int termCmp(const void *va, const void *vb)
/* Compare controlled vocab based on term value */
{
const struct hash *a = *((struct hash **)va);
const struct hash *b = *((struct hash **)vb);
char *termA = hashMustFindVal((struct hash *)a, "term");
char *termB = hashMustFindVal((struct hash *)b, "term");
return (strcmp(termA, termB));
}

void doTypeHeader(char *type)
{
if (!strcmp(type,"Antibody"))
    {
    puts("  <TH>Term</TH><TH>Target Description</TH><TH>Antibody Description</TH><TH>Vendor ID</TH>");
    }
else if (!strcmp(type,"localization"))
    {
    puts("  <TH>Term</TH><TH>Description</TH><TH>GO ID</TH>");
    }
else if (!strcmp(type,"rnaExtract"))
    {
    puts("  <TH>Term</TH><TH>Description</TH>");
    }
else if (!strcmp(type,"Gene Type"))
    {
    puts("  <TH>Term</TH><TH>Description</TH>");
    }
else if (!strcmp(type,"Cell Line"))
    {
    puts("  <TH>Term</TH><TH>Description</TH><TH>Lineage</TH><TH>Karyotype</TH><TH>Order URL</TH><TH>Term ID</TH>");
    }
else 
    errAbort("Error: Unrecognised type (%s)\n", type);
}

void doTypeRow(struct hash *ra, char *type, int *total)
{
char *s, *u;

if (!strcmp(type,"Antibody"))
    {
    ++(*total);
    puts("<TR>");
    s = hashMustFindVal(ra, "term");
    printf("  <TD>%s</TD>\n", s);
    s = hashFindVal(ra, "targetDescription");
    printf("  <TD>%s</TD>\n", s ? s : "&nbsp;");
    s = hashFindVal(ra, "antibodyDescription");
    printf("  <TD>%s</TD>\n", s ? s : "&nbsp;");
    s = hashFindVal(ra, "vendorName");
    char *t = hashFindVal(ra, "vendorId");
    char *u = hashFindVal(ra, "orderUrl");
    printf("  <TD>");
    if (u)
        printf("<A STYLE=\"text-decoration:none\" TARGET=_BLANK HREF=%s>", u);
    printf("%s %s", s ? s : "&nbsp;", t ? t : "&nbsp;");
    if (u)
        printf("</A>");
    puts("</TD>");
    puts("</TR>");
    }
else if (strcmp(type,"localization")==0)
    {
    ++(*total);
    puts("<TR>");
    s = hashMustFindVal(ra, "term");
    printf("  <TD>%s</TD>\n", s);
    s = hashMustFindVal(ra, "description");
    printf("  <TD>%s</TD>\n", s);
    s = hashFindVal(ra, "termId");
    u = hashFindVal(ra, "termUrl");
    printf("  <TD>");
    if (u)
        printf("<A STYLE=\"text-decoration:none\" TARGET=_BLANK HREF=%s>", u);
    printf("%s", s ? s : "&nbsp;");
    if (u)
        printf("</A>");
    puts("</TD>");
    puts("</TR>");
    }
else if (strcmp(type,"rnaExtract")==0)
    {
    ++(*total);
    puts("<TR>");
    s = hashMustFindVal(ra, "term");
    printf("  <TD>%s</TD>\n", s);
    s = hashMustFindVal(ra, "description");
    printf("  <TD>%s</TD>\n", s);
    puts("</TR>");
    }
else if (strcmp(type,"Gene Type")==0)
    {
    ++(*total);
    puts("<TR>");
    s = hashMustFindVal(ra, "term");
    printf("  <TD>%s</TD>\n", s);
    s = hashMustFindVal(ra, "description");
    printf("  <TD>%s</TD>\n", s);
    puts("</TR>");
    }
else if (strcmp(type,"Cell Line")==0)
    {
    if (cgiOptionalInt("tier",0) && hashFindVal(ra,"tier") && atoi(hashFindVal(ra,"tier"))==cgiOptionalInt("tier",0))
    	{
    	++(*total);
	puts("<TR>");
	s = hashMustFindVal(ra, "term");
    	printf("  <TD>%s</TD>\n", s);
       	s = hashFindVal(ra, "description");
	printf("  <TD>%s</TD>\n", s ? s : "&nbsp;" );
	s = hashFindVal(ra, "lineage");
	printf("  <TD>%s</TD>\n", s ? s : "&nbsp;" );
       	s = hashFindVal(ra, "karyotype");
       	printf("  <TD>%s</TD>\n", s ? s : "&nbsp;" );
	u = hashFindVal(ra, "orderUrl");
	printf("  <TD>");
	if (u)
	    printf("<A STYLE=\"text-decoration:none\" TARGET=_BLANK HREF=%s>", u);
      	printf("%s", u ? u : "&nbsp;");
	if (u)
	    printf("</A>");
       	printf("</TD>\n");
	s = hashFindVal(ra, "termId");
	printf("  <TD>%s</TD>\n", s ? s : "&nbsp;" );
    	puts("</TR>");
	}
    }
else 
    errAbort("Error: Unrecognised type (%s)\n", type);
}


void doMiddle()
{
struct hash *cvHash = raReadAll(cgiUsualString("ra", cv_file()), "term");
struct hashCookie hc = hashFirst(cvHash);
struct hashEl *hEl;
struct slList *termList = NULL;
struct hash *ra;
char *type;
int total = 0;

puts("<TABLE BORDER=1 BGCOLOR=#FFFEE8 CELLSPACING=0 CELLPADDING=2>");
puts("<TR style=\"background:#D9E4F8\">");
type = cgiString("type");
doTypeHeader(type);
puts("</TR>");

while ((hEl = hashNext(&hc)) != NULL)
    {
    ra = (struct hash *)hEl->val;
    if (differentString(hashMustFindVal(ra, "type"), type))
        continue;
    slAddTail(&termList, ra);
    }
slSort(&termList, termCmp);
while((ra = slPopHead(&termList)) != NULL)
    {
    // TODO: Add check for unknown tags in cv.ra
    doTypeRow(ra, type, &total);
    }
puts("</TABLE><BR>");
printf("Total = %d\n", total);
}

int main(int argc, char *argv[])
/* Process command line */
{
cgiSpoof(&argc, argv);
htmShell("ENCODE Controlled Vocabulary", doMiddle, "get");
return 0;
}

