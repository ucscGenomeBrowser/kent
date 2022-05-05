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

    #for line in gzip.open("whole_genome_SNVs.tsv.gz"):
    currChrom = None
    chromData = {"A":{}, "C":{}, "T":{}, "G":{}}
    isHg19 = (db=="hg19")
    for line in subprocess.Popen(['zcat', "hmc_wst_uniquevar_outputforshare_v2.txt.gz"], stdout=subprocess.PIPE, encoding="ascii").stdout:
        #print(line)
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
            #print("change of chrom: old %s, new %s" % (currChrom, chrom))
            yield currChrom, chromData
            chromData = {"A":{}, "C":{}, "T":{}, "G":{}}
            currChrom = chrom

            # XX
            #chrom = None
            #break

        if isHg19:
            pos = int(row[1])
        else:
            pos = row[2]
            if pos =='':
                continue
            pos = int(pos)

        score = float(row[-1])

        maxScore = score
        if pos in chromData:
            maxScore = max(chromData[pos], score)
            print("duplicate value at position, using max: ", pos, score, maxScore)

        #print("adding %f at %d in %s" % (maxScore, pos, chrom))
        chromData[alt][pos] = maxScore
    
    if chrom is not None:
        yield chrom, chromData

#def iterStretches(nuclData):
def main():
    #j." yield chrom, pos, nuclValues from nuclData which is "
    #j.pos = 
    outFhs = {
        "A" : open("a.wig", "w"),
        "C" : open("c.wig", "w"),
        "T" : open("t.wig", "w"),
        "G" : open("g.wig", "w")
     }

    db = sys.argv[1]

    for chrom, chromData in readPerChrom(db):
        #print("read %s" % chrom)
        for nucl, nuclData in chromData.items():
            #print("nucleotide: %s" % (nucl))
            #print("to array and sort %s" % chrom)
            nuclData = list(nuclData.items())
            nuclData.sort()

            ofh = outFhs[nucl]
            #print("write %s" % chrom)
            for pos, val in nuclData:
                ofh.write("%s\t%d\t%d\t%f\n" % (chrom, pos-1, pos, val))

    print("wrote wigs")
main()
