/* gfClient - A client for the genomic finding program that produces a .psl file. */
#include "common.h"
#include "linefile.h"
#include "aliType.h"
#include "fa.h"
#include "genoFind.h"
#include "psl.h"
#include "cheapcgi.h"

void gfClient(char *hostName, char *portName, char *nibDir, char *inName, 
	char *outName, char *tTypeName, char *qTypeName)
/* gfClient - A client for the genomic finding program that produces a .psl file. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
static bioSeq seq;
struct ssBundle *bundleList;
enum ffStringency stringency = ffCdna;
FILE *out = mustOpen(outName, "w");
enum gfType qType = gfTypeFromName(qTypeName);
enum gfType tType = gfTypeFromName(tTypeName);

if (!cgiVarExists("nohead"))
    pslWriteHead(out);
while (faSomeSpeedReadNext(lf, &seq.dna, &seq.size, &seq.name, qType != gftProt))
    {
    int conn = gfConnect(hostName, portName);
    printf("Processing %s\n", seq.name);
    if (qType == gftProt && (tType == gftDnaX || tType == gftRnaX))
        {
	static struct gfSavePslxData data;
	data.f = out;
	data.reportTargetStrand = TRUE;
	gfAlignTrans(conn, nibDir, &seq, 12, gfSavePslx, &data);
	}
    else if ((qType == gftRnaX || qType == gftDnaX) && (tType == gftDnaX || tType == gftRnaX))
        {
	static struct gfSavePslxData data;
	uglyf("Getting set to do the big trans/trans.\n");
	data.f = out;
	data.reportTargetStrand = TRUE;
	gfAlignTransTrans(conn, nibDir, &seq, FALSE, 12, gfSavePslx, &data);
	if (qType == gftDnaX)
	    {
	    reverseComplement(seq.dna, seq.size);
	    close(conn);
	    conn = gfConnect(hostName, portName);
	    gfAlignTransTrans(conn, nibDir, &seq, TRUE, 12, gfSavePslx, &data);
	    }
	}
    else if ((tType == gftDna || tType == gftRna) && (qType == gftDna || qType == gftRna))
	{
	gfAlignStrand(conn, nibDir, &seq, FALSE, stringency, 36, gfSavePsl, out);
	close(conn);
	conn = gfConnect(hostName, portName);
	reverseComplement(seq.dna, seq.size);
	gfAlignStrand(conn, nibDir, &seq, TRUE,  stringency, 36, gfSavePsl, out);
	}
    else
        {
	errAbort("Comparisons between %s queries and %s databases not yet supported",
		qTypeName, tTypeName);
	}
    close(conn);
    }
printf("Output is in %s\n", outName);
}

void usage()
/* Explain usage and exit. */
{
errAbort(
  "gfClient - A client for the genomic finding program that produces a .psl file\n"
  "usage:\n"
  "   gfClient host port nibDir in.fa out.psl\n"
  "where\n"
  "   host is the name of the machine running the gfServer\n"
  "   port is the same as you started the gfServer with\n"
  "   nibDir is the path of the nib files relative to the current dir\n"
  "       (note these are needed by the client as well as the server)\n"
  "   in.fa a fasta format file.  May contain multiple records\n"
  "   out.psl where to put the output\n"
  "options:\n"
  "   -t=type     Database type.  Type is one of:\n"
  "                 dna - DNA sequence\n"
  "                 prot - protein sequence\n"
  "                 dnax - DNA sequence translated in six frames to protein\n"
  "               The default is dna\n"
  "   -q=type     Query type.  Type is one of:\n"
  "                 dna - DNA sequence\n"
  "                 rna - RNA sequence\n"
  "                 prot - protein sequence\n"
  "                 dnax - DNA sequence translated in six frames to protein\n"
  "                 rnax - DNA sequence translated in three frames to protein\n"
  "   -nohead   Suppresses psl five line header");
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 6)
    usage();
gfClient(argv[1], argv[2], argv[3], argv[4], argv[5], cgiUsualString("t", "dna"), cgiUsualString("q", "dna"));
return 0;
}
