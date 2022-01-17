/* rdf.h autoXml generated file */

/* Copyright (C) 2007 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#ifndef RDF_H
#define RDF_H

#ifndef XAP_H
#include "xap.h"
#endif

/* The start and end handlers here are used with routines defined in xap.h.
 * In particular if you want to read just parts of the XML file into memory
 * call xapOpen() with these, and then xapNext() with the name of the tag
 * you want to load. */

void *rdfStartHandler(struct xap *xp, char *name, char **atts);
/* Called by xap with start tag.  Does most of the parsing work. */

void rdfEndHandler(struct xap *xp, char *name);
/* Called by xap with end tag.  Checks all required children are loaded. */


struct rdfGoGo
    {
    struct rdfGoGo *next;
    char *xmlns_go;	/* Required */
    char *xmlns_rdf;	/* Required */
    struct rdfRdfRDF *rdfRdfRDF;	/** Single instance required. **/
    };

void rdfGoGoFree(struct rdfGoGo **pObj);
/* Free up rdfGoGo. */

void rdfGoGoFreeList(struct rdfGoGo **pList);
/* Free up list of rdfGoGo. */

void rdfGoGoSave(struct rdfGoGo *obj, int indent, FILE *f);
/* Save rdfGoGo to file. */

struct rdfGoGo *rdfGoGoLoad(char *fileName);
/* Load rdfGoGo from XML file where it is root element. */

struct rdfGoGo *rdfGoGoLoadNext(struct xap *xap);
/* Load next rdfGoGo element.  Use xapOpen to get xap. */

struct rdfRdfRDF
    {
    struct rdfRdfRDF *next;
    struct rdfGoTerm *rdfGoTerm;	/** Non-empty list required. **/
    };

void rdfRdfRDFFree(struct rdfRdfRDF **pObj);
/* Free up rdfRdfRDF. */

void rdfRdfRDFFreeList(struct rdfRdfRDF **pList);
/* Free up list of rdfRdfRDF. */

void rdfRdfRDFSave(struct rdfRdfRDF *obj, int indent, FILE *f);
/* Save rdfRdfRDF to file. */

struct rdfRdfRDF *rdfRdfRDFLoad(char *fileName);
/* Load rdfRdfRDF from XML file where it is root element. */

struct rdfRdfRDF *rdfRdfRDFLoadNext(struct xap *xap);
/* Load next rdfRdfRDF element.  Use xapOpen to get xap. */

struct rdfGoTerm
    {
    struct rdfGoTerm *next;
    char *rdf_about;	/* Required */
    struct rdfGoAccession *rdfGoAccession;	/** Single instance required. **/
    struct rdfGoName *rdfGoName;	/** Single instance required. **/
    struct rdfGoDbxref *rdfGoDbxref;	/** Single instance required. **/
    };

void rdfGoTermFree(struct rdfGoTerm **pObj);
/* Free up rdfGoTerm. */

void rdfGoTermFreeList(struct rdfGoTerm **pList);
/* Free up list of rdfGoTerm. */

void rdfGoTermSave(struct rdfGoTerm *obj, int indent, FILE *f);
/* Save rdfGoTerm to file. */

struct rdfGoTerm *rdfGoTermLoad(char *fileName);
/* Load rdfGoTerm from XML file where it is root element. */

struct rdfGoTerm *rdfGoTermLoadNext(struct xap *xap);
/* Load next rdfGoTerm element.  Use xapOpen to get xap. */

struct rdfGoAccession
    {
    struct rdfGoAccession *next;
    char *text;
    };

void rdfGoAccessionFree(struct rdfGoAccession **pObj);
/* Free up rdfGoAccession. */

void rdfGoAccessionFreeList(struct rdfGoAccession **pList);
/* Free up list of rdfGoAccession. */

void rdfGoAccessionSave(struct rdfGoAccession *obj, int indent, FILE *f);
/* Save rdfGoAccession to file. */

struct rdfGoAccession *rdfGoAccessionLoad(char *fileName);
/* Load rdfGoAccession from XML file where it is root element. */

struct rdfGoAccession *rdfGoAccessionLoadNext(struct xap *xap);
/* Load next rdfGoAccession element.  Use xapOpen to get xap. */

struct rdfGoName
    {
    struct rdfGoName *next;
    };

void rdfGoNameFree(struct rdfGoName **pObj);
/* Free up rdfGoName. */

void rdfGoNameFreeList(struct rdfGoName **pList);
/* Free up list of rdfGoName. */

void rdfGoNameSave(struct rdfGoName *obj, int indent, FILE *f);
/* Save rdfGoName to file. */

struct rdfGoName *rdfGoNameLoad(char *fileName);
/* Load rdfGoName from XML file where it is root element. */

struct rdfGoName *rdfGoNameLoadNext(struct xap *xap);
/* Load next rdfGoName element.  Use xapOpen to get xap. */

struct rdfGoDbxref
    {
    struct rdfGoDbxref *next;
    char *rdf_parseType;	/* Required */
    struct rdfGoDatabaseSymbol *rdfGoDatabaseSymbol;	/** Single instance required. **/
    struct rdfGoReference *rdfGoReference;	/** Single instance required. **/
    };

void rdfGoDbxrefFree(struct rdfGoDbxref **pObj);
/* Free up rdfGoDbxref. */

void rdfGoDbxrefFreeList(struct rdfGoDbxref **pList);
/* Free up list of rdfGoDbxref. */

void rdfGoDbxrefSave(struct rdfGoDbxref *obj, int indent, FILE *f);
/* Save rdfGoDbxref to file. */

struct rdfGoDbxref *rdfGoDbxrefLoad(char *fileName);
/* Load rdfGoDbxref from XML file where it is root element. */

struct rdfGoDbxref *rdfGoDbxrefLoadNext(struct xap *xap);
/* Load next rdfGoDbxref element.  Use xapOpen to get xap. */

struct rdfGoDatabaseSymbol
    {
    struct rdfGoDatabaseSymbol *next;
    char *text;
    };

void rdfGoDatabaseSymbolFree(struct rdfGoDatabaseSymbol **pObj);
/* Free up rdfGoDatabaseSymbol. */

void rdfGoDatabaseSymbolFreeList(struct rdfGoDatabaseSymbol **pList);
/* Free up list of rdfGoDatabaseSymbol. */

void rdfGoDatabaseSymbolSave(struct rdfGoDatabaseSymbol *obj, int indent, FILE *f);
/* Save rdfGoDatabaseSymbol to file. */

struct rdfGoDatabaseSymbol *rdfGoDatabaseSymbolLoad(char *fileName);
/* Load rdfGoDatabaseSymbol from XML file where it is root element. */

struct rdfGoDatabaseSymbol *rdfGoDatabaseSymbolLoadNext(struct xap *xap);
/* Load next rdfGoDatabaseSymbol element.  Use xapOpen to get xap. */

struct rdfGoReference
    {
    struct rdfGoReference *next;
    char *text;
    };

void rdfGoReferenceFree(struct rdfGoReference **pObj);
/* Free up rdfGoReference. */

void rdfGoReferenceFreeList(struct rdfGoReference **pList);
/* Free up list of rdfGoReference. */

void rdfGoReferenceSave(struct rdfGoReference *obj, int indent, FILE *f);
/* Save rdfGoReference to file. */

struct rdfGoReference *rdfGoReferenceLoad(char *fileName);
/* Load rdfGoReference from XML file where it is root element. */

struct rdfGoReference *rdfGoReferenceLoadNext(struct xap *xap);
/* Load next rdfGoReference element.  Use xapOpen to get xap. */

#endif /* RDF_H */

