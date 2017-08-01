# extend the guides in guides.bed -30 / + 50 and split over cluster for eff score calculations
# also creates/deletes the output directory 

# argument 1: chrom.sizes file
# argument 2: the list of all guides, BED file
# argument 3: the output directory
# argument 4: file with filenames created
import os, sys

def parseChromSizes(fname):
    ret = {}
    for line in open(fname):
        chrom, size = line.rstrip("\n").split()[:2]
        ret[chrom] = int(size)
    return ret

def main():
    #outDir = "effScores"
    chromSizesFname = sys.argv[1]
    guideFname = sys.argv[2]
    outDir = sys.argv[3]
    fnameFname = sys.argv[4]

    chromSizes = parseChromSizes(chromSizesFname)

    print "cleaning %s" % outDir
    cmd = "rm -rf %s" % outDir
    print "running %s" % cmd
    assert(os.system(cmd)==0)

    cmd = "mkdir -p %s/bed %s/out" % (outDir, outDir)
    assert(os.system(cmd)==0)

    fnameFh = open(fnameFname, "w")

    chunkSize = 10000
    chunk = []
    chunkIdx = 0
    print "one dot is %d guides = one parasol job" % chunkSize
    for line in open(guideFname):
        fs = line.rstrip("\n").split("\t")
        # chr11   89059955        89059975        guide1_TTGTCCCATATGAGTTGTTC_TGG 0       +
        chrom, start, end, name, score, strand = fs
        if "N" in name:
            continue
        newName = "%s:%s-%s:%s" % (chrom, start, end, strand)
        start, end = int(start), int(end)
        if strand=="+":
            start -= 30
            end += 50
        else:
            start -= 50
            end += 30
        # just ignore guides that are too close to chrom end
        if start < 0 or end > chromSizes[chrom]:
            continue
        row = [chrom, str(start), str(end), newName, score, strand]
        chunk.append(row)
        if len(chunk)>chunkSize:
            ofh = open(outDir+"/bed/%05d.bed" % chunkIdx, "w")
            for row in chunk:
                ofh.write("\t".join(row))
                ofh.write("\n")
            ofh.close()
            #print "wrote %s" % ofh.name
            print ".",
            sys.stdout.flush()
            fnameFh.write("%s\n" % ofh.name)
            chunk = []
            chunkIdx += 1

    if len(chunk)!=0:
        ofh = open(outDir+"/bed/%05d.bed" % chunkIdx, "w")
        for row in chunk:
            ofh.write("\t".join(row))
            ofh.write("\n")
        fnameFh.write("%s\n" % ofh.name)
        ofh.close()

main()
