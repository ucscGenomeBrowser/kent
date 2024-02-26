import subprocess

def bash(cmd):
    """Run the cmd in bash subprocess"""
    try:
        rawBashOutput = subprocess.run(cmd, check=True, shell=True,\
                                       stdout=subprocess.PIPE, universal_newlines=True, stderr=subprocess.STDOUT)
        bashStdoutt = rawBashOutput.stdout
    except subprocess.CalledProcessError as e:
        raise RuntimeError("command '{}' return with error (code {}): {}".format(e.cmd, e.returncode, e.output))
    return(bashStdoutt)

outputBedFile = open("/hive/data/inside/enigmaTracksData/outputBedFile.bed",'w')

outputText="""chr17\t41256276\t41276109\tRING\t0\t.\t41256276\t41276109\t255,0,0\tBRCA1\tNM_007294.4\t2-101\t<b>Domain: </b>RING</br><b>Transcript: </b>NM_007294.4</br><b>Amino acid loc:</b> 2-101
chr17\t41234506\t41242975\tCoiled-coil\t0\t.\t41234506\t41242975\t255,0,0\tBRCA1\tNM_007294.4\t1391-1424\t<b>Domain: </b>Coiled-coil</br><b>Transcript: </b>NM_007294.4</br><b>Amino acid loc:</b> 1391-1424
chr17\t41197716\t41222983\tBRTC repeats\t0\t.\t41197716\t41222983\t255,0,0\tBRCA1\tNM_007294.4\t1650-1857\t<b>Domain: </b>BRTC repeats</br><b>Transcript: </b>NM_007294.4</br><b>Amino acid loc:</b> 1650-1857
chr13\t32890627\t32893266\tPALB2 BD\t0\t.\t32890627\t32893266\t255,0,0\tBRCA2\tNM_000059.4\t10-40\t<b>Domain: </b>PALB2 BD</br><b>Transcript: </b>NM_000059.4</br><b>Amino acid loc:</b> 10-40
chr13\t32930572\t32971091\tDNA BD\t0\t.\t32930572\t32971091\t255,0,0\tBRCA2\tNM_000059.4\t2481-3186\t<b>Domain: </b>DNA BD</br><b>Transcript: </b>NM_000059.4</br><b>Amino acid loc:</b> 2481-3186
"""

outputBedFile.write(outputText)

            
outputBedFile.close()
rawFileNoHeader.close()

bash("bedSort /hive/data/inside/enigmaTracksData/outputBedFile.bed \
/hive/data/inside/enigmaTracksData/outputBedFile.bed")

startOfAsFile="""table BRCAclinDomains
"BRCA1 and BRCA2 ENIGMA clinically relevant protein domains (ENIGMA specifications version 1.1.0)"
   (
   string chrom;       "Reference sequence chromosome or scaffold"
   uint   chromStart;  "Start position in chromosome"
   uint   chromEnd;    "End position in chromosome"
   string name;        "HGVS Nucleotide"
   uint score;         "Not used, all 0"
   char[1] strand;     "Not used, all ."
   uint thickStart;    "Same as chromStart"
   uint thickEnd;      "Same as chromEnd"
   uint reserved;      "RGB value (use R,G,B string in input file)"
   string geneSymbol;  "Gene symbol"
   string NMaccession; "NCBI NM isoform accession"
   string AAlocation;  "Amino acid location of domain"
   string _mouseOver;  "Field only used as mouseOver"
   )"""

asFileOutput = open("/hive/data/inside/enigmaTracksData/BRCAclinDomains.as","w")
asFileOutput.write(startOfAsFile)
asFileOutput.close()

bash("bedToBigBed -as=/hive/data/inside/enigmaTracksData/BRCAclinDomains.as -type=bed9+4 -tab \
/hive/data/inside/enigmaTracksData/outputBedFile.bed /cluster/data/hg19/chrom.sizes \
/hive/data/inside/enigmaTracksData/BRCAclinDomainsHg19.bb")

bash("liftOver -bedPlus=9 -tab /hive/data/inside/enigmaTracksData/outputBedFile.bed \
/hive/data/genomes/hg19/bed/liftOver/hg19ToHg38.over.chain.gz \
/hive/data/inside/enigmaTracksData/outputBedFileHg38.bed /hive/data/inside/enigmaTracksData/unmapped.bed")

bash("bedToBigBed -as=/hive/data/inside/enigmaTracksData/BRCAclinDomains.as -type=bed9+4 -tab \
/hive/data/inside/enigmaTracksData/outputBedFileHg38.bed /cluster/data/hg38/chrom.sizes \
/hive/data/inside/enigmaTracksData/BRCAclinDomainsHg38.bb")

bash("ln -sf /hive/data/inside/enigmaTracksData/BRCAclinDomainsHg38.bb /gbdb/hg38/bbi/enigma/BRCAclinDomains.bb")
bash("ln -sf /hive/data/inside/enigmaTracksData/BRCAclinDomainsHg19.bb /gbdb/hg19/bbi/enigma/BRCAclinDomains.bb")
