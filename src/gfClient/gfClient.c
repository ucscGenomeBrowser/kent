/* gfClient - A client for the genomic finding program that produces a .psl file. */
#include "common.h"
#include "linefile.h"
#include "fa.h"
#include "genoFind.h"
#include "psl.h"
#include "cheapcgi.h"

void gfClient(char *hostName, char *portName, char *nibDir, char *inName, 
	char *outName, boolean tx)
/* gfClient - A client for the genomic finding program that produces a .psl file. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
static bioSeq seq;
struct ssBundle *bundleList;
enum ffStringency stringency = ffCdna;
FILE *out = mustOpen(outName, "w");
int conn = gfConnect(hostName, portName);

if (!cgiVarExists("nohead"))
    pslWriteHead(out);
while (faSomeSpeedReadNext(lf, &seq.dna, &seq.size, &seq.name, !tx))
    {
    printf("Processing %s\n", seq.name);
    if (tx)
        {
	static struct gfSavePslxData data;
	uglyf("got tx\n");
	data.f = out;
	data.reportTargetStrand = TRUE;
	gfAlignTrans(conn, nibDir, &seq, 12, gfSavePslx, &data);
	}
    else
	{
	gfAlignStrand(conn, nibDir, &seq, FALSE, stringency, 36, gfSavePsl, out);
	close(conn);
	conn = gfConnect(hostName, portName);
	reverseComplement(seq.dna, seq.size);
	gfAlignStrand(conn, nibDir, &seq, TRUE,  stringency, 36, gfSavePsl, out);
	}
    }
printf("Output is in %s\n", outName);
close(conn);
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
  "   -tx   Compare protein query to translated database\n"
  "   -nohead   Suppresses psl five line header");
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 6)
    usage();
gfClient(argv[1], argv[2], argv[3], argv[4], argv[5], cgiVarExists("tx"));
return 0;
}
