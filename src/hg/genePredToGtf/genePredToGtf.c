/* genePredToGtf - Convert genePred table or file to gtf.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "jksql.h"
#include "genePred.h"
#include "genePredReader.h"

static char const rcsid[] = "$Id: genePredToGtf.c,v 1.9 2006/06/07 17:06:57 acs Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "genePredToGtf - Convert genePred table or file to gtf.\n"
  "usage:\n"
  "   genePredToGtf database genePredTable output.gtf\n"
  "If database is 'file' then track is interpreted as a file\n"
  "rather than a table in database.\n"
  "options:\n"
  "   -utr   Add UTRs to the output\n"
  "   -order Order by chrom, txStart\n"
  "   -source=src set source name to uses\n"
  "Note: use refFlat table to include geneName\n"
  );
}

static struct optionSpec options[] = {
   {"utr", OPTION_BOOLEAN},
   {"order", OPTION_BOOLEAN},
   {"source", OPTION_STRING},
   {NULL, 0}
};

char *findUniqueName(struct hash *dupeHash, char *root)
/* If root name is already in hash, return root_1, root_2
 * or something like that. */
{
struct hashEl *hel;
if ((hel = hashLookup(dupeHash, root)) == NULL)
    {
    hashAddInt(dupeHash, root, 1);
    return root;
    }
else
    {
    static char buf[256];
    int val = ptToInt(hel->val) + 1;
    hel->val = intToPt(val);
    safef(buf, sizeof(buf), "%s_%d", root, val);
    return buf;
    }
}

static void writeGtfLine(FILE *f, char *source, char *name, char *geneName,
	char *chrom, char *strand, char *type, 
	int start, int end, int exonIx, int frame)
/* Write a single exon to GTF file */
{
/* convert frame to phase */
char phase = (frame < 0) ? '.' : (frame == 0) ? '0'
    : (frame == 1) ? '2' : '1';
fprintf(f, "%s\t", chrom);
fprintf(f, "%s\t", source);
fprintf(f, "%s\t", type);
fprintf(f, "%d\t", start+1);
fprintf(f, "%d\t", end);
fprintf(f, ".\t");	/* Score. */
fprintf(f, "%s\t", strand);
fprintf(f, "%c\t", phase);
fprintf(f, "gene_id \"%s\"; ", name);
fprintf(f, "transcript_id \"%s\"; ", name);
fprintf(f, "exon_number \"%d\"; ", exonIx+1);
fprintf(f, "exon_id \"%s.%d\";", name, exonIx+1);
if ((geneName != NULL) && (strlen(geneName)>0))
    fprintf(f, " gene_name \"%s\";", geneName);
fprintf(f, "\n");
}

struct codonCoords
/* coordinates of a codon */
{
    int start1, end1;   /* first part of codon */
    int iExon1;         /* exon containing first part */
    int start2, end2;   /* second part of codon if spliced, or 0,0 */
    int iExon2;         /* exon containing second part */
};

void writeCodon(FILE *f, char *source, char *name, char *geneName,
                char *chrom, char *strand, char *type, 
                struct codonCoords *codon)
/* write the location of a codon to the GTF  */
{
if (codon->start1 < codon->end1)
    {
    writeGtfLine(f, source, name, geneName, chrom, strand, type, 
                 codon->start1, codon->end1, codon->iExon1, 0);
        
    if (codon->start2 < codon->end2)
        writeGtfLine(f, source, name, geneName, chrom, strand, type, 
                     codon->start2, codon->end2, codon->iExon2, 
                     (codon->end1-codon->start1));
    }
}

void getStartStopCoords(struct genePred *gp, struct codonCoords *start,
                        struct codonCoords *end)
/* get the coordinates of the start and stop codons */
{
int i;
ZeroVar(start);
ZeroVar(end);

/* find the exons containing the first and last codons in gene */
for (i=0; i<gp->exonCount; ++i)
    {
    if ((gp->exonStarts[i] <= gp->cdsStart) && (gp->cdsStart < gp->exonEnds[i]))
        {
        /* exon contains CDS start */
        start->start1 = gp->cdsStart;
        start->end1 = start->start1 + 3;
        start->iExon1 = i;
        if (start->end1 >= gp->exonEnds[i])
            {
            start->end1 = gp->exonEnds[i];
            start->start2 = gp->exonStarts[i+1];
            start->end2 = start->start2 + (3 - (start->end1 - start->start1));
            start->iExon2 = i;
            }
        }
    if ((gp->exonStarts[i] < gp->cdsEnd) && (gp->cdsEnd <= gp->exonEnds[i]))
        {
        /* exon contains CDS end */
        end->start1 = gp->cdsEnd - 3;
        end->end1 = gp->cdsEnd;
        end->iExon1 = i;
        if (end->end1 >= gp->exonEnds[i])
            {
            end->end1 = gp->exonEnds[i];
            end->start2 = gp->exonStarts[i+1];
            end->end2 = end->start2 + (3 - (end->end1 - end->start1));
            end->iExon2 = i;
            }
        }
    }

/* if we have cds status, use it to determine if we have start/end */
if (gp->optFields & genePredCdsStatFld)
    {
    if (gp->cdsStartStat != cdsComplete)
        ZeroVar(start);
    if (gp->cdsEndStat != cdsComplete)
        ZeroVar(end);
    }
}

void genePredWriteToGtf(struct genePred *gp, char *source, 
	struct hash *dupeHash, FILE *f)
/* Write out genePredName to GTF file. */
{
int i;
char *name = findUniqueName(dupeHash, gp->name);
char *geneName = gp->name2;
char *chrom = gp->chrom;
char *strand = gp->strand;

for (i=0; i<gp->exonCount; ++i)
    {
    int start = gp->exonStarts[i];
    int end = gp->exonEnds[i];
    int exonStart = start;
    int exonEnd = end;
    int frame = (gp->optFields & genePredExonFramesFld) ? gp->exonFrames[i] : -1;

    writeGtfLine(f, source, name, geneName, chrom, strand, "exon", start, end, i, -1);
    if (start < gp->cdsStart) start = gp->cdsStart;
    if (end > gp->cdsEnd) end = gp->cdsEnd;
    if (optionExists("utr"))
	{
	if (start < end)
	    {
	    if (start > exonStart)
		writeGtfLine(f, source, name, geneName, chrom, strand, "UTR", exonStart, start, i, -1);
	    writeGtfLine(f, source, name, geneName, chrom, strand, "CDS", start, end, i, frame);
	    if (end < exonEnd)
		writeGtfLine(f, source, name, geneName, chrom, strand, "UTR", end, exonEnd, i, -1);
	    }
	else 
	    writeGtfLine(f, source, name, geneName, chrom, strand, "UTR", exonStart, exonEnd, i, -1);
	}
    else if (start < end)
	writeGtfLine(f, source, name, geneName, chrom, strand, "CDS", start, end, i, frame);
    }

if (gp->cdsStart < gp->cdsEnd)
    {
    struct codonCoords start, end;
    getStartStopCoords(gp, &start, &end);
    if (gp->strand[0] == '+')
        {
        writeCodon(f, source, name, geneName, chrom, strand, "start_codon", 
                   &start);
        writeCodon(f, source, name, geneName, chrom, strand, "stop_codon",
                   &end);
        }
    else
        {
        writeCodon(f, source, name, geneName, chrom, strand, "start_codon",
                   &end);
        writeCodon(f, source, name, geneName, chrom, strand, "stop_codon",
                   &start);
        }
    }
}

void genePredToGtf(char *database, char *table, char *gtfOut)
/* genePredToGtf - Convert genePredName table or file to gtf.. */
{
FILE *f = mustOpen(gtfOut, "w");
struct hash *dupeHash = newHash(16);
struct genePred *gpList = NULL, *gp = NULL;
char *source = optionVal("source", table);

if (sameString(database, "file"))
    {
    gpList = genePredReaderLoadFile(table, NULL);
    }
else
    {
    struct sqlConnection *conn = sqlConnect(database);
    gpList = genePredReaderLoadQuery(conn, table, NULL);
    sqlDisconnect(&conn);
    }
slSort(&gpList, genePredCmp);

for (gp = gpList; gp != NULL; gp = gp->next)
    genePredWriteToGtf(gp, source, dupeHash, f);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
genePredToGtf(argv[1], argv[2], argv[3]);
return 0;
}
