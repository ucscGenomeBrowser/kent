# merge together the specScores.tab file with the guide.bed file and 
# the effScores.tab file and create crispor.bb from the result
import os, sys, glob
from os.path import dirname, join

# arguments:
# arg1: db = genome db
# arg2: inFname = bed file with all guides in genome, e.g. allGuides.bed
# arg3: specScoreMask = glob mask or file name of specificity score files
# arg4: effScoreFname = file name with efficiency scores

stopOnMissingEffScore = False

# three arguments:
# arg1: input bed file with all guides
# arg2: specificity scores, one per target guide sequence
# arg3: efficiency scores, one per genome position

# guides that have no specificity score are assumed to be non-unique

# names and order of scores to export to bigbed file
# the .AS file has to match these!!
# the first two MUST BE fusi and crisprScan!
# the code handles them in a special way.
scoreNames = ["fusi", "crisprScan", "doench", "oof"]

# don't put these score fields into the final .bb file
# mh = microhomology - has no use
# drsc = same as housden score
skipScores = ["mh", "guideId", "chariRaw", "wang", "chariRank", "ssc", "housden", "wuCrispr", "finalGc6", "finalGg"]

# at the moment, we only handle the three-bp PAM NGG
PAMLEN = 3

def parseRanks(fusiPercFname):
    " parse tab sep file with two columsn and return dict col1 -> col2 "
    ret = {}
    for line in open(fusiPercFname):
        row = line.rstrip("\n").split("\t")
        key, val = row
        key = int(key)
        val = int(val)
        ret[key] = val
    return ret

def rewriteBed(inFname, specScores, effScores, otOffsetPath, dataDir):
    """
    spec score: dict of guideSeq -> spec score
    effScores: dict of guideId -> tuple with all scores
    """
    # read table with fusi score -> percentile
    fusiPercFname = join(dataDir, "fusiRanks.tab")
    fusiToPerc = parseRanks(fusiPercFname)

    # read table with moreno mateos score -> percentile
    morenoPercFname = join(dataDir, "crisprScanRanks.tab")
    morenoToPercent = parseRanks(morenoPercFname)

    print ("Reading %s" % otOffsetPath)
    otOffsets = {}
    for line in open(otOffsetPath, "rb"):
        ot, offset = line.rstrip("\n").split("\t")
        otOffsets[ot] = offset
    print ("Read %d offsets" % len(otOffsets))

    print "reading %s, writing to crispr.bed" % inFname
    donePos = set()
    ofh = open("crispr.bed", "w")
    for line in open(inFname):
        chrom, start, end, nameSeqPam, _, strand = line.rstrip("\n").split("\t")

        # make sure to remove duplicate lines from guides.bed
        idx = chrom+start+strand
        if idx in donePos:
            continue
        donePos.add(idx)

        name, seq, pam = nameSeqPam.split("_")
        specScore = int(specScores.get(seq, -1))

        scoreDesc = str(specScore)
        if "N" in seq:
            scoreDesc = "This guide sequence includes an N character and could therefore not be processed"
        elif specScore in [-1, -2]:
            if specScore==-1:
                scoreDesc = "This guide sequence is not unique in the genome. The specificity scores were not determined."
            elif specScore==-2:
                #scoreDesc = "This guide includes N character(s). Specificity scores could not be determined."
                scoreDesc = "This guide has too many potential off-targets. The specificity score could not be calculated."

        # get eff scores
        guideId = "%s:%s-%s:%s" % (chrom, start, end, strand)
        guideEffScores = effScores.get(guideId, None)
        if guideEffScores is None:
            if stopOnMissingEffScore:
                assert(False)
            fusi, moreno, doench, oof = "ND","ND","ND","ND"
        else:
            fusi, moreno, doench, oof = guideEffScores
            fusi = int(fusi)
            moreno = int(moreno)

        fusiPerc = fusiToPerc.get(fusi, None)
        if fusiPerc is not None:
            fusiHover = str(fusiPerc)+"%"
            fusi = "%d%% (%d)" % (fusiPerc, fusi)
        else:
            fusiHover = "%s (raw)" % str(fusi)
            fusi = "percentile unknown (%s)" % (str(fusi))

        morenoPerc = morenoToPercent.get(moreno, None)
        if morenoPerc is not None:
            morenoHover = str(morenoPerc)+"%"
            moreno = "%d%% (%d)" % (morenoPerc, moreno)
        else:
            morenoHover = "%s (raw)" % str(moreno)
            moreno = "percentile unknown (%s)" % (str(moreno))

        # not unique -> light grey
        if specScore == -1:
            color = "150,150,150"
            altColor = "150,150,150"
        # too many off-targets -> darker grey
        elif specScore == -2:
            color = "120,120,120"
            altColor = "120,120,120"
        # spec score low -> darkest grey
        elif specScore < 50:
            color = "80,80,80"
            altColor = "80,80,80"
        else:
            # color primarily by fusi score
            if fusiPerc is not None:
                if fusiPerc > 55:
                    color =  "0,200,0"
                elif fusiPerc > 30:
                    color =  "255,255,0"
                else:
                    color = "255,100,100"
            else:
                color = "0,0,100"

            # alternative color by moreno score
            if morenoPerc is not None:
                if morenoPerc > 60:
                    altColor =  "0,200,0"
                elif morenoPerc > 30:
                    altColor =  "255,255,0"
                else:
                    altColor = "255,100,100"
            else:
                altColor = "0,0,100"
        
        if specScore > 70:
            specColor = "0,255,0"
        elif specScore > 50:
            specColor = "128,128,0"
        else:
            specColor = "255,0,0"

        score = str(max(0, specScore)) # bigBed does not allow neg. score values

        offset = otOffsets.get(seq, "0")

        # extend features as suggested by Chris Lee in ticket
        if strand=="+":
            thickStart = start
            thickEnd = end
            end = str(int(end) + PAMLEN)
        else:
            thickStart = start
            thickEnd = end
            start = str(int(start) - PAMLEN)

        row = [chrom, start, end, "", score, strand, thickStart, thickEnd,
                color, altColor, specColor, seq, pam, scoreDesc]

        row.extend([fusi, moreno, doench, oof])

        if specScore>=0:
            mouseOver = "MIT Spec. Score: %d, Doench 2016: %s, Moreno-Mateos: %s" % \
                (specScore, fusiHover, morenoHover)
        else:
            mouseOver = "Sequence is not unique in genome"
        row.append(mouseOver)
        row.append(str(offset))

        ofh.write("\t".join(row))
        ofh.write("\n")
    ofh.close()

def readEffScores(masks):
    """ given a list of input file specs, parse eff scores and return as dict 
    chr:start-end:strand -> list of scores (same order as the scoreNames global var)
    """
    print "reading eff scores"
    print "Make sure that the crispr.as file has entries for these fields: %s" % scoreNames
    effScores = dict()
    for mask in masks:
        for fname in glob.glob(mask):
            headers = open(fname).readline().strip().split('\t')

            excessFields = set(headers) - set(skipScores) - set(scoreNames)
            if len(excessFields)!=0:
                raise Exception("fields %s not in scoreNames nor skipScores" % (str(excessFields)))

            overdefFields = set(skipScores).union(set(scoreNames)) - set(headers)
            if len(overdefFields)!=0:
                raise Exception("fields %s are defined in script but are not in .tab file " % (str(overdefFields)))
            # get the index of columns that we need
            removeHeaders = [headers.index(x) for x in skipScores]
            scoreColumns = [headers.index(x) for x in scoreNames]
            scoreColumns = [x for x in scoreColumns if x not in removeHeaders]

            print "parsing %s" % fname
            headerDone = False
            for line in open(fname):
                fs = line.rstrip("\n").split()
                guideId = fs[0]

                scores = [fs[i] for i in scoreColumns]
                scores = tuple(scores)
                effScores[guideId] = scores
    return effScores

# format of specScores.tab, I'm not using headers to save time
guideSeqField = 0
specScoreField = 1

def readSpecScores(inMask):
    " read specificity scores and return as 20mer -> score (int) "
    print "reading spec scores"
    specScores = dict()
    for fname in glob.glob(inMask):
        print "parsing %s" % fname
        for line in open(fname):
            if line=="\n" or line.startswith("targetSeq"):
                continue
            fs = line.rstrip("\n").split()
            guideSeq = fs[guideSeqField]
            specScore = fs[specScoreField]

            if specScore=="None": # overly repeated sequence?
                specScore = -2

            specScores[guideSeq[:20]] = int(specScore)
    return specScores

def main():
    args = sys.argv[1:]
    scriptDir = dirname(__file__) # directory where this script is located

    db = args[0]
    inFname = args[1]
    specScoreMask = args[2]
    effScoreFname = args[3]
    offtargetOffsets = args[4]

    effScores = readEffScores([effScoreFname])

    #specScores = readSpecScores("specScores.*.tab")
    specScores = readSpecScores(specScoreMask)

    rewriteBed(inFname, specScores, effScores, offtargetOffsets, scriptDir)

    print "sorting bed"
    cmd = "bedSort crispr.bed crispr.bed"
    assert(os.system(cmd)==0)

    print "converting to bigBed"
    cmd = "bedToBigBed -tab -type=bed9+ -as=/hive/groups/browser/crisprTrack/crispr.as crispr.bed /cluster/data/genomes/%s/chrom.sizes crispr.bb" % db
    assert(os.system(cmd)==0)

main()
