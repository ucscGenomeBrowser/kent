/* aliWith - show alignment with another protein. */

#include "common.h"
#include "obscure.h"
#include "dnaseq.h"
#include "fa.h"
#include "axt.h"
#include "hgGene.h"


static boolean aliWithExists(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Can we display this section? */
{
return aliWith != NULL;
}

static bioSeq *getSeq(struct sqlConnection *conn, char *geneId, 
	char *geneName, char *tableId)
/* Get sequence from table. */
{
char *table = genomeSetting(tableId);
char query[512];
struct sqlResult *sr;
char **row;
bioSeq *seq = NULL;

safef(query, sizeof(query), 
    "select seq from %s where name = '%s'", table, geneId);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(seq);
    seq->name = cloneString(geneName);
    seq->dna = cloneString(row[0]);
    seq->size = strlen(seq->dna);
    }
sqlFreeResult(&sr);
return seq;
}

static void printAxtAli(struct axt *axt, int maxLine, struct axtScoreScheme *ss, FILE *f)
/* Print out an alignment. */
{
int lineStart;
int qPos = axt->qStart;
int tPos = axt->tStart;
int symPos;
int aDigits = digitsBaseTen(axt->qEnd);
int bDigits = digitsBaseTen(axt->tEnd);
int digits = max(aDigits, bDigits);

for (symPos = 0; symPos < axt->symCount; symPos += maxLine)
    {
    /* Figure out which part of axt to use for this line. */
    int lineSize = axt->symCount - symPos;
    int lineEnd, i;
    if (lineSize > maxLine)
        lineSize = maxLine;
    lineEnd = symPos + lineSize;

    /* Draw query line including numbers. */
    fprintf(f, "%0*d ", digits, qPos+1);
    for (i=symPos; i<lineEnd; ++i)
        {
	char c = axt->qSym[i];
	fputc(c, f);
	if (c != '.' && c != '-')
	    ++qPos;
	}
    fprintf(f, " %0*d\n", digits, qPos);

    /* Draw line with match/mismatch symbols. */
    spaceOut(f, digits+1);
    for (i=symPos; i<lineEnd; ++i)
        {
	char q = axt->qSym[i];
	char t = axt->tSym[i];
	char out = ' ';
	if (q == t)
	    out = '|';
	else if (ss != NULL && ss->matrix[q][t] > 0)
	    out = '+';
	fputc(out, f);
	}
    fputc('\n', f);

    /* Draw target line including numbers. */
    fprintf(f, "%0*d ", digits, tPos+1);
    for (i=symPos; i<lineEnd; ++i)
        {
	char c = axt->tSym[i];
	fputc(c, f);
	if (c != '.' && c != '-')
	    ++tPos;
	}
    fprintf(f, " %0*d\n", digits, tPos);

    /* Draw extra empty line. */
    fputc('\n', f);
    }
}

static void aliWithPrint(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Print out altSplicing info. */
{
char *otherName = getGeneName(aliWith, conn);
bioSeq *a = getSeq(conn, geneId, curGeneName, "knownGenePep");
bioSeq *b = getSeq(conn, aliWith, otherName, "knownGenePep");
struct axtScoreScheme *ss = axtScoreSchemeProteinDefault();

printf("<TT><PRE>");
if (axtAffineSmallEnough(a->size, b->size))
    {
    struct axt *axt = axtAffine(a, b, ss);
    if (axt != NULL)
	{
	printf("Alignment between %s (top %daa) and %s (bottom %daa) score %d\n\n",
		a->name, a->size, b->name, b->size, axt->score);
	printAxtAli(axt, 60, ss, stdout);
	axtFree(&axt);
	}
    else
        {
	printf("%s and %s don't align\n", a->name, b->name);
	}
    }
else
    {
    printf("Sorry, %s (%d amino acids) and %s (%d amino acids) are too big to align",
    	a->name, a->size, b->name, b->size);
    }
printf("</PRE></TT>");
}

struct section *aliWithSection(struct sqlConnection *conn,
	struct hash *sectionRa)
/* Create alignment with other protein section  section. */
{
struct section *section = sectionNew(sectionRa, "aliWith");
section->exists = aliWithExists;
section->print = aliWithPrint;
return section;
}

