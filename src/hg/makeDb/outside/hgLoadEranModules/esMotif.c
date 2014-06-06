/* esm.c autoXml generated file */

/* Copyright (C) 2006 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "xap.h"
#include "esMotif.h"

void *esmStartHandler(struct xap *xp, char *name, char **atts);
/* Called by expat with start tag.  Does most of the parsing work. */

void esmEndHandler(struct xap *xp, char *name);
/* Called by expat with end tag.  Checks all required children are loaded. */


void esmMotifsSave(struct esmMotifs *obj, int indent, FILE *f)
/* Save esmMotifs to file. */
{
struct esmMotif *esmMotif;
boolean isNode = TRUE;
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Motifs");
fprintf(f, " SeqFile=\"%s\"", obj->SeqFile);
fprintf(f, ">");
for (esmMotif = obj->esmMotif; esmMotif != NULL; esmMotif = esmMotif->next)
   {
   if (isNode)
       {
       fprintf(f, "\n");
       isNode = FALSE;
       }
   esmMotifSave(esmMotif, indent+2, f);
   }
if (!isNode)
    xapIndent(indent, f);
fprintf(f, "</Motifs>\n");
}

struct esmMotifs *esmMotifsLoad(char *fileName)
/* Load esmMotifs from file. */
{
struct esmMotifs *obj;
xapParseAny(fileName, "Motifs", esmStartHandler, esmEndHandler, NULL, &obj);
return obj;
}

void esmMotifSave(struct esmMotif *obj, int indent, FILE *f)
/* Save esmMotif to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Motif");
fprintf(f, " Consensus=\"%s\"", obj->Consensus);
fprintf(f, " Source=\"%s\"", obj->Source);
fprintf(f, " Name=\"%s\"", obj->Name);
if (obj->Description != NULL)
    fprintf(f, " Description=\"%s\"", obj->Description);
fprintf(f, ">");
fprintf(f, "\n");
esmWeightsSave(obj->esmWeights, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</Motif>\n");
}

struct esmMotif *esmMotifLoad(char *fileName)
/* Load esmMotif from file. */
{
struct esmMotif *obj;
xapParseAny(fileName, "Motif", esmStartHandler, esmEndHandler, NULL, &obj);
return obj;
}

void esmWeightsSave(struct esmWeights *obj, int indent, FILE *f)
/* Save esmWeights to file. */
{
struct esmPosition *esmPosition;
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Weights");
fprintf(f, " ZeroWeight=\"%f\"", obj->ZeroWeight);
fprintf(f, ">");
fprintf(f, "\n");
for (esmPosition = obj->esmPosition; esmPosition != NULL; esmPosition = esmPosition->next)
   {
   esmPositionSave(esmPosition, indent+2, f);
   }
xapIndent(indent, f);
fprintf(f, "</Weights>\n");
}

struct esmWeights *esmWeightsLoad(char *fileName)
/* Load esmWeights from file. */
{
struct esmWeights *obj;
xapParseAny(fileName, "Weights", esmStartHandler, esmEndHandler, NULL, &obj);
return obj;
}

void esmPositionSave(struct esmPosition *obj, int indent, FILE *f)
/* Save esmPosition to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Position");
fprintf(f, " Num=\"%d\"", obj->Num);
fprintf(f, " Weights=\"%s\"", obj->Weights);
fprintf(f, "->\n");
}

struct esmPosition *esmPositionLoad(char *fileName)
/* Load esmPosition from file. */
{
struct esmPosition *obj;
xapParseAny(fileName, "Position", esmStartHandler, esmEndHandler, NULL, &obj);
return obj;
}

void *esmStartHandler(struct xap *xp, char *name, char **atts)
/* Called by expat with start tag.  Does most of the parsing work. */
{
struct xapStack *st = xp->stack+1;
int depth = xp->stackDepth;
int i;

if (sameString(name, "Motifs"))
    {
    struct esmMotifs *obj;
    AllocVar(obj);
    for (i=0; atts[i] != NULL; i += 2)
        {
        char *name = atts[i], *val = atts[i+1];
        if  (sameString(name, "SeqFile"))
            obj->SeqFile = cloneString(val);
        }
    if (obj->SeqFile == NULL)
        xapError(xp, "missing SeqFile");
    return obj;
    }
else if (sameString(name, "Motif"))
    {
    struct esmMotif *obj;
    AllocVar(obj);
    for (i=0; atts[i] != NULL; i += 2)
        {
        char *name = atts[i], *val = atts[i+1];
        if  (sameString(name, "Consensus"))
            obj->Consensus = cloneString(val);
        else if (sameString(name, "Source"))
            obj->Source = cloneString(val);
        else if (sameString(name, "Name"))
            obj->Name = cloneString(val);
        else if (sameString(name, "Description"))
            obj->Description = cloneString(val);
        }
    if (obj->Consensus == NULL)
        xapError(xp, "missing Consensus");
    if (obj->Source == NULL)
        xapError(xp, "missing Source");
    if (obj->Name == NULL)
        xapError(xp, "missing Name");
    if (depth > 1)
        {
        if  (sameString(st->elName, "Motifs"))
            {
            struct esmMotifs *parent = st->object;
            slAddHead(&parent->esmMotif, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Weights"))
    {
    struct esmWeights *obj;
    AllocVar(obj);
    for (i=0; atts[i] != NULL; i += 2)
        {
        char *name = atts[i], *val = atts[i+1];
        if  (sameString(name, "ZeroWeight"))
            obj->ZeroWeight = atof(val);
        }
    if (depth > 1)
        {
        if  (sameString(st->elName, "Motif"))
            {
            struct esmMotif *parent = st->object;
            slAddHead(&parent->esmWeights, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Position"))
    {
    struct esmPosition *obj;
    AllocVar(obj);
    for (i=0; atts[i] != NULL; i += 2)
        {
        char *name = atts[i], *val = atts[i+1];
        if  (sameString(name, "Num"))
            obj->Num = atoi(val);
        else if (sameString(name, "Weights"))
            obj->Weights = cloneString(val);
        }
    if (obj->Weights == NULL)
        xapError(xp, "missing Weights");
    if (depth > 1)
        {
        if  (sameString(st->elName, "Weights"))
            {
            struct esmWeights *parent = st->object;
            slAddHead(&parent->esmPosition, obj);
            }
        }
    return obj;
    }
else
    {
    xapSkip(xp);
    return NULL;
    }
}

void esmEndHandler(struct xap *xp, char *name)
/* Called by expat with end tag.  Checks all required children are loaded. */
{
struct xapStack *stack = xp->stack;
if (sameString(name, "Motifs"))
    {
    struct esmMotifs *obj = stack->object;
    slReverse(&obj->esmMotif);
    }
else if (sameString(name, "Motif"))
    {
    struct esmMotif *obj = stack->object;
    if (obj->esmWeights == NULL)
        xapError(xp, "Missing Weights");
    if (obj->esmWeights->next != NULL)
        xapError(xp, "Multiple Weights");
    }
else if (sameString(name, "Weights"))
    {
    struct esmWeights *obj = stack->object;
    if (obj->esmPosition == NULL)
        xapError(xp, "Missing Position");
    slReverse(&obj->esmPosition);
    }
}

