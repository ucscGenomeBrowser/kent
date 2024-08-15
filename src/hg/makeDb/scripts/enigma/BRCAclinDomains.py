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

outputText="""chr17\t41256276\t41256278\tRING\t0\t.\t41256276\t41256278\t230,3,131\tBRCA1\tNM_007294.4\t2-101\t<b>Domain: </b>RING</br><b>Transcript: </b>NM_007294.4</br><b>Amino acid loc:</b> 2-101
chr17\t41256884\t41256973\tRING\t0\t.\t41256884\t41256973\t230,3,131\tBRCA1\tNM_007294.4\t2-101\t<b>Domain: </b>RING</br><b>Transcript: </b>NM_007294.4</br><b>Amino acid loc:</b> 2-101
chr17\t41258472\t41258550\tRING\t0\t.\t41258472\t41258550\t230,3,131\tBRCA1\tNM_007294.4\t2-101\t<b>Domain: </b>RING</br><b>Transcript: </b>NM_007294.4</br><b>Amino acid loc:</b> 2-101
chr17\t41267742\t41267796\tRING\t0\t.\t41267742\t41267796\t230,3,131\tBRCA1\tNM_007294.4\t2-101\t<b>Domain: </b>RING</br><b>Transcript: </b>NM_007294.4</br><b>Amino acid loc:</b> 2-101
chr17\t41276033\t41276109\tRING\t0\t.\t41276033\t41276109\t230,3,131\tBRCA1\tNM_007294.4\t2-101\t<b>Domain: </b>RING</br><b>Transcript: </b>NM_007294.4</br><b>Amino acid loc:</b> 2-101
chr17\t41234506\t41234592\tCoiled-coil\t0\t.\t41234506\t41234592\t230,3,131\tBRCA1\tNM_007294.4\t1391-1424\t<b>Domain: </b>Coiled-coil</br><b>Transcript: </b>NM_007294.4</br><b>Amino acid loc:</b> 1391-1424
chr17\t41242960\t41242975\tCoiled-coil\t0\t.\t41242960\t41242975\t230,3,131\tBRCA1\tNM_007294.4\t1391-1424\t<b>Domain: </b>Coiled-coil</br><b>Transcript: </b>NM_007294.4</br><b>Amino acid loc:</b> 1391-1424
chr17\t41197716\t41197819\tBRTC repeats\t0\t.\t41197716\t41197819\t230,3,131\tBRCA1\tNM_007294.4\t1650-1857\t<b>Domain: </b>BRTC repeats</br><b>Transcript: </b>NM_007294.4</br><b>Amino acid loc:</b> 1650-1857
chr17\t41199659\t41199720\tBRTC repeats\t0\t.\t41199659\t41199720\t230,3,131\tBRCA1\tNM_007294.4\t1650-1857\t<b>Domain: </b>BRTC repeats</br><b>Transcript: </b>NM_007294.4</br><b>Amino acid loc:</b> 1650-1857
chr17\t41201137\t41201211\tBRTC repeats\t0\t.\t41201137\t41201211\t230,3,131\tBRCA1\tNM_007294.4\t1650-1857\t<b>Domain: </b>BRTC repeats</br><b>Transcript: </b>NM_007294.4</br><b>Amino acid loc:</b> 1650-1857
chr17\t41203079\t41203134\tBRTC repeats\t0\t.\t41203079\t41203134\t230,3,131\tBRCA1\tNM_007294.4\t1650-1857\t<b>Domain: </b>BRTC repeats</br><b>Transcript: </b>NM_007294.4</br><b>Amino acid loc:</b> 1650-1857
chr17\t41209068\t41209152\tBRTC repeats\t0\t.\t41209068\t41209152\t230,3,131\tBRCA1\tNM_007294.4\t1650-1857\t<b>Domain: </b>BRTC repeats</br><b>Transcript: </b>NM_007294.4</br><b>Amino acid loc:</b> 1650-1857
chr17\t41215349\t41215390\tBRTC repeats\t0\t.\t41215349\t41215390\t230,3,131\tBRCA1\tNM_007294.4\t1650-1857\t<b>Domain: </b>BRTC repeats</br><b>Transcript: </b>NM_007294.4</br><b>Amino acid loc:</b> 1650-1857
chr17\t41215890\t41215968\tBRTC repeats\t0\t.\t41215890\t41215968\t230,3,131\tBRCA1\tNM_007294.4\t1650-1857\t<b>Domain: </b>BRTC repeats</br><b>Transcript: </b>NM_007294.4</br><b>Amino acid loc:</b> 1650-1857
chr17\t41219624\t41219712\tBRTC repeats\t0\t.\t41219624\t41219712\t230,3,131\tBRCA1\tNM_007294.4\t1650-1857\t<b>Domain: </b>BRTC repeats</br><b>Transcript: </b>NM_007294.4</br><b>Amino acid loc:</b> 1650-1857
chr17\t41222944\t41222983\tBRTC repeats\t0\t.\t41222944\t41222983\t230,3,131\tBRCA1\tNM_007294.4\t1650-1857\t<b>Domain: </b>BRTC repeats</br><b>Transcript: </b>NM_007294.4</br><b>Amino acid loc:</b> 1650-1857
chr13\t32930572\t32930746\tDNA BD\t0\t.\t32930572\t32930746\t230,3,131\tBRCA2\tNM_000059.4\t2481-3186\t<b>Domain: </b>DNA BD</br><b>Transcript: </b>NM_000059.4</br><b>Amino acid loc:</b> 2481-3186
chr13\t32931878\t32932066\tDNA BD\t0\t.\t32931878\t32932066\t230,3,131\tBRCA2\tNM_000059.4\t2481-3186\t<b>Domain: </b>DNA BD</br><b>Transcript: </b>NM_000059.4</br><b>Amino acid loc:</b> 2481-3186
chr13\t32936659\t32936830\tDNA BD\t0\t.\t32936659\t32936830\t230,3,131\tBRCA2\tNM_000059.4\t2481-3186\t<b>Domain: </b>DNA BD</br><b>Transcript: </b>NM_000059.4</br><b>Amino acid loc:</b> 2481-3186
chr13\t32937315\t32937670\tDNA BD\t0\t.\t32937315\t32937670\t230,3,131\tBRCA2\tNM_000059.4\t2481-3186\t<b>Domain: </b>DNA BD</br><b>Transcript: </b>NM_000059.4</br><b>Amino acid loc:</b> 2481-3186
chr13\t32944538\t32944694\tDNA BD\t0\t.\t32944538\t32944694\t230,3,131\tBRCA2\tNM_000059.4\t2481-3186\t<b>Domain: </b>DNA BD</br><b>Transcript: </b>NM_000059.4</br><b>Amino acid loc:</b> 2481-3186
chr13\t32945092\t32945237\tDNA BD\t0\t.\t32945092\t32945237\t230,3,131\tBRCA2\tNM_000059.4\t2481-3186\t<b>Domain: </b>DNA BD</br><b>Transcript: </b>NM_000059.4</br><b>Amino acid loc:</b> 2481-3186
chr13\t32950806\t32950928\tDNA BD\t0\t.\t32950806\t32950928\t230,3,131\tBRCA2\tNM_000059.4\t2481-3186\t<b>Domain: </b>DNA BD</br><b>Transcript: </b>NM_000059.4</br><b>Amino acid loc:</b> 2481-3186
chr13\t32953453\t32953652\tDNA BD\t0\t.\t32953453\t32953652\t230,3,131\tBRCA2\tNM_000059.4\t2481-3186\t<b>Domain: </b>DNA BD</br><b>Transcript: </b>NM_000059.4</br><b>Amino acid loc:</b> 2481-3186
chr13\t32953886\t32954050\tDNA BD\t0\t.\t32953886\t32954050\t230,3,131\tBRCA2\tNM_000059.4\t2481-3186\t<b>Domain: </b>DNA BD</br><b>Transcript: </b>NM_000059.4</br><b>Amino acid loc:</b> 2481-3186
chr13\t32954143\t32954282\tDNA BD\t0\t.\t32954143\t32954282\t230,3,131\tBRCA2\tNM_000059.4\t2481-3186\t<b>Domain: </b>DNA BD</br><b>Transcript: </b>NM_000059.4</br><b>Amino acid loc:</b> 2481-3186
chr13\t32968825\t32969070\tDNA BD\t0\t.\t32968825\t32969070\t230,3,131\tBRCA2\tNM_000059.4\t2481-3186\t<b>Domain: </b>DNA BD</br><b>Transcript: </b>NM_000059.4</br><b>Amino acid loc:</b> 2481-3186
chr13\t32971034\t32971091\tDNA BD\t0\t.\t32971034\t32971091\t230,3,131\tBRCA2\tNM_000059.4\t2481-3186\t<b>Domain: </b>DNA BD</br><b>Transcript: </b>NM_000059.4</br><b>Amino acid loc:</b> 2481-3186
chr13\t32890627\t32890664\tPALB2 BD\t0\t.\t32890627\t32890664\t230,3,131\tBRCA2\tNM_000059.4\t10-40\t<b>Domain: </b>PALB2 BD</br><b>Transcript: </b>NM_000059.4</br><b>Amino acid loc:</b> 10-40
chr13\t32893213\t32893266\tPALB2 BD\t0\t.\t32893213\t32893266\t230,3,131\tBRCA2\tNM_000059.4\t10-40\t<b>Domain: </b>PALB2 BD</br><b>Transcript: </b>NM_000059.4</br><b>Amino acid loc:</b> 10-40
"""

outputBedFile.write(outputText)


outputBedFile.close()

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
