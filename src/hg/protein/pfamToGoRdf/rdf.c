/* rdf.c autoXml generated file */

/* Copyright (C) 2007 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "xap.h"
#include "rdf.h"


void rdfGoGoFree(struct rdfGoGo **pObj)
/* Free up rdfGoGo. */
{
struct rdfGoGo *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->xmlns_go);
freeMem(obj->xmlns_rdf);
rdfRdfRDFFree(&obj->rdfRdfRDF);
freez(pObj);
}

void rdfGoGoFreeList(struct rdfGoGo **pList)
/* Free up list of rdfGoGo. */
{
struct rdfGoGo *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    rdfGoGoFree(&el);
    el = next;
    }
}

void rdfGoGoSave(struct rdfGoGo *obj, int indent, FILE *f)
/* Save rdfGoGo to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<go:go");
fprintf(f, " xmlns:go=\"%s\"", obj->xmlns_go);
fprintf(f, " xmlns:rdf=\"%s\"", obj->xmlns_rdf);
fprintf(f, ">");
fprintf(f, "\n");
rdfRdfRDFSave(obj->rdfRdfRDF, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</go:go>\n");
}

struct rdfGoGo *rdfGoGoLoad(char *fileName)
/* Load rdfGoGo from XML file where it is root element. */
{
struct rdfGoGo *obj;
xapParseAny(fileName, "go:go", rdfStartHandler, rdfEndHandler, NULL, &obj);
return obj;
}

struct rdfGoGo *rdfGoGoLoadNext(struct xap *xap)
/* Load next rdfGoGo element.  Use xapOpen to get xap. */
{
return xapNext(xap, "go:go");
}

void rdfRdfRDFFree(struct rdfRdfRDF **pObj)
/* Free up rdfRdfRDF. */
{
struct rdfRdfRDF *obj = *pObj;
if (obj == NULL) return;
rdfGoTermFreeList(&obj->rdfGoTerm);
freez(pObj);
}

void rdfRdfRDFFreeList(struct rdfRdfRDF **pList)
/* Free up list of rdfRdfRDF. */
{
struct rdfRdfRDF *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    rdfRdfRDFFree(&el);
    el = next;
    }
}

void rdfRdfRDFSave(struct rdfRdfRDF *obj, int indent, FILE *f)
/* Save rdfRdfRDF to file. */
{
struct rdfGoTerm *rdfGoTerm;
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<rdf:RDF>");
fprintf(f, "\n");
for (rdfGoTerm = obj->rdfGoTerm; rdfGoTerm != NULL; rdfGoTerm = rdfGoTerm->next)
   {
   rdfGoTermSave(rdfGoTerm, indent+2, f);
   }
xapIndent(indent, f);
fprintf(f, "</rdf:RDF>\n");
}

struct rdfRdfRDF *rdfRdfRDFLoad(char *fileName)
/* Load rdfRdfRDF from XML file where it is root element. */
{
struct rdfRdfRDF *obj;
xapParseAny(fileName, "rdf:RDF", rdfStartHandler, rdfEndHandler, NULL, &obj);
return obj;
}

struct rdfRdfRDF *rdfRdfRDFLoadNext(struct xap *xap)
/* Load next rdfRdfRDF element.  Use xapOpen to get xap. */
{
return xapNext(xap, "rdf:RDF");
}

void rdfGoTermFree(struct rdfGoTerm **pObj)
/* Free up rdfGoTerm. */
{
struct rdfGoTerm *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->rdf_about);
rdfGoAccessionFree(&obj->rdfGoAccession);
rdfGoNameFree(&obj->rdfGoName);
rdfGoDbxrefFree(&obj->rdfGoDbxref);
freez(pObj);
}

void rdfGoTermFreeList(struct rdfGoTerm **pList)
/* Free up list of rdfGoTerm. */
{
struct rdfGoTerm *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    rdfGoTermFree(&el);
    el = next;
    }
}

void rdfGoTermSave(struct rdfGoTerm *obj, int indent, FILE *f)
/* Save rdfGoTerm to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<go:term");
fprintf(f, " rdf:about=\"%s\"", obj->rdf_about);
fprintf(f, ">");
fprintf(f, "\n");
rdfGoAccessionSave(obj->rdfGoAccession, indent+2, f);
rdfGoNameSave(obj->rdfGoName, indent+2, f);
rdfGoDbxrefSave(obj->rdfGoDbxref, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</go:term>\n");
}

struct rdfGoTerm *rdfGoTermLoad(char *fileName)
/* Load rdfGoTerm from XML file where it is root element. */
{
struct rdfGoTerm *obj;
xapParseAny(fileName, "go:term", rdfStartHandler, rdfEndHandler, NULL, &obj);
return obj;
}

struct rdfGoTerm *rdfGoTermLoadNext(struct xap *xap)
/* Load next rdfGoTerm element.  Use xapOpen to get xap. */
{
return xapNext(xap, "go:term");
}

void rdfGoAccessionFree(struct rdfGoAccession **pObj)
/* Free up rdfGoAccession. */
{
struct rdfGoAccession *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void rdfGoAccessionFreeList(struct rdfGoAccession **pList)
/* Free up list of rdfGoAccession. */
{
struct rdfGoAccession *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    rdfGoAccessionFree(&el);
    el = next;
    }
}

void rdfGoAccessionSave(struct rdfGoAccession *obj, int indent, FILE *f)
/* Save rdfGoAccession to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<go:accession>");
fprintf(f, "%s", obj->text);
fprintf(f, "</go:accession>\n");
}

struct rdfGoAccession *rdfGoAccessionLoad(char *fileName)
/* Load rdfGoAccession from XML file where it is root element. */
{
struct rdfGoAccession *obj;
xapParseAny(fileName, "go:accession", rdfStartHandler, rdfEndHandler, NULL, &obj);
return obj;
}

struct rdfGoAccession *rdfGoAccessionLoadNext(struct xap *xap)
/* Load next rdfGoAccession element.  Use xapOpen to get xap. */
{
return xapNext(xap, "go:accession");
}

void rdfGoNameFree(struct rdfGoName **pObj)
/* Free up rdfGoName. */
{
struct rdfGoName *obj = *pObj;
if (obj == NULL) return;
freez(pObj);
}

void rdfGoNameFreeList(struct rdfGoName **pList)
/* Free up list of rdfGoName. */
{
struct rdfGoName *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    rdfGoNameFree(&el);
    el = next;
    }
}

void rdfGoNameSave(struct rdfGoName *obj, int indent, FILE *f)
/* Save rdfGoName to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, " ->\n");
}

struct rdfGoName *rdfGoNameLoad(char *fileName)
/* Load rdfGoName from XML file where it is root element. */
{
struct rdfGoName *obj;
xapParseAny(fileName, "go:name", rdfStartHandler, rdfEndHandler, NULL, &obj);
return obj;
}

struct rdfGoName *rdfGoNameLoadNext(struct xap *xap)
/* Load next rdfGoName element.  Use xapOpen to get xap. */
{
return xapNext(xap, "go:name");
}

void rdfGoDbxrefFree(struct rdfGoDbxref **pObj)
/* Free up rdfGoDbxref. */
{
struct rdfGoDbxref *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->rdf_parseType);
rdfGoDatabaseSymbolFree(&obj->rdfGoDatabaseSymbol);
rdfGoReferenceFree(&obj->rdfGoReference);
freez(pObj);
}

void rdfGoDbxrefFreeList(struct rdfGoDbxref **pList)
/* Free up list of rdfGoDbxref. */
{
struct rdfGoDbxref *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    rdfGoDbxrefFree(&el);
    el = next;
    }
}

void rdfGoDbxrefSave(struct rdfGoDbxref *obj, int indent, FILE *f)
/* Save rdfGoDbxref to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<go:dbxref");
fprintf(f, " rdf:parseType=\"%s\"", obj->rdf_parseType);
fprintf(f, ">");
fprintf(f, "\n");
rdfGoDatabaseSymbolSave(obj->rdfGoDatabaseSymbol, indent+2, f);
rdfGoReferenceSave(obj->rdfGoReference, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</go:dbxref>\n");
}

struct rdfGoDbxref *rdfGoDbxrefLoad(char *fileName)
/* Load rdfGoDbxref from XML file where it is root element. */
{
struct rdfGoDbxref *obj;
xapParseAny(fileName, "go:dbxref", rdfStartHandler, rdfEndHandler, NULL, &obj);
return obj;
}

struct rdfGoDbxref *rdfGoDbxrefLoadNext(struct xap *xap)
/* Load next rdfGoDbxref element.  Use xapOpen to get xap. */
{
return xapNext(xap, "go:dbxref");
}

void rdfGoDatabaseSymbolFree(struct rdfGoDatabaseSymbol **pObj)
/* Free up rdfGoDatabaseSymbol. */
{
struct rdfGoDatabaseSymbol *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void rdfGoDatabaseSymbolFreeList(struct rdfGoDatabaseSymbol **pList)
/* Free up list of rdfGoDatabaseSymbol. */
{
struct rdfGoDatabaseSymbol *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    rdfGoDatabaseSymbolFree(&el);
    el = next;
    }
}

void rdfGoDatabaseSymbolSave(struct rdfGoDatabaseSymbol *obj, int indent, FILE *f)
/* Save rdfGoDatabaseSymbol to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<go:database_symbol>");
fprintf(f, "%s", obj->text);
fprintf(f, "</go:database_symbol>\n");
}

struct rdfGoDatabaseSymbol *rdfGoDatabaseSymbolLoad(char *fileName)
/* Load rdfGoDatabaseSymbol from XML file where it is root element. */
{
struct rdfGoDatabaseSymbol *obj;
xapParseAny(fileName, "go:database_symbol", rdfStartHandler, rdfEndHandler, NULL, &obj);
return obj;
}

struct rdfGoDatabaseSymbol *rdfGoDatabaseSymbolLoadNext(struct xap *xap)
/* Load next rdfGoDatabaseSymbol element.  Use xapOpen to get xap. */
{
return xapNext(xap, "go:database_symbol");
}

void rdfGoReferenceFree(struct rdfGoReference **pObj)
/* Free up rdfGoReference. */
{
struct rdfGoReference *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void rdfGoReferenceFreeList(struct rdfGoReference **pList)
/* Free up list of rdfGoReference. */
{
struct rdfGoReference *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    rdfGoReferenceFree(&el);
    el = next;
    }
}

void rdfGoReferenceSave(struct rdfGoReference *obj, int indent, FILE *f)
/* Save rdfGoReference to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<go:reference>");
fprintf(f, "%s", obj->text);
fprintf(f, "</go:reference>\n");
}

struct rdfGoReference *rdfGoReferenceLoad(char *fileName)
/* Load rdfGoReference from XML file where it is root element. */
{
struct rdfGoReference *obj;
xapParseAny(fileName, "go:reference", rdfStartHandler, rdfEndHandler, NULL, &obj);
return obj;
}

struct rdfGoReference *rdfGoReferenceLoadNext(struct xap *xap)
/* Load next rdfGoReference element.  Use xapOpen to get xap. */
{
return xapNext(xap, "go:reference");
}

void *rdfStartHandler(struct xap *xp, char *name, char **atts)
/* Called by xap with start tag.  Does most of the parsing work. */
{
struct xapStack *st = xp->stack+1;
int depth = xp->stackDepth;
int i;

if (sameString(name, "go:go"))
    {
    struct rdfGoGo *obj;
    AllocVar(obj);
    for (i=0; atts[i] != NULL; i += 2)
        {
        char *name = atts[i], *val = atts[i+1];
        if  (sameString(name, "xmlns:go"))
            obj->xmlns_go = cloneString(val);
        else if (sameString(name, "xmlns:rdf"))
            obj->xmlns_rdf = cloneString(val);
        }
    if (obj->xmlns_go == NULL)
        xapError(xp, "missing xmlns:go");
    if (obj->xmlns_rdf == NULL)
        xapError(xp, "missing xmlns:rdf");
    return obj;
    }
else if (sameString(name, "rdf:RDF"))
    {
    struct rdfRdfRDF *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "go:go"))
            {
            struct rdfGoGo *parent = st->object;
            slAddHead(&parent->rdfRdfRDF, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "go:term"))
    {
    struct rdfGoTerm *obj;
    AllocVar(obj);
    for (i=0; atts[i] != NULL; i += 2)
        {
        char *name = atts[i], *val = atts[i+1];
        if  (sameString(name, "rdf:about"))
            obj->rdf_about = cloneString(val);
        }
    if (obj->rdf_about == NULL)
        xapError(xp, "missing rdf:about");
    if (depth > 1)
        {
        if  (sameString(st->elName, "rdf:RDF"))
            {
            struct rdfRdfRDF *parent = st->object;
            slAddHead(&parent->rdfGoTerm, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "go:accession"))
    {
    struct rdfGoAccession *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "go:term"))
            {
            struct rdfGoTerm *parent = st->object;
            slAddHead(&parent->rdfGoAccession, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "go:name"))
    {
    struct rdfGoName *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "go:term"))
            {
            struct rdfGoTerm *parent = st->object;
            slAddHead(&parent->rdfGoName, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "go:dbxref"))
    {
    struct rdfGoDbxref *obj;
    AllocVar(obj);
    for (i=0; atts[i] != NULL; i += 2)
        {
        char *name = atts[i], *val = atts[i+1];
        if  (sameString(name, "rdf:parseType"))
            obj->rdf_parseType = cloneString(val);
        }
    if (obj->rdf_parseType == NULL)
        xapError(xp, "missing rdf:parseType");
    if (depth > 1)
        {
        if  (sameString(st->elName, "go:term"))
            {
            struct rdfGoTerm *parent = st->object;
            slAddHead(&parent->rdfGoDbxref, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "go:database_symbol"))
    {
    struct rdfGoDatabaseSymbol *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "go:dbxref"))
            {
            struct rdfGoDbxref *parent = st->object;
            slAddHead(&parent->rdfGoDatabaseSymbol, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "go:reference"))
    {
    struct rdfGoReference *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "go:dbxref"))
            {
            struct rdfGoDbxref *parent = st->object;
            slAddHead(&parent->rdfGoReference, obj);
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

void rdfEndHandler(struct xap *xp, char *name)
/* Called by xap with end tag.  Checks all required children are loaded. */
{
struct xapStack *stack = xp->stack;
if (sameString(name, "go:go"))
    {
    struct rdfGoGo *obj = stack->object;
    if (obj->rdfRdfRDF == NULL)
        xapError(xp, "Missing rdf:RDF");
    if (obj->rdfRdfRDF->next != NULL)
        xapError(xp, "Multiple rdf:RDF");
    }
else if (sameString(name, "rdf:RDF"))
    {
    struct rdfRdfRDF *obj = stack->object;
    if (obj->rdfGoTerm == NULL)
        xapError(xp, "Missing go:term");
    slReverse(&obj->rdfGoTerm);
    }
else if (sameString(name, "go:term"))
    {
    struct rdfGoTerm *obj = stack->object;
    if (obj->rdfGoAccession == NULL)
        xapError(xp, "Missing go:accession");
    if (obj->rdfGoAccession->next != NULL)
        xapError(xp, "Multiple go:accession");
    if (obj->rdfGoName == NULL)
        xapError(xp, "Missing go:name");
    if (obj->rdfGoName->next != NULL)
        xapError(xp, "Multiple go:name");
    if (obj->rdfGoDbxref == NULL)
        xapError(xp, "Missing go:dbxref");
    if (obj->rdfGoDbxref->next != NULL)
        xapError(xp, "Multiple go:dbxref");
    }
else if (sameString(name, "go:accession"))
    {
    struct rdfGoAccession *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "go:dbxref"))
    {
    struct rdfGoDbxref *obj = stack->object;
    if (obj->rdfGoDatabaseSymbol == NULL)
        xapError(xp, "Missing go:database_symbol");
    if (obj->rdfGoDatabaseSymbol->next != NULL)
        xapError(xp, "Multiple go:database_symbol");
    if (obj->rdfGoReference == NULL)
        xapError(xp, "Missing go:reference");
    if (obj->rdfGoReference->next != NULL)
        xapError(xp, "Multiple go:reference");
    }
else if (sameString(name, "go:database_symbol"))
    {
    struct rdfGoDatabaseSymbol *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "go:reference"))
    {
    struct rdfGoReference *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
#ifdef SOON
#endif /* SOON */
}

