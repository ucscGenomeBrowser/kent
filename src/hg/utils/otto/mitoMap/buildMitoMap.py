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

# Need -1 to all start positions
# 3 T-C needs to end up as  chrM 2 3
# 46 TG-del needs to end up as chrM 45 47
# One item at 16289 with no AA change
# 3 items with 366 G-GAAAG

pwd = "/hive/data/outside/otto/mitoMap/"

mitoMapControlRegionVarsFileInputFile = pwd+"variantsControl.latest.tsv"
mitoMapVarsOutputFile = pwd+"mitoMapVars.bed"
fh = open(mitoMapControlRegionVarsFileInputFile,"r", encoding="utf-8", errors="replace")
fh2 = open(mitoMapVarsOutputFile,"w")

# $ head VariantsControlMITOMAPFoswiki.tsv
# Position        Locus   Nucleotide Change       GB FreqFL (CR)*‡        GB Seqstotal (FL/CR)*   Curated References
# 3       Control Region  T-C     0.000%(0.000%)  0       2

# I want the output to be bed9 for itemRgb and mouseover with 10 additional fields. The 6 original, plus 3 more from the coding file plus mouseOver:
# chrom chromStart chromEnd name score strand thickStart thickEnd itemRgb position locus nucleotideChange GBfreq GBseqs curatedRefs _mouseOver
for line in fh:
    if line.startswith("Position"):
        continue
    else:
        line = line.rstrip().split("\t")
        position = line[0]
        nucleotideChange = line[2]
        chrom = "chrM"
        chromStart = int(position)-1
        if nucleotideChange == "":
            nucleotideChange = "Blank"
            chromEnd = chromStart + 1
        else:
            if " " in nucleotideChange: #3 items with 366 G-GAAAG
                # print(nucleotideChange) #For troubleshooting
                if "or" in nucleotideChange:
                    chromEnd = chromStart + len(nucleotideChange.split(" or ")[0].split("-")[0])
                else:
                    chromEnd = chromStart + len(nucleotideChange.split("-")[0].split(" ")[1])
            else:
                chromEnd = chromStart + len(nucleotideChange.split("-")[0])
        name = nucleotideChange
        score = "0"
        strand = "."
        thickStart = chromStart
        thickEnd = chromEnd
        itemRgb = "83,175,1"
        locus = line[1]
        codonNumber = ""
        codonPosition = ""
        AAchange = ""
        GBfreq = line[3]
        GBseqs = line[4]
        curatedRefs = line[5]
        varType = "VariantsControl"
        mouseOver = "<b>Nucl change: </b>"+name+"<br><b>Position: </b>"+position+"<br><b>Locus: </b>"+locus+"<br>"+\
        "<b>GenBank freq FL(CR): </b>"+GBfreq+"<br><b>GenBank seqs FL(CR): </b>"+GBseqs+"<br><b># of curated refs: </b>"+curatedRefs
        
        fh2.write("\t".join(map(str, [chrom, chromStart, chromEnd, name, score, strand, thickStart, thickEnd, itemRgb, \
                                  position, locus, nucleotideChange, codonNumber, codonPosition, AAchange, GBfreq, GBseqs, curatedRefs, varType, mouseOver])) + "\n")

fh2.close()
fh.close()

mitoMapCodingRegionVarsFileInputFile = pwd+"variantsCoding.latest.tsv"
mitoMapVarsOutputFile = pwd+"mitoMapVars.bed"
fh = open(mitoMapCodingRegionVarsFileInputFile,"r", encoding="utf-8", errors="replace")
fh2 = open(mitoMapVarsOutputFile,"a")

# $ head VariantsCodingMITOMAPFoswiki.tsv
#Position        Locus   Nucleotide Change       Codon Number    Codon Position  Amino Acid Change       GB Freq‡        GB Seqs Curated References
#577     MT-TF   G-A     -       -       tRNA    0.000%  0       1

# I want the output to be bed9 for itemRgb and mouseover with 10 additional fields. The 6 original, plus 3 more from the coding file plus mouseOver:
# chrom chromStart chromEnd name score strand thickStart thickEnd itemRgb position locus nucleotideChange GBfreq GBseqs curatedRefs _mouseOver
for line in fh:
    if line.startswith("Position"):
        continue
    else:
        line = line.rstrip().split("\t")
        position = line[0]
        nucleotideChange = line[2]
        chrom = "chrM"
        chromStart = int(position)-1
        if nucleotideChange == "":
            nucleotideChange = "Blank"
            chromEnd = chromStart + 1
        else:
            if " " in nucleotideChange: #3 items with 366 G-GAAAG
                # print(nucleotideChange) #For troubleshooting
                if "or" in nucleotideChange:
                    chromEnd = chromStart + len(nucleotideChange.split(" or ")[0].split("-")[0])
                else:
                    chromEnd = chromStart + len(nucleotideChange.split("-")[0].split(" ")[1])
            else:
                chromEnd = chromStart + len(nucleotideChange.split("-")[0])
        name = nucleotideChange
        score = "0"
        strand = "."
        thickStart = chromStart
        thickEnd = chromEnd
        itemRgb = "62,137,211"
        locus = line[1]
        codonNumber = line[3]
        codonPosition = line[4]
        AAchange = line[5]
        GBfreq = line[6]
        GBseqs = line[7]
        curatedRefs = line[8]
        varType = "VariantsCoding"
        mouseOver = "<b>Nucl change: </b>"+name+"<br><b>Position: </b>"+position+"<br><b>Locus: </b>"+locus+"<br>"+\
        "<b>Codon num: </b>"+codonNumber+"<br><b>Codon pos: </b>"+codonPosition+"<br><b>AA change: </b>"+AAchange+\
        "<br><b>GenBank freq FL(CR): </b>"+GBfreq+"<br><b>GenBank seqs FL(CR): </b>"+GBseqs+"<br><b># of curated refs: </b>"+curatedRefs

        fh2.write("\t".join(map(str, [chrom, chromStart, chromEnd, name, score, strand, thickStart, thickEnd, itemRgb, \
                                  position, locus, nucleotideChange, codonNumber, codonPosition, AAchange, GBfreq, GBseqs, curatedRefs, varType, mouseOver])) + "\n")

fh2.close()
fh.close()

bash("bedSort "+mitoMapVarsOutputFile+" "+mitoMapVarsOutputFile)
bash("bedToBigBed -as="+pwd+"mitoMapVars.as -type=bed9+11 -tab "+mitoMapVarsOutputFile+" /cluster/data/hg38/chrom.sizes "+mitoMapVarsOutputFile.split(".")[0]+".new.bb")
print("Final file created: "+mitoMapVarsOutputFile.split(".")[0]+".new.bb")

######
# BEGIN PROCESSING OF DISEASE FILES
######

# Need -1 to all start positions
# Everything is point mutations - only one of the files needs size calculation

mitoMapRNAmutsFileInputFile = pwd+"mutationsRNA.latest.tsv"
mitoMapDiseaseMutsOutputFile = pwd+"mitoMapDiseaseMuts.bed"
fh = open(mitoMapRNAmutsFileInputFile,"r", encoding="utf-8", errors="replace")
fh2 = open(mitoMapDiseaseMutsOutputFile,"w")

# $ head -2 MutationsRNAMITOMAPFoswiki.tsv
#Position        Locus   Disease Allele  RNA     Homoplasmy      Heteroplasmy    Status  MitoTIP†        GB Freq  FL (CR)*‡      GB Seqs FL (CR)*        References
#576     MT-CR   Hearing loss patient    A576G   noncoding MT-TF precursor       nr      nr      Reported        N/A     0.005%(0.012%)  3 (10)  1

# I want the output to be bed9 for itemRgb and mouseover with 14 additional fields. 
# chrom chromStart chromEnd name score strand thickStart thickEnd itemRgb position locus disease nucleotideChange allele rnaOrAAchange plasmy clinStatus mitoTIP GBfreq GBseqs curatedRefs _mouseOver
for line in fh:
    if line.startswith("Position"):
        continue
    else:
        line = line.rstrip().split("\t")
        position = line[0]
        nucleotideChange = line[3]
        chrom = "chrM"
        chromStart = int(position)-1
        chromEnd = chromStart + 1 #These are all base mutations
        name = nucleotideChange
        score = "0"
        strand = "."
        thickStart = chromStart
        thickEnd = chromEnd
        itemRgb = "153,51,255"
        locus = line[1]
        disease = line[2]
        allele = line[3]
        rnaOrAAchange = line[4] 
        plasmy = line[5] + "/" + line[6]
        clinStatus = line[7]
        mitoTIP = line[8]
        GBfreq = line[9]
        GBseqs = line[10]
        curatedRefs = line[11]
        mutsCodingOrRNA = "MutationsRNA"
        mouseOver = "<b>Allele: </b>"+allele+"<br><b>Position: </b>"+position+"<br><b>Locus: </b>"+locus+"<br>"+\
        "<b>Disease: </b>"+disease+"<br><b>RNA: </b>"+rnaOrAAchange+"<br><b>Plasmy (Hom/Het): </b>"+plasmy+"<br>"+\
        "<b>Status: </b>"+clinStatus+"<br><b>MitoTIP: </b>"+mitoTIP+"<br>"+\
        "<b>GenBank freq FL(CR): </b>"+GBfreq+"<br><b>GenBank seqs FL(CR): </b>"+GBseqs+"<br><b># of curated refs: </b>"+curatedRefs
        
        fh2.write("\t".join(map(str, [chrom, chromStart, chromEnd, name, score, strand, thickStart, thickEnd, itemRgb, \
                                  position, locus, disease, nucleotideChange, allele, rnaOrAAchange, plasmy, clinStatus, \
                                      mitoTIP, GBfreq, GBseqs, curatedRefs, mutsCodingOrRNA, mouseOver])) + "\n")

fh2.close()
fh.close()

mitoMapDiseaseMutsFileInputFile = pwd+"mutationsCodingControl.latest.tsv"
mitoMapDiseaseMutsOutputFile = pwd+"mitoMapDiseaseMuts.bed"
fh = open(mitoMapDiseaseMutsFileInputFile,"r", encoding="utf-8", errors="replace")
fh2 = open(mitoMapDiseaseMutsOutputFile,"a")

# $ head -2 MutationsCodingControlMITOMAPFoswiki.tsv
#Locus   Allele  Position        NucleotideChange        Amino AcidChange        Plasmy Reports(Homo/Hetero)     Disease Status  GB Freq  FL (CR)*‡      GB Seqs FL (CR)*        References
#MT-ATP6 m.8573G>A       8573    G-A     G16D    +/-     Patient with suspected mitochondrial disease    Reported by paper as Benign     0.107%(0.000%)  66 (0)  1

# I want the output to be bed9 for itemRgb and mouseover with 14 additional fields. 
# chrom chromStart chromEnd name score strand thickStart thickEnd itemRgb position locus disease nucleotideChange allele rnaOrAAchange plasmy clinStatus mitoTIP GBfreq GBseqs curatedRefs mutsCodingOrRNA _mouseOver
for line in fh:
    if line.startswith("Locus"):
        continue
    else:
        line = line.rstrip().split("\t")
        position = line[2]
        nucleotideChange = line[3]
        chrom = "chrM"
        chromStart = int(position)-1
        if nucleotideChange == "":
            print("This is an error!")
            chromEnd = chromStart + 1
        else:
            if " " in nucleotideChange: #3 items with 366 G-GAAAG
                print("Look at this entry:")
                print(nucleotideChange)
                if "or" in nucleotideChange:
                    chromEnd = chromStart + len(nucleotideChange.split(" or ")[0].split("-")[0])
                else:
                    chromEnd = chromStart + len(nucleotideChange.split("-")[0].split(" ")[1])
            elif "_" in nucleotideChange: #3 18bp_deletion
                chromEnd = chromStart + len(nucleotideChange.split("bp_")[0])
            else: # ACCTTGC-GCAAGGT
                chromEnd = chromStart + len(nucleotideChange.split("-")[0])        
        name = nucleotideChange
        score = "0"
        strand = "."
        thickStart = chromStart
        thickEnd = chromEnd
        itemRgb = "1,85,135"
        locus = line[0]
        disease = line[6]
        allele = line[1]
        rnaOrAAchange = line[4] 
        plasmy = line[5]
        clinStatus = line[7]
        mitoTIP = ""
        GBfreq = line[8]
        GBseqs = line[9]
        curatedRefs = line[10]
        mutsCodingOrRNA = "MutationsCodingControl"
        mouseOver = "<b>Allele: </b>"+allele+"<br><b>Nucl change: </b>"+nucleotideChange+"<br><b>Position: </b>"+position+"<br><b>Locus: </b>"+locus+"<br>"+\
        "<b>Disease: </b>"+disease+"<br><b>RNA: </b>"+rnaOrAAchange+"<br><b>Plasmy (Hom/Het): </b>"+plasmy+"<br>"+\
        "<b>Status: </b>"+clinStatus+"<br>"+\
        "<b>GenBank freq FL(CR): </b>"+GBfreq+"<br><b>GenBank seqs FL(CR): </b>"+GBseqs+"<br><b># of curated refs: </b>"+curatedRefs
        
        fh2.write("\t".join(map(str, [chrom, chromStart, chromEnd, name, score, strand, thickStart, thickEnd, itemRgb, \
                                  position, locus, disease, nucleotideChange, allele, rnaOrAAchange, plasmy, clinStatus, \
                                      mitoTIP, GBfreq, GBseqs, curatedRefs, mutsCodingOrRNA, mouseOver])) + "\n")
fh2.close()
fh.close()

bash("bedSort "+mitoMapDiseaseMutsOutputFile+" "+mitoMapDiseaseMutsOutputFile)
bash("bedToBigBed -as=/hive/data/genomes/hg38/bed/mitomap/mitoMapDiseaseMuts.as -type=bed9+14 -tab "+mitoMapDiseaseMutsOutputFile+" /cluster/data/hg38/chrom.sizes "+mitoMapDiseaseMutsOutputFile.split(".")[0]+".new.bb")
print("Final file created: "+mitoMapDiseaseMutsOutputFile.split(".")[0]+".new.bb")
