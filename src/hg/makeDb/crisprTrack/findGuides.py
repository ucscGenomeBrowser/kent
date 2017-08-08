# find all possible NGG 20bp guides in a file of exon sequences that were extended by some bp
# on each side

# first parameter: name of fasta file produced by twoBitToFa with the -bed option
# 2nd parameter: chrom.sizes file
# 3nd parameter: guide positions in bed format
# 4rd parameter: guide sequences in fasta format

# This job script has been ripped out of the crispor.py source code

import sys, logging
#sys.path.append("..")
#from annotateOffs import *

compTable = { "a":"t", "A":"T", "t" :"a", "T":"A", "c":"g", "C":"G", "g":"c", \
        "G":"C", "N":"N", "n":"n",
        "Y":"R", "R" : "Y", "M" : "K", "K" : "M", "W":"W", "S":"S",
        "H":"D", "B":"V", "V":"B", "D":"H", "y":"r", "r":"y","m":"k",
        "k":"m","w":"w","s":"s","h":"d","b":"v","d":"h","v":"b","y":"r","r":"y" }

def parseChromSizes(fname):
    ret = {}
    for line in open(fname):
        chrom, size = line.rstrip("\n").split()[:2]
        ret[chrom] = int(size)
    return ret

def revComp(seq):
    newseq = []
    for nucl in reversed(seq):
       newseq += compTable[nucl]
    return "".join(newseq)

def findPat(seq, pat):
    """ yield positions where pat matches seq, stupid brute force search 
    """
    for i in range(0, len(seq)-len(pat)+1):
        #print "new pos", i, seq[i:i+len(pat)],"<br>"
        found = True
        for x in range(0, len(pat)):
            #print "new step", x, "<br>"
            if pat[x]=="N":
                #print "N","<br>"
                continue
            seqPos = i+x
            if seqPos == len(seq):
                found = False
                break
            if not matchNuc(pat[x], seq[seqPos]):
                #print i, x, pat[x], seq[seqPos], "no match<br>"
                found = False
                break
            #print "match", i, x, found, "<br>"
        if found:
            #print "yielding", i, "<br>"
            yield i

def findPams(seq, pam, strand, startDict, endSet):
    """ return two values: dict with pos -> strand of PAM and set of end positions of PAMs
    Makes sure to return only values with at least 20 bp left (if strand "+") or to the
    right of the match (if strand "-")
    >>> findPams("GGGGGGGGGGGGGGGGGGGGGGG", "NGG", "+", {}, set())
    ({20: '+'}, set([23]))
    >>> findPams("CCAGCCCCCCCCCCCCCCCCCCC", "CCA", "-", {}, set())
    ({0: '-'}, set([3]))

    """

    # -------------------
    #          OKOKOKOKOK
    minPosPlus  = 20
    # -------------------
    # OKOKOKOKOK
    maxPosMinus = len(seq)-(20+len(pam))

    #print "new search", seq, pam, "<br>"
    for start in findPat(seq, pam):
        # need enough flanking seq on one side
        #print "found", start,"<br>"
        if strand=="+" and start<minPosPlus:
            #print "no, out of bounds +", "<br>"
            continue
        if strand=="-" and start>maxPosMinus:
            #print "no, out of bounds, -<br>"
            continue

        #print "match", strand, start, end, "<br>"
        startDict[start] = strand
        end = start+len(pam)
        endSet.add(end)
    return startDict, endSet

def matchNuc(pat, nuc):
    " returns true if pat (single char) matches nuc (single char) "
    if pat in ["A", "C", "T", "G"] and pat==nuc:
        return True
    elif pat=="M" and nuc in ["A", "C"]:
        return True
    elif pat=="K" and nuc in ["T", "G"]:
        return True
    elif pat=="R" and nuc in ["A", "G"]:
        return True
    elif pat=="Y" and nuc in ["C", "T"]:
        return True
    else:
        return False

def findAllPams(seq, pam):
    """ find all matches for PAM and return as dict startPos -> strand and a set
    of end positions
    >>> findAllPams("GGGGGGGGGGGGGGGGGGGGGGG", "NGG")
    """
    startDict, endSet = findPams(seq, pam, "+", {}, set())
    startDict, endSet = findPams(seq, revComp(pam), "-", startDict, endSet)
    return startDict, endSet

def getFlankSeq(seq, startPos, strand, pamLen):
    """ given a startPos and a pamLen
    return the guide sequence
    based on flankSeqIter in crispor.cgi
    """
    if strand=="+":
        flankSeq = seq[startPos-20:startPos]
        pamSeq = seq[startPos:startPos+pamLen]
    else: # strand is minus
        flankSeq = revComp(seq[startPos+pamLen:startPos+pamLen+20])
        pamSeq = revComp(seq[startPos:startPos+pamLen])

    return flankSeq, pamSeq

def iterFastaSeqs(fileObj):
    " parse a fasta file, yield (id, seq) "
    parts = []
    seqId = None
    for line in fileObj:
        line = line.rstrip("\n")
        if line.startswith(">"):
            if seqId!=None:
                yield (seqId, "".join(parts))
            seqId = line.lstrip(">")
            parts = []
        else:
            parts.append(line)
    if len(parts)!=0:
        yield (seqId, "".join(parts))

def main():
    fname       = sys.argv[1]
    chromSizesPath  = sys.argv[2]
    outBedFname = sys.argv[3]
    faFname     = sys.argv[4]

    logging.warn("input fasta: %s, chromSizes %s, outBed %s, output fasta %s" % (fname, chromSizesPath, outBedFname, faFname))
    chromSizes = parseChromSizes(chromSizesPath)

    ofh   = open(outBedFname, "w")
    faOfh = open(faFname, "w")

    guideCount = 0

    doneGuides = {}

    for seqId, seq in iterFastaSeqs(open(fname)):
        if len(seq)<23:
            continue
        seq = seq.upper()

        startDict, _ = findAllPams(seq, "NGG")

        # seqId looks like 'chr1:12234-123456'
        chrom, startEnd = seqId.split(":")
        seqStart, seqEnd = startEnd.split("-")
        seqStart = int(seqStart)
        seqEnd = int(seqEnd)

        for pamStart, strand in startDict.iteritems():

            guideSeq, pamSeq = getFlankSeq(seq, pamStart, strand, 3)

            if guideSeq not in doneGuides:
                guideCount += 1
                guideId = guideCount
                doneGuides[guideSeq] = guideId
                faOfh.write(">guide%d\n" % guideId)
                faOfh.write(guideSeq+"\n")
            else:
                guideId = doneGuides[guideSeq]

            if strand=="+":
                gStart = seqStart+pamStart-20
            else:
                gStart = seqStart+pamStart+3

            gEnd = gStart+20

            bedRow = [chrom, gStart, gEnd, "guide"+str(guideId)+"_"+guideSeq+"_"+pamSeq, "0", strand]
            if max(gStart, gEnd) > chromSizes[chrom] or min(gStart,gEnd) < 0:
                print "Skipping %s, outside of chrom size" % str(bedRow)
                continue
            ofh.write("\t".join([str(x) for x in bedRow]))
            ofh.write("\n")

if __name__=="__main__":
    main()
