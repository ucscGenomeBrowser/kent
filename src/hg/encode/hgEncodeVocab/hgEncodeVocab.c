/* hgEncodeVocab - print table of controlled vocabulary from ENCODE configuration files */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "cheapcgi.h"
#include "hCommon.h"
#include "htmshell.h"
#include "ra.h"


static char const rcsid[] = "$Id: hgEncodeVocab.c,v 1.2 2008/07/23 18:51:12 kate Exp $";

/* Replace these with CGI variables */
#define CV_FILE "/cluster/data/encode/pipeline/encpipeline_beta/config/cv.ra"
#define ANTIBODY_TYPE "Antibody"

int termCmp(const void *va, const void *vb)
/* Compare controlled vocab based on term value */
{
const struct hash *a = *((struct hash **)va);
const struct hash *b = *((struct hash **)vb);
char *termA = hashMustFindVal((struct hash *)a, "term");
char *termB = hashMustFindVal((struct hash *)b, "term");
return (strcmp(termA, termB));
}

void doMiddle()
{
struct hash *cvHash = raReadAll(CV_FILE, "term");
struct hashCookie hc = hashFirst(cvHash);
struct hashEl *hEl;
struct slList *termList = NULL;
struct hash *ra;
int total = 0;
puts("<TABLE BORDER=1 BGCOLOR=#FFFEE8 CELLSPACING=0 CELLPADDING=2>\n");
puts("<TR style=\"background:#D9E4F8\"><TH>Term</TH><TH>Target Description</TH><TH>Antibody Description</TH><TH>Vendor ID</TH></TR>\n");

while ((hEl = hashNext(&hc)) != NULL)
    {
    ra = (struct hash *)hEl->val;
    if (differentString(hashMustFindVal(ra, "type"), ANTIBODY_TYPE))
        continue;
    slAddTail(&termList, ra);
    }
slSort(&termList, termCmp);
while((ra = slPopHead(&termList)) != NULL)
    {
    // TODO: Add check for unknown tags in cv.ra
    total++;
    puts("<TR>");
    char *s;
    s = hashMustFindVal(ra, "term");
    printf("<TD>%s</TD>\n", s);
    s = hashFindVal(ra, "targetDescription");
    printf("<TD>%s</TD>\n", s ? s : "&nbsp;");
    s = hashFindVal(ra, "antibodyDescription");
    printf("<TD>%s</TD>\n", s ? s : "&nbsp;");
    s = hashFindVal(ra, "vendorName");
    char *t = hashFindVal(ra, "vendorId");
    char *u = hashFindVal(ra, "orderUrl");
    puts("<TD>");
    if (u)
        printf("<A STYLE=\"text-decoration:none\" TARGET=_BLANK HREF=%s>", u);
    printf("%s %s", s ? s : "&nbsp;", t ? t : "&nbsp;");
    if (u)
        puts("<A>");
    puts("</TD></TR>");
    }
puts("</TABLE><BR>\n");
printf("Total = %d\n", total);
}

int main(int argc, char *argv[])
/* Process command line */
{
cgiSpoof(&argc, argv);
htmShell("ENCODE Controlled Vocabulary", doMiddle, "get");
return 0;
}

