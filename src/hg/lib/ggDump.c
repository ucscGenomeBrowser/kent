/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* ggDump - Dump out geneGraph structures. */
#include "common.h"
#include "ggMrnaAli.h"
#include "ggPrivate.h"


static char charForType(int type)
/* Return character corresponding to edge. */
{
char c = 0;
switch (type)
    {
    case ggSoftStart:
	c = '{';
	break;
    case ggSoftEnd:
	c = '}';
	break;
    case ggHardStart:
	c = '[';
	break;
    case ggHardEnd:
	c = ']';
	break;
    case ggUnused:
        c = 'x';
	break;
    default:
	errAbort("Unknown vertex type %d", type);
	break;
    }
return c;
}

void asciiArtDumpDa(struct ggAliInfo *da, int start, int end)
/* Display list of dense Alis in artistic ascii format. */
{
enum { screenWidth = 80};
static char line[screenWidth+1];
int ggiWidth = end - start;
double scaler = (double)(screenWidth-1)/ggiWidth;
struct ggVertex *v;
int pos;
int i;

memset(line, ' ', screenWidth);
v = da->vertices;
for (i=0; i<da->vertexCount; ++i,++v)
    {
    char c = charForType(v->type);
    pos = round((v->position-start) * scaler);
    assert(pos >= 0 && pos < screenWidth);
    line[pos] = c;
    }
printf("%s\n", line);
}

void dumpGgAliInfo(struct ggAliInfo *da, int start, int end)
/* Display list of dense Alis. */
{
struct ggVertex *v;
int i;

v = da->vertices;
for (i=0; i<da->vertexCount; ++i,++v)
    {
    char c = charForType(v->type);
    if (v->type == ggSoftStart || v->type == ggHardStart)
	printf("%c%d ", c, v->position);
    else
	printf("%d%c   ", v->position, c);
    }
printf("\n");
}

void dumpMc(struct ggMrnaCluster *mc)
/* Display mc on the screen. */
{
struct ggAliInfo *da;
struct maRef *ref;

printf("refs -");
for (ref = mc->refList; ref != NULL; ref = ref->next)
    printf(" %s", ref->ma->tName);
printf("\n");
for (da = mc->mrnaList; da != NULL; da = da->next)
    dumpGgAliInfo(da, mc->tStart, mc->tEnd);
}

void dumpGg(struct geneGraph *gg)
/* Print out a gene graph. */
{
int i,j;
int vertexCount = gg->vertexCount;

printf("geneGraph has %d vertices\n", vertexCount);
for (i=0; i<vertexCount; ++i)
    {
    struct ggVertex *v = &gg->vertices[i];
    bool *arcsOut = gg->edgeMatrix[i];
    printf("  %d %5d %c   ", i, v->position, charForType(v->type));
    for (j=0; j<vertexCount; ++j)
	{
	if (arcsOut[j])
	    printf(" %d", j);
	}
    printf("\n");
    }
}

void freeGgMrnaAli(struct ggMrnaAli **pMa)
/* Free up a single ggMrnaAli. */
{
struct ggMrnaAli *ma;
if ((ma = *pMa) != NULL)
    {
    freeMem(ma->tName);
    freeMem(ma->blocks);
    freez(pMa);
    }
}

void freeGgMrnaAliList(struct ggMrnaAli **pMaList)
/* Free up ggMrnaAli list */
{
struct ggMrnaAli *ma,*next;
for (ma = *pMaList; ma != NULL; ma = next)
    {
    next = ma->next;
    freeGgMrnaAli(&ma);
    }
*pMaList = NULL;
}

void freeGgMrnaInput(struct ggMrnaInput **pCi)
/* Free up a ggMrnaInput. */
{
struct ggMrnaInput *ci;
if ((ci = *pCi) != NULL)
    {
    ggMrnaAliFreeList(&ci->maList);
    freeDnaSeq(&ci->genoSeq);
    freez(pCi);
    }
}


