/* gffgenes - Creates files that say extents of genes from gff files. */

#include "common.h"
#include "sig.h"
#include "wormdna.h"

static char *inNames[] = {
    "i.gff",
    "ii.gff",
    "iii.gff",
    "iv.gff",
    "v.gff",
    "x.gff",
};

static char *chromNames[] = {
    "i", "ii", "iii", "iv", "v", "x",
};

struct exon
    {
    struct exon *next;
    int start, end;
    };

struct gene
    {
    struct gene *next;
    char *name;
    struct exon *exons;
    int start, end;
    UBYTE chromIx;
    char strand;
    };

int cmpGenes(const void *va, const void *vb)
{
const struct gene *a = *((struct gene **)va);
const struct gene *b = *((struct gene **)vb);
int dif = a->start - b->start;
if (a == 0)
    dif = a->end - b->end;
return dif;
}

void writeShortString(FILE *f, char *s)
{
UBYTE count = strlen(s);
writeOne(f, count);
mustWrite(f, s, count);
}

void checkGene(struct gene *gene)
{
int min = 0x7fffffff;
int max = -min;
struct exon *e;
for (e=gene->exons; e != NULL; e = e->next)
    {
    if (e->start < min)
        min = e->start;
    if (e->end > max)
        max = e->end;
    }
if (gene->exons == NULL)
    {
    printf("Gene %s has no exons\n", gene->name);
    }
else if (min != gene->start || max != gene->end)
    {
    printf("Gene %s - goes from %d-%d but exons go from %d-%d\n", 
        gene->name, gene->start, gene->end, min, max);
    gene->start = min;
    gene->end = max;
    }
}

void writeGene(struct gene *gene, FILE *c2g, FILE *gl)
{
short pointCount;
struct exon *exon;

checkGene(gene);
fprintf(c2g, "%s:%d-%d %c %s\n", chromNames[gene->chromIx], gene->start-1, gene->end, gene->strand, gene->name);
writeShortString(gl, gene->name);
writeOne(gl, gene->chromIx);
writeOne(gl, gene->strand);
pointCount = slCount(gene->exons) * 2;
writeOne(gl, pointCount);
for (exon = gene->exons; exon != NULL; exon = exon->next)
    {
    int start = exon->start - 1;
    int end = exon->end;
    writeOne(gl, start);
    writeOne(gl, exon->end);
    }
}

char *unquote(char *s)
/* Remove opening and closing quotes from string s. */
{
int len =strlen(s);
if (s[0] != '"')
    errAbort("Expecting begin quote on %s\n", s);
if (s[len-1] != '"')
    errAbort("Expecting end quote on %s\n", s);
s[len-1] = 0;
return s+1;
}

void procOne(char *inName, UBYTE chromIx, FILE *c2g, FILE *gl)
{
FILE *in = mustOpen(inName, "r");
struct gene *geneList = NULL, *g = NULL;
struct exon *exon;
char line[1024];
int lineCount = 0;
char *words[256];
int wordCount;
char *type;
char *geneName;

printf("Processing %s\n", inName);
while (fgets(line, sizeof(line), in))
    {
    ++lineCount;
    wordCount = chopLine(line, words);
    if (wordCount > 0)
        {
        if (wordCount < 10)
            errAbort("Short line %d of %s\n", lineCount, inName);
        type = words[2];
        geneName = unquote(words[9]);
        if (sameString(type, "sequence"))
            {
            if (g != NULL)
                {
                slAddHead(&geneList, g);
                }
            AllocVar(g);
            g->name = cloneString(geneName);
            g->start = atoi(words[3]);
            g->end = atoi(words[4]);
            g->strand = words[6][0];
            g->chromIx = chromIx;
            g->exons = NULL;
            }
        else if (sameString(type, "exon"))
            {
            if (!sameString(g->name, geneName) )
                {
                errAbort("exon of %s follows sequence of %s line %d of %s",
                    words[8], g->name, lineCount, inName);
                }
            AllocVar(exon);
            exon->start = atoi(words[3]);
            exon->end = atoi(words[4]);
            slAddTail(&g->exons, exon);
            }
        else if (sameString(type, "intron"))
            {
            }
        else
            {
            errAbort("Unexpected type %s line %d of %s", type, lineCount, inName);
            }
        }
    }
if (g != NULL)
    slAddHead(&geneList, g);
slReverse(&geneList);
slSort(&geneList, cmpGenes);
for (g = geneList; g != NULL; g=g->next)
    writeGene(g, c2g, gl);
fclose(in);
}

int main(int argc, char *argv[])
{
char *c2gName, *glName;
FILE *c2g, *gl;
int i;
bits32 sig = glSig;

if (argc != 3)
    {
    errAbort("gffgenes - creates files that store extents of genes for intronerator\n"
             "usage:\n"
             "     gffgenes c2g file.gl\n"
             "This needs to be run in the directory with the Xgenes.gff files.");
    }
c2gName = argv[1];
glName = argv[2];
c2g = mustOpen(c2gName, "w");
gl = mustOpen(glName, "wb");
writeOne(gl, sig);
for (i=0; i<ArraySize(inNames); ++i)
    {
    procOne(inNames[i], (UBYTE)i, c2g, gl);
    }
return 0;
}