/* riken.c autoXml generated file to load RIKEN annotations. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "xap.h"
#include "riken.h"


void *rikenStartHandler(struct xap *xp, char *name, char **atts);
/* Called by expat with start tag.  Does most of the parsing work. */

void rikenEndHandler(struct xap *xp, char *name);
/* Called by expat with end tag.  Checks all required children are loaded. */


void rikenMaxmlClustersSave(struct rikenMaxmlClusters *obj, int indent, FILE *f)
/* Save rikenMaxmlClusters to file. */
{
struct rikenCluster *rikenCluster;
boolean isNode = TRUE;
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<maxml-clusters>");
for (rikenCluster = obj->rikenCluster; rikenCluster != NULL; rikenCluster = rikenCluster->next)
   {
   if (isNode)
       {
       fprintf(f, "\n");
       isNode = FALSE;
       }
   rikenClusterSave(rikenCluster, indent+2, f);
   }
if (!isNode)
    xapIndent(indent, f);
fprintf(f, "</maxml-clusters>\n");
}

struct rikenMaxmlClusters *rikenMaxmlClustersLoad(char *fileName)
/* Load rikenMaxmlClusters from file. */
{
struct rikenMaxmlClusters *obj;
xapParseAny(fileName, "maxml-clusters", rikenStartHandler, rikenEndHandler, NULL, &obj);
return obj;
}

void rikenClusterSave(struct rikenCluster *obj, int indent, FILE *f)
/* Save rikenCluster to file. */
{
struct rikenSequence *rikenSequence;
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<cluster");
fprintf(f, " id=\"%s\"", obj->id);
fprintf(f, ">");
fprintf(f, "\n");
rikenFantomidSave(obj->rikenFantomid, indent+2, f);
rikenRepresentativeSeqidSave(obj->rikenRepresentativeSeqid, indent+2, f);
for (rikenSequence = obj->rikenSequence; rikenSequence != NULL; rikenSequence = rikenSequence->next)
   {
   rikenSequenceSave(rikenSequence, indent+2, f);
   }
xapIndent(indent, f);
fprintf(f, "</cluster>\n");
}

struct rikenCluster *rikenClusterLoad(char *fileName)
/* Load rikenCluster from file. */
{
struct rikenCluster *obj;
xapParseAny(fileName, "cluster", rikenStartHandler, rikenEndHandler, NULL, &obj);
return obj;
}

void rikenRepresentativeSeqidSave(struct rikenRepresentativeSeqid *obj, int indent, FILE *f)
/* Save rikenRepresentativeSeqid to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<representative-seqid>");
fprintf(f, "%s", obj->text);
fprintf(f, "</representative-seqid>\n");
}

struct rikenRepresentativeSeqid *rikenRepresentativeSeqidLoad(char *fileName)
/* Load rikenRepresentativeSeqid from file. */
{
struct rikenRepresentativeSeqid *obj;
xapParseAny(fileName, "representative-seqid", rikenStartHandler, rikenEndHandler, NULL, &obj);
return obj;
}

void rikenMaxmlSequencesSave(struct rikenMaxmlSequences *obj, int indent, FILE *f)
/* Save rikenMaxmlSequences to file. */
{
struct rikenSequence *rikenSequence;
boolean isNode = TRUE;
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<maxml-sequences>");
for (rikenSequence = obj->rikenSequence; rikenSequence != NULL; rikenSequence = rikenSequence->next)
   {
   if (isNode)
       {
       fprintf(f, "\n");
       isNode = FALSE;
       }
   rikenSequenceSave(rikenSequence, indent+2, f);
   }
if (!isNode)
    xapIndent(indent, f);
fprintf(f, "</maxml-sequences>\n");
}

struct rikenMaxmlSequences *rikenMaxmlSequencesLoad(char *fileName)
/* Load rikenMaxmlSequences from file. */
{
struct rikenMaxmlSequences *obj;
xapParseAny(fileName, "maxml-sequences", rikenStartHandler, rikenEndHandler, NULL, &obj);
return obj;
}

void rikenSequenceSave(struct rikenSequence *obj, int indent, FILE *f)
/* Save rikenSequence to file. */
{
struct rikenAltid *rikenAltid;
boolean isNode = TRUE;
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<sequence");
fprintf(f, " id=\"%s\"", obj->id);
fprintf(f, ">");
for (rikenAltid = obj->rikenAltid; rikenAltid != NULL; rikenAltid = rikenAltid->next)
   {
   if (isNode)
       {
       fprintf(f, "\n");
       isNode = FALSE;
       }
   rikenAltidSave(rikenAltid, indent+2, f);
   }
if (obj->rikenSeqid != NULL)
    {
    if (isNode)
       {
       fprintf(f, "\n");
       isNode = FALSE;
       }
    rikenSeqidSave(obj->rikenSeqid, indent+2, f);
    }
if (obj->rikenFantomid != NULL)
    {
    if (isNode)
       {
       fprintf(f, "\n");
       isNode = FALSE;
       }
    rikenFantomidSave(obj->rikenFantomid, indent+2, f);
    }
if (obj->rikenCloneid != NULL)
    {
    if (isNode)
       {
       fprintf(f, "\n");
       isNode = FALSE;
       }
    rikenCloneidSave(obj->rikenCloneid, indent+2, f);
    }
if (obj->rikenRearrayid != NULL)
    {
    if (isNode)
       {
       fprintf(f, "\n");
       isNode = FALSE;
       }
    rikenRearrayidSave(obj->rikenRearrayid, indent+2, f);
    }
if (obj->rikenAccession != NULL)
    {
    if (isNode)
       {
       fprintf(f, "\n");
       isNode = FALSE;
       }
    rikenAccessionSave(obj->rikenAccession, indent+2, f);
    }
if (obj->rikenAnnotator != NULL)
    {
    if (isNode)
       {
       fprintf(f, "\n");
       isNode = FALSE;
       }
    rikenAnnotatorSave(obj->rikenAnnotator, indent+2, f);
    }
if (obj->rikenVersion != NULL)
    {
    if (isNode)
       {
       fprintf(f, "\n");
       isNode = FALSE;
       }
    rikenVersionSave(obj->rikenVersion, indent+2, f);
    }
if (obj->rikenModifiedTime != NULL)
    {
    if (isNode)
       {
       fprintf(f, "\n");
       isNode = FALSE;
       }
    rikenModifiedTimeSave(obj->rikenModifiedTime, indent+2, f);
    }
if (obj->rikenAnnotations != NULL)
    {
    if (isNode)
       {
       fprintf(f, "\n");
       isNode = FALSE;
       }
    rikenAnnotationsSave(obj->rikenAnnotations, indent+2, f);
    }
if (obj->rikenComment != NULL)
    {
    if (isNode)
       {
       fprintf(f, "\n");
       isNode = FALSE;
       }
    rikenCommentSave(obj->rikenComment, indent+2, f);
    }
if (!isNode)
    xapIndent(indent, f);
fprintf(f, "</sequence>\n");
}

struct rikenSequence *rikenSequenceLoad(char *fileName)
/* Load rikenSequence from file. */
{
struct rikenSequence *obj;
xapParseAny(fileName, "sequence", rikenStartHandler, rikenEndHandler, NULL, &obj);
return obj;
}

void rikenAltidSave(struct rikenAltid *obj, int indent, FILE *f)
/* Save rikenAltid to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<altid");
fprintf(f, " type=\"%s\"", obj->type);
fprintf(f, ">");
fprintf(f, "%s", obj->text);
fprintf(f, "</altid>\n");
}

struct rikenAltid *rikenAltidLoad(char *fileName)
/* Load rikenAltid from file. */
{
struct rikenAltid *obj;
xapParseAny(fileName, "altid", rikenStartHandler, rikenEndHandler, NULL, &obj);
return obj;
}

void rikenSeqidSave(struct rikenSeqid *obj, int indent, FILE *f)
/* Save rikenSeqid to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<seqid>");
fprintf(f, "%s", obj->text);
fprintf(f, "</seqid>\n");
}

struct rikenSeqid *rikenSeqidLoad(char *fileName)
/* Load rikenSeqid from file. */
{
struct rikenSeqid *obj;
xapParseAny(fileName, "seqid", rikenStartHandler, rikenEndHandler, NULL, &obj);
return obj;
}

void rikenCloneidSave(struct rikenCloneid *obj, int indent, FILE *f)
/* Save rikenCloneid to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<cloneid>");
fprintf(f, "%s", obj->text);
fprintf(f, "</cloneid>\n");
}

struct rikenCloneid *rikenCloneidLoad(char *fileName)
/* Load rikenCloneid from file. */
{
struct rikenCloneid *obj;
xapParseAny(fileName, "cloneid", rikenStartHandler, rikenEndHandler, NULL, &obj);
return obj;
}

void rikenFantomidSave(struct rikenFantomid *obj, int indent, FILE *f)
/* Save rikenFantomid to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<fantomid>");
fprintf(f, "%s", obj->text);
fprintf(f, "</fantomid>\n");
}

struct rikenFantomid *rikenFantomidLoad(char *fileName)
/* Load rikenFantomid from file. */
{
struct rikenFantomid *obj;
xapParseAny(fileName, "fantomid", rikenStartHandler, rikenEndHandler, NULL, &obj);
return obj;
}

void rikenRearrayidSave(struct rikenRearrayid *obj, int indent, FILE *f)
/* Save rikenRearrayid to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<rearrayid>");
fprintf(f, "%s", obj->text);
fprintf(f, "</rearrayid>\n");
}

struct rikenRearrayid *rikenRearrayidLoad(char *fileName)
/* Load rikenRearrayid from file. */
{
struct rikenRearrayid *obj;
xapParseAny(fileName, "rearrayid", rikenStartHandler, rikenEndHandler, NULL, &obj);
return obj;
}

void rikenAccessionSave(struct rikenAccession *obj, int indent, FILE *f)
/* Save rikenAccession to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<accession>");
fprintf(f, "%s", obj->text);
fprintf(f, "</accession>\n");
}

struct rikenAccession *rikenAccessionLoad(char *fileName)
/* Load rikenAccession from file. */
{
struct rikenAccession *obj;
xapParseAny(fileName, "accession", rikenStartHandler, rikenEndHandler, NULL, &obj);
return obj;
}

void rikenAnnotatorSave(struct rikenAnnotator *obj, int indent, FILE *f)
/* Save rikenAnnotator to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<annotator>");
fprintf(f, "%s", obj->text);
fprintf(f, "</annotator>\n");
}

struct rikenAnnotator *rikenAnnotatorLoad(char *fileName)
/* Load rikenAnnotator from file. */
{
struct rikenAnnotator *obj;
xapParseAny(fileName, "annotator", rikenStartHandler, rikenEndHandler, NULL, &obj);
return obj;
}

void rikenVersionSave(struct rikenVersion *obj, int indent, FILE *f)
/* Save rikenVersion to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<version>");
fprintf(f, "%s", obj->text);
fprintf(f, "</version>\n");
}

struct rikenVersion *rikenVersionLoad(char *fileName)
/* Load rikenVersion from file. */
{
struct rikenVersion *obj;
xapParseAny(fileName, "version", rikenStartHandler, rikenEndHandler, NULL, &obj);
return obj;
}

void rikenModifiedTimeSave(struct rikenModifiedTime *obj, int indent, FILE *f)
/* Save rikenModifiedTime to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<modified_time>");
fprintf(f, "%s", obj->text);
fprintf(f, "</modified_time>\n");
}

struct rikenModifiedTime *rikenModifiedTimeLoad(char *fileName)
/* Load rikenModifiedTime from file. */
{
struct rikenModifiedTime *obj;
xapParseAny(fileName, "modified_time", rikenStartHandler, rikenEndHandler, NULL, &obj);
return obj;
}

void rikenAnnotationsSave(struct rikenAnnotations *obj, int indent, FILE *f)
/* Save rikenAnnotations to file. */
{
struct rikenAnnotation *rikenAnnotation;
boolean isNode = TRUE;
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<annotations>");
for (rikenAnnotation = obj->rikenAnnotation; rikenAnnotation != NULL; rikenAnnotation = rikenAnnotation->next)
   {
   if (isNode)
       {
       fprintf(f, "\n");
       isNode = FALSE;
       }
   rikenAnnotationSave(rikenAnnotation, indent+2, f);
   }
if (!isNode)
    xapIndent(indent, f);
fprintf(f, "</annotations>\n");
}

struct rikenAnnotations *rikenAnnotationsLoad(char *fileName)
/* Load rikenAnnotations from file. */
{
struct rikenAnnotations *obj;
xapParseAny(fileName, "annotations", rikenStartHandler, rikenEndHandler, NULL, &obj);
return obj;
}

void rikenCommentSave(struct rikenComment *obj, int indent, FILE *f)
/* Save rikenComment to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<comment>");
fprintf(f, "%s", obj->text);
fprintf(f, "</comment>\n");
}

struct rikenComment *rikenCommentLoad(char *fileName)
/* Load rikenComment from file. */
{
struct rikenComment *obj;
xapParseAny(fileName, "comment", rikenStartHandler, rikenEndHandler, NULL, &obj);
return obj;
}

void rikenAnnotationSave(struct rikenAnnotation *obj, int indent, FILE *f)
/* Save rikenAnnotation to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<annotation>");
fprintf(f, "\n");
rikenQualifierSave(obj->rikenQualifier, indent+2, f);
rikenAnntextSave(obj->rikenAnntext, indent+2, f);
rikenDatasrcSave(obj->rikenDatasrc, indent+2, f);
rikenSrckeySave(obj->rikenSrckey, indent+2, f);
rikenEvidenceSave(obj->rikenEvidence, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</annotation>\n");
}

struct rikenAnnotation *rikenAnnotationLoad(char *fileName)
/* Load rikenAnnotation from file. */
{
struct rikenAnnotation *obj;
xapParseAny(fileName, "annotation", rikenStartHandler, rikenEndHandler, NULL, &obj);
return obj;
}

void rikenQualifierSave(struct rikenQualifier *obj, int indent, FILE *f)
/* Save rikenQualifier to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<qualifier>");
fprintf(f, "%s", obj->text);
fprintf(f, "</qualifier>\n");
}

struct rikenQualifier *rikenQualifierLoad(char *fileName)
/* Load rikenQualifier from file. */
{
struct rikenQualifier *obj;
xapParseAny(fileName, "qualifier", rikenStartHandler, rikenEndHandler, NULL, &obj);
return obj;
}

void rikenAnntextSave(struct rikenAnntext *obj, int indent, FILE *f)
/* Save rikenAnntext to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<anntext>");
fprintf(f, "%s", obj->text);
fprintf(f, "</anntext>\n");
}

struct rikenAnntext *rikenAnntextLoad(char *fileName)
/* Load rikenAnntext from file. */
{
struct rikenAnntext *obj;
xapParseAny(fileName, "anntext", rikenStartHandler, rikenEndHandler, NULL, &obj);
return obj;
}

void rikenDatasrcSave(struct rikenDatasrc *obj, int indent, FILE *f)
/* Save rikenDatasrc to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<datasrc>");
fprintf(f, "%s", obj->text);
fprintf(f, "</datasrc>\n");
}

struct rikenDatasrc *rikenDatasrcLoad(char *fileName)
/* Load rikenDatasrc from file. */
{
struct rikenDatasrc *obj;
xapParseAny(fileName, "datasrc", rikenStartHandler, rikenEndHandler, NULL, &obj);
return obj;
}

void rikenSrckeySave(struct rikenSrckey *obj, int indent, FILE *f)
/* Save rikenSrckey to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<srckey>");
fprintf(f, "%s", obj->text);
fprintf(f, "</srckey>\n");
}

struct rikenSrckey *rikenSrckeyLoad(char *fileName)
/* Load rikenSrckey from file. */
{
struct rikenSrckey *obj;
xapParseAny(fileName, "srckey", rikenStartHandler, rikenEndHandler, NULL, &obj);
return obj;
}

void rikenEvidenceSave(struct rikenEvidence *obj, int indent, FILE *f)
/* Save rikenEvidence to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<evidence>");
fprintf(f, "%s", obj->text);
fprintf(f, "</evidence>\n");
}

struct rikenEvidence *rikenEvidenceLoad(char *fileName)
/* Load rikenEvidence from file. */
{
struct rikenEvidence *obj;
xapParseAny(fileName, "evidence", rikenStartHandler, rikenEndHandler, NULL, &obj);
return obj;
}

void *rikenStartHandler(struct xap *xp, char *name, char **atts)
/* Called by expat with start tag.  Does most of the parsing work. */
{
struct xapStack *st = xp->stack+1;
int depth = xp->stackDepth;
int i;

if (sameString(name, "maxml-clusters"))
    {
    struct rikenMaxmlClusters *obj;
    AllocVar(obj);
    return obj;
    }
else if (sameString(name, "cluster"))
    {
    struct rikenCluster *obj;
    AllocVar(obj);
    for (i=0; atts[i] != NULL; i += 2)
        {
        char *name = atts[i], *val = atts[i+1];
        if  (sameString(name, "id"))
            obj->id = cloneString(val);
        }
    if (obj->id == NULL)
        xapError(xp, "missing id");
    if (depth > 1)
        {
        if  (sameString(st->elName, "maxml-clusters"))
            {
            struct rikenMaxmlClusters *parent = st->object;
            slAddHead(&parent->rikenCluster, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "representative-seqid"))
    {
    struct rikenRepresentativeSeqid *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "cluster"))
            {
            struct rikenCluster *parent = st->object;
            slAddHead(&parent->rikenRepresentativeSeqid, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "maxml-sequences"))
    {
    struct rikenMaxmlSequences *obj;
    AllocVar(obj);
    return obj;
    }
else if (sameString(name, "sequence"))
    {
    struct rikenSequence *obj;
    AllocVar(obj);
    for (i=0; atts[i] != NULL; i += 2)
        {
        char *name = atts[i], *val = atts[i+1];
        if  (sameString(name, "id"))
            obj->id = cloneString(val);
        }
    if (obj->id == NULL)
        xapError(xp, "missing id");
    if (depth > 1)
        {
        if  (sameString(st->elName, "cluster"))
            {
            struct rikenCluster *parent = st->object;
            slAddHead(&parent->rikenSequence, obj);
            }
        else if (sameString(st->elName, "maxml-sequences"))
            {
            struct rikenMaxmlSequences *parent = st->object;
            slAddHead(&parent->rikenSequence, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "altid"))
    {
    struct rikenAltid *obj;
    AllocVar(obj);
    for (i=0; atts[i] != NULL; i += 2)
        {
        char *name = atts[i], *val = atts[i+1];
        if  (sameString(name, "type"))
            obj->type = cloneString(val);
        }
    if (obj->type == NULL)
        xapError(xp, "missing type");
    if (depth > 1)
        {
        if  (sameString(st->elName, "sequence"))
            {
            struct rikenSequence *parent = st->object;
            slAddHead(&parent->rikenAltid, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "seqid"))
    {
    struct rikenSeqid *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "sequence"))
            {
            struct rikenSequence *parent = st->object;
            slAddHead(&parent->rikenSeqid, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "cloneid"))
    {
    struct rikenCloneid *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "sequence"))
            {
            struct rikenSequence *parent = st->object;
            slAddHead(&parent->rikenCloneid, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "fantomid"))
    {
    struct rikenFantomid *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "cluster"))
            {
            struct rikenCluster *parent = st->object;
            slAddHead(&parent->rikenFantomid, obj);
            }
        else if (sameString(st->elName, "sequence"))
            {
            struct rikenSequence *parent = st->object;
            slAddHead(&parent->rikenFantomid, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "rearrayid"))
    {
    struct rikenRearrayid *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "sequence"))
            {
            struct rikenSequence *parent = st->object;
            slAddHead(&parent->rikenRearrayid, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "accession"))
    {
    struct rikenAccession *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "sequence"))
            {
            struct rikenSequence *parent = st->object;
            slAddHead(&parent->rikenAccession, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "annotator"))
    {
    struct rikenAnnotator *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "sequence"))
            {
            struct rikenSequence *parent = st->object;
            slAddHead(&parent->rikenAnnotator, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "version"))
    {
    struct rikenVersion *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "sequence"))
            {
            struct rikenSequence *parent = st->object;
            slAddHead(&parent->rikenVersion, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "modified_time"))
    {
    struct rikenModifiedTime *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "sequence"))
            {
            struct rikenSequence *parent = st->object;
            slAddHead(&parent->rikenModifiedTime, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "annotations"))
    {
    struct rikenAnnotations *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "sequence"))
            {
            struct rikenSequence *parent = st->object;
            slAddHead(&parent->rikenAnnotations, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "comment"))
    {
    struct rikenComment *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "sequence"))
            {
            struct rikenSequence *parent = st->object;
            slAddHead(&parent->rikenComment, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "annotation"))
    {
    struct rikenAnnotation *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "annotations"))
            {
            struct rikenAnnotations *parent = st->object;
            slAddHead(&parent->rikenAnnotation, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "qualifier"))
    {
    struct rikenQualifier *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "annotation"))
            {
            struct rikenAnnotation *parent = st->object;
            slAddHead(&parent->rikenQualifier, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "anntext"))
    {
    struct rikenAnntext *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "annotation"))
            {
            struct rikenAnnotation *parent = st->object;
            slAddHead(&parent->rikenAnntext, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "datasrc"))
    {
    struct rikenDatasrc *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "annotation"))
            {
            struct rikenAnnotation *parent = st->object;
            slAddHead(&parent->rikenDatasrc, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "srckey"))
    {
    struct rikenSrckey *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "annotation"))
            {
            struct rikenAnnotation *parent = st->object;
            slAddHead(&parent->rikenSrckey, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "evidence"))
    {
    struct rikenEvidence *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "annotation"))
            {
            struct rikenAnnotation *parent = st->object;
            slAddHead(&parent->rikenEvidence, obj);
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

void rikenEndHandler(struct xap *xp, char *name)
/* Called by expat with end tag.  Checks all required children are loaded. */
{
struct xapStack *stack = xp->stack;
if (sameString(name, "maxml-clusters"))
    {
    struct rikenMaxmlClusters *obj = stack->object;
    slReverse(&obj->rikenCluster);
    }
else if (sameString(name, "cluster"))
    {
    struct rikenCluster *obj = stack->object;
    if (obj->rikenFantomid == NULL)
        xapError(xp, "Missing fantomid");
    if (obj->rikenFantomid->next != NULL)
        xapError(xp, "Multiple fantomid");
    if (obj->rikenRepresentativeSeqid == NULL)
        xapError(xp, "Missing representative-seqid");
    if (obj->rikenRepresentativeSeqid->next != NULL)
        xapError(xp, "Multiple representative-seqid");
    slReverse(&obj->rikenSequence);
    }
else if (sameString(name, "representative-seqid"))
    {
    struct rikenRepresentativeSeqid *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "maxml-sequences"))
    {
    struct rikenMaxmlSequences *obj = stack->object;
    slReverse(&obj->rikenSequence);
    }
else if (sameString(name, "sequence"))
    {
    struct rikenSequence *obj = stack->object;
    slReverse(&obj->rikenAltid);
    if (obj->rikenSeqid != NULL && obj->rikenSeqid->next != NULL)
        xapError(xp, "Multiple seqid");
    if (obj->rikenFantomid != NULL && obj->rikenFantomid->next != NULL)
        xapError(xp, "Multiple fantomid");
    if (obj->rikenCloneid != NULL && obj->rikenCloneid->next != NULL)
        xapError(xp, "Multiple cloneid");
    if (obj->rikenRearrayid != NULL && obj->rikenRearrayid->next != NULL)
        xapError(xp, "Multiple rearrayid");
    if (obj->rikenAccession != NULL && obj->rikenAccession->next != NULL)
        xapError(xp, "Multiple accession");
    if (obj->rikenAnnotator != NULL && obj->rikenAnnotator->next != NULL)
        xapError(xp, "Multiple annotator");
    if (obj->rikenVersion != NULL && obj->rikenVersion->next != NULL)
        xapError(xp, "Multiple version");
    if (obj->rikenModifiedTime != NULL && obj->rikenModifiedTime->next != NULL)
        xapError(xp, "Multiple modified_time");
    if (obj->rikenAnnotations != NULL && obj->rikenAnnotations->next != NULL)
        xapError(xp, "Multiple annotations");
    if (obj->rikenComment != NULL && obj->rikenComment->next != NULL)
        xapError(xp, "Multiple comment");
    }
else if (sameString(name, "altid"))
    {
    struct rikenAltid *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "seqid"))
    {
    struct rikenSeqid *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "cloneid"))
    {
    struct rikenCloneid *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "fantomid"))
    {
    struct rikenFantomid *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "rearrayid"))
    {
    struct rikenRearrayid *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "accession"))
    {
    struct rikenAccession *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "annotator"))
    {
    struct rikenAnnotator *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "version"))
    {
    struct rikenVersion *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "modified_time"))
    {
    struct rikenModifiedTime *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "annotations"))
    {
    struct rikenAnnotations *obj = stack->object;
    slReverse(&obj->rikenAnnotation);
    }
else if (sameString(name, "comment"))
    {
    struct rikenComment *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "annotation"))
    {
    struct rikenAnnotation *obj = stack->object;
    if (obj->rikenQualifier == NULL)
        xapError(xp, "Missing qualifier");
    if (obj->rikenQualifier->next != NULL)
        xapError(xp, "Multiple qualifier");
    if (obj->rikenAnntext != NULL && obj->rikenAnntext->next != NULL)
        xapError(xp, "Multiple anntext");
    if (obj->rikenDatasrc != NULL && obj->rikenDatasrc->next != NULL)
        xapError(xp, "Multiple datasrc");
    if (obj->rikenSrckey != NULL && obj->rikenSrckey->next != NULL)
        xapError(xp, "Multiple srckey");
    if (obj->rikenEvidence != NULL && obj->rikenEvidence->next != NULL)
        xapError(xp, "Multiple evidence");
    }
else if (sameString(name, "qualifier"))
    {
    struct rikenQualifier *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "anntext"))
    {
    struct rikenAnntext *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "datasrc"))
    {
    struct rikenDatasrc *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "srckey"))
    {
    struct rikenSrckey *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "evidence"))
    {
    struct rikenEvidence *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
}

