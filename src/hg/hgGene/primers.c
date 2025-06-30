/* primers - do primers section. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "spDb.h"
#include "hgGene.h"
#include "hdb.h"
#include "net.h"


static boolean primersExist(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Return TRUE */
{
return(TRUE);
}

static void primersPrint(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Print out primers section. */
{
puts("<p style='margin:0'>Primer3Plus can design qPCR Primers that straddle exon-exon-junctions, which amplify only cDNA, not genomic DNA.<br>");
printPrimer3Anchor(globalTdb->table, curGeneId, curGeneChrom, curGeneStart, curGeneEnd);
puts("Click here to load the transcript sequence and exon structure into Primer3Plus</a></p>");

puts("<p style='margin-top:0.3em; margin-bottom:0.3em'>Exonprimer can design one pair of Sanger sequencing primers around every exon, located in non-genic sequence.<br>");
printf("<a href='https://ihg.helmholtz-munich.de/cgi-bin/primer/ExonPrimerUCSC.pl?db=%s&acc=%s'>Click here to open Exonprimer with this transcript</a></p>", database, geneId);
printf("<p style='margin-top:0.3em; margin-bottom:0.3em'>To design primers for a non-coding sequence, zoom to a region of interest and select from the drop-down menu: View &gt; In External Tools &gt; Primer3</p>");
}

struct section *primersSection(struct sqlConnection *conn, 
	struct hash *sectionRa)
/* Create primers section. */
{
struct section *section = sectionNew(sectionRa, "primers");
if (!section)
    return NULL;

section->exists = primersExist;
section->print = primersPrint;

return section;
}

