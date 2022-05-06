import subprocess
import sys

def readPerChrom(db):
    " yield all values at consecutive positions as a tuple (chrom, pos, list of (nucl, phred value)) "
    # chr     hg19_pos        hg38_pos        ref     alt     pfam_id NP_id   gene_symbol     HGVSc   HGVSp   hmc_score
    values = []
    firstChrom = None
    firstPos = None
    lastChrom = None
    lastPos = None

    currChrom = None
    chromData = {}
    isHg19 = (db=="hg19")
    for line in subprocess.Popen(['zcat', "hmc_wst_uniquevar_outputforshare_v2.txt.gz"], stdout=subprocess.PIPE, encoding="ascii").stdout:
        if line.startswith("chr\t"):
            continue
        row = line.rstrip("\n").split("\t")
        # chr     hg19_pos        hg38_pos        ref     alt     pfam_id NP_id   gene_symbol     HGVSc   HGVSp   hmc_score
        #chrom, hg19Pos, hg38Pos, ref, alt, pfamId, npId, sym, hgvsc, hgvsp, score = row
        chrom = row[0]
        alt = row[4]
        if currChrom is None:
            currChrom = chrom

        if chrom != currChrom:
            yield currChrom, chromData
            chromData = {}
            currChrom = chrom

        if isHg19:
            pos = int(row[1])
        else:
            pos = row[2]
            if pos =='':
                continue
            pos = int(pos)

        score = float(row[-1])
        maxScore = score

        # this is only necessary for hg38 - lifting leads to duplicates
        if pos in chromData:
            maxScore = max(chromData[pos], score)
            if maxScore != score:
                print("duplicate value at position, using max: ", pos, score, maxScore)

        chromData[pos] = maxScore
    
    if chrom is not None:
        yield chrom, chromData

def main():
    ofh = open("hmc.wig", "w")

    db = sys.argv[1]

    for chrom, chromData in readPerChrom(db):
        for pos, val in chromData.items():
            ofh.write("%s\t%d\t%d\t%f\n" % (chrom, pos-1, pos, val))

    print("wrote hmc.wig")
main()
