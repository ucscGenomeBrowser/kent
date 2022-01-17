/* pfamToGoRdf - Turn pfam2go.rdf file to a simple table.. */

/* Copyright (C) 2007 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "xap.h"
#include "rdf.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pfamToGoRdf - Turn pfam2go.rdf file to a simple table. The\n"
  "pfam2go.rdf is an XML format downloaded from\n"
  "    http://www.berkeleybop.org/ontologies/#mappings\n"
  "usage:\n"
  "   pfamToGoRdf pfam2go.rdf pfam2go.tab\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void pfamToGoRdf(char *inRdf, char *outTab)
/* pfamToGoRdf - Turn pfam2go.rdf file to a simple table.. */
{
struct rdfGoGo *go = rdfGoGoLoad(inRdf);
FILE *f = mustOpen(outTab, "w");
struct rdfGoTerm *term;
for (term = go->rdfRdfRDF->rdfGoTerm; term != NULL; term = term->next)
    {
    fprintf(f, "%s\t%s\n", term->rdfGoDbxref->rdfGoReference->text,
    	term->rdfGoAccession->text);
    }
carefulClose(&f);
}

#ifdef SOON
int indent = 0;

void *startHandler(struct xap *xap, char *name, char **atts)
/* Called at the start of a tag after attributes are parsed. */
{
spaceOut(stdout, indent*3);
++indent;
printf("%s\n", name);
return NULL;
}

void endHandler(struct xap *xap, char *name)
/* Called at the start of a tag after attributes are parsed. */
{
--indent;
spaceOut(stdout, indent*3);
printf("/%s\n", name);
}

void pfamToGoRdf(char *inRdf, char *outTab)
/* pfamToGoRdf - Turn pfam2go.rdf file to a simple table.. */
{
struct xap *xap = xapNew(startHandler, endHandler, inRdf); 
xapParseFile(xap, inRdf);
}
#endif /* SOON */

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
pfamToGoRdf(argv[1], argv[2]);
return 0;
}
