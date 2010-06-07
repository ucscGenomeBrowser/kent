/* dataMatrixToSoft.c - Take a column from a data matrix and output
   the appropriate soft (file format from geo) sample file. */
#include "common.h"
#include "dMatrix.h"
#include "linefile.h"

void usage() {
errAbort("dataMatrixToSoft - Take a data matrix and output a SOFT formatted\n"
	 "file for a particular sample. \n"
	 "\n"
	 "usage:\n"
	 "  dataMatrixToSoft columnDescriptions matrixFile\n"
	 );
}

void writeOutSoftFile(char **words, int wordCount, 
		      struct dMatrix *matrix, int colIx)
/* Write out a sample in soft format. */
{
int rowIx = 0;
int wordIx = 0;
printf("^PLATFORM=1\n");
printf("!Platform_GEO_accession = XXXXXX\n");
printf("^SAMPLE=%s\n", words[wordIx++]);
printf("!Sample_title = %s\n", words[wordIx++]);
printf("!Sample_source_name = %s\n", words[wordIx++]);
printf("     !Sample_organism = Mus musculus\n");
/* printf("     !Sample_characteristics =  wall\n"); */
printf("!Sample_molecule = total RNA\n");
printf("     !Sample_extract_protocol = Extracted using the Invitrogen Micro-to-Midi Total RNA Purification System (Catalog # 12183-018). RNA samples were treated with DNAseI for 30 minutes at 37, extracted twice with phenol/chloroform, once with chloroform and ethanol precipitated.\n");
printf("!Sample_label = biotin\n");
printf("!Sample_label_protocol = RNA was primed with random hexamers and reverse transcribed. After the reaction was completed, RNA was removed from the reaction by alkaline hydrolysis and the cDNA was purified using Qiagen PCR Quick Purification Kit.  A typical reaction started with 5-6ug of total RNA usually yielded ~3ug of cDNA. The cDNA was then fragmented using DNAseI in an empirically controlled reaction that yields DNA fragments of 50-200 bases. This fragmented cDNA was then end labeled using terminal deoxynucleotidyl transferase and \"DNA-Labeling-Reagent-1a (DLR-1a)\", which is a biotinylated dideoxynucleoside triphosphate.\n");
printf("!Sample_hyb_protocol = Targets were hybridized to chips in 7% DMSO solution for 16 hrs overnight at 50. Microarrays were washed, processed with anti-biotin antibodies and streptavidin-phycoerythrin according to the standard Affymetrix protocol.\n");
printf("!Sample_scan_protocol = standard Affymetrix procedures\n");
printf("!Sample_description = %s\n", words[wordIx]++);
printf("!Sample_data_processing = Intensity values from the DNA arrays were normalized using a quantile normalization (Bolstad et al., 2003), and probe set summaries were derived using the Robust Multi-chip Analysis (RMA) procedure (Irizarry et al., 2003a; Irizarry et al., 2003b) with two modifications.  The first modification was to remove all probes with 17 or more continuous bases that match to any other mouse transcript in order to minimize cross-hybridization issues.  The second modification was to use the mode of the probe intensity values of similar GC content probes for the background estimate of a particular probe.  For example, if a probe has a GC count of 16, then the mode of the intensity of all the probes with a GC count of 16 was used as a background estimate as opposed to RMA in which the mode of all the probes is used as a background estimate for all the probes.\n");
printf("!Sample_platform_id = 1\n");
printf("#ID_REF = \n");
printf("#VALUE = Probe Set Summary Estimate\n");
printf("!Sample_table_begin\n");
printf("ID_REFVALUE\n");
for(rowIx = 0; rowIx < matrix->rowCount; rowIx++) 
    {
    printf("%s\t%.2f\n", matrix->rowNames[rowIx], matrix->matrix[rowIx][colIx]);
    }
}

void makeSoftFormat(char *descriptions, char *matrixFile) 
/* Load up everything. */
{
struct dMatrix *matrix = NULL;
char *words[5];
int columnCount = 0;
int wordCount = 0;
struct lineFile *lf = NULL;
warn("Loading file %s", matrixFile);
matrix = dMatrixLoad(matrixFile);
lf = lineFileOpen(descriptions, TRUE);
while(lineFileNextRow(lf, words, ArraySize(words))) 
    {
    warn("Doing column %d", columnCount);
    writeOutSoftFile(words, wordCount, matrix, columnCount++);
    }

}

int main(int argc, char *argv[]) 
/* Everybody's favorite function. */
{
if(argc != 3)
    usage();
makeSoftFormat(argv[1], argv[2]);
return 0;
}
