#!/cluster/software/bin/python2.7

import glob, sys, operator, zlib
import gc
from os.path import isfile, join, dirname

# external module to open twoBit files in python: originally installed with 
# "pip install twobitreader"
sys.path.append(join(dirname(__file__), "lib"))
from twobitreader import TwoBitFile # is included with script, should not fail

gc.disable()
# convert CRISPOR off-target files into the format
# guideSeq, chrom;start;score;pam;diffString;annotation|...

def countMm(str1, str2):
    """ count mismatches between two strings
    """
    mismCount = 0
    for c1, c2 in zip(str1, str2):
        if c1!=c2:
            mismCount += 1
    return mismCount

def showMism(str1, str2):
    """ return a string with just the mismatching nucleotides, '.' for matching ones 
    also return number of mismatches
    """
    assert(len(str1)==len(str2))
    res = []
    mismCount = 0
    for c1, c2 in zip(str1, str2):
        if c1==c2:
            res.append(".")
        else:
            res.append(c2)
            mismCount += 1
    return "".join(res), mismCount

def revComp(seq):
    table = { "a":"t", "A":"T", "t" :"a", "T":"A", "c":"g", "C":"G", "g":"c", "G":"C", "N":"N", "n":"n", 
            "Y":"R", "R" : "Y", "M" : "K", "K" : "M", "W":"W", "S":"S",
            "H":"D", "B":"V", "V":"B", "D":"H", "y":"r", "r":"y","m":"k",
            "k":"m","w":"w","s":"s","h":"d","b":"v","d":"h","v":"b","y":"r","r":"y" }
    newseq = []
    for nucl in reversed(seq):
       newseq += table[nucl]
    return "".join(newseq)

def writeGuideRow(db, guideSeq, otRows, ofh):
    " write a guide row, with all off-targets merged into a single field "
    otRows.sort(key=operator.itemgetter(2), reverse=True)
    filtOtRows = []
    mismCounts = [0]*5
    for row in otRows:
        # format of row is: chrom;start;score;pam;diffString;annotation
        otSeq = row[4]
        #mismString, mismCount = showMism(guideSeq, row[4][:20])
        #row[4] = compressAln(mismString)
        mismCount = countMm(guideSeq, row[5][:20])
        if mismCount <= 4: # very very rare >4: only when there are Ns in the off-target seq
            mismCounts[mismCount] += 1
        #if mismCount >= 4:
            #continue # just show count, don't store locations of off-targets with 4 mismatches
        #row[2] = "%0.2f" % row[2]
        chrom, start, strand, score, pam, diffString, annot = row
        start = int(start)-1 # aargh!! crispor is 1-based!
        row = [chrom, start, strand, score, otSeq]

        #otStrings.append(";".join(row))
        filtOtRows.append(row)
        
    # need to determine strand. idiotic bug: old version of crispor didn't give me the strand.

    # get seqs, but in chrom order to get better speed
    #otCoords = []
    #for row in filtOtRows:
        #chrom, start, score, otSeq = row
        #otCoords.append( (chrom, start, otSeq) )
    #otCoords.sort()

    # write to bed
    #tmpFh = tempfile.NamedTemporaryFile(dir="/dev/shm", prefix="max-crisprTrack")
    #for r in bedRows:
        #r = [str(x) for x in r]
        #tmpFh.write("%s\n" % ("\t".join(r)))
    #tmpFh.flush()

    #twoBitFname = '/scratch/data/%s/%s.2bit' % (db, db)
    #if not isfile(twoBitFname): # can happen these days, says Hiram
        #twoBitFname = '/gbdb/%s/%s.2bit' % (db, db)
    #if not isfile(twoBitFname): # can happen these days, says Hiram
        #twoBitFname = '/cluster/data/%s/%s.2bit' % (db, db)
    #genome = TwoBitFile(twoBitFname)

    # get sequences
    #strands = {}
    #for otRow in otCoords:
    #    chrom, start, otSeq = otRow
    #    twoBitChrom = genome[chrom]
    #    forwSeq = twoBitChrom[start:start+23].upper()
    #    # two possible sequences, depending on strand
    #    revSeq = revComp(forwSeq).upper()

    #    #print guideSeq, otSeq, forwSeq, revSeq
    #    # for palindromes, we can't decide, default to +
    #    if otSeq==forwSeq:
    #        strand = "+"
    #    elif otSeq==revSeq:
    #        strand = "-"
    #    else:
    #        assert(False)
    #    strands[(chrom, start)] = strand

    # now add the strand to the features
    otStrings = []
    for row in filtOtRows:
        chrom, start, strand, score, mismCount = row
        scoreStr = str(int(score*1000))
        #if scoreStr[:2]=="0.":
            #scoreStr = scoreStr[1:]
        row = (chrom, str(start)+strand,scoreStr)
        otStrings.append(";".join(row))

    mismCounts = [str(x) for x in mismCounts]
    otField = "|".join(otStrings)
    # mysql has trouble with very long blobs, and we also can save a lot of space by 
    # ignoring too repetitive sequences
    if len(otField) > 5000:
        otField = ""
    row = (guideSeq, ",".join(mismCounts), otField)
    ofh.write("\t".join(row))
    ofh.write("\n")

def compressAln(inStr):
    """ replace consecutive dots with count of dots 
    >>> compressAln("...AC...T...")
    '3AC3T3'
    """
    outStr = []
    stretchLen = -1
    for i, c in enumerate(inStr):
        if c!=".":
            if stretchLen!=-1:
                outStr.append(str(stretchLen))
            start = i
            outStr.append(c)
            stretchLen = -1
        else:
            if stretchLen==-1:
                stretchLen=0
            stretchLen+=1
    if stretchLen!=-1:
        outStr.append(str(stretchLen))
    return "".join(outStr)

def main():
    db, inFnamePath, outFname = sys.argv[1:]

    fnames = open(inFnamePath).read().splitlines()

    ofh = open(outFname, "wb")

    lastGuideSeq = None
    otRows = []
    doneGuides = {}
    for i, fname in enumerate(fnames):
        # 21forw    ATATTGCTCCATTGGCCACGAGG AAATAGGTCCATTTGCCACGTGG 4   0.152972547468  0.0530920060567 chrV    16374724    16374746    exon:srv-34

        #if i==1:
            #print "XXXXX HACK"
            #break

        print "%s: %d of %d input files done" % (fname, i, len(fnames))
        for line in open(fname, "rb"):
            if line.startswith("guideId") or line.startswith("seqId"):
                continue
            _, _, guideSeq, otSeq, _, mismCount, mitScore, cfdScore, chrom, start, end, strand, annot = line.rstrip("\n").split("\t")

            guideSeq = guideSeq[:20]
            if cfdScore=="None":
                # this is very rare, sometimes the CFD is not defined ("N" nucl codes?)
                continue
            else:
                cfdScore = float(cfdScore)
            if cfdScore < 0.023:
                continue

            if guideSeq != lastGuideSeq and not lastGuideSeq==None:
                # for some reason, a handful of guides can appear twice
                # in the files. Just skip these. Will need to remove 
                # them later, too
                if guideSeq in doneGuides:
                    print "Found guide %s twice: " % guideSeq
                    print doneGuides[guideSeq]
                    print fname
                else:
                    writeGuideRow(db, lastGuideSeq, otRows, ofh)

                otRows = []
                doneGuides[guideSeq] = fname

            lastGuideSeq = guideSeq
                
            pam = otSeq[-3:]
            annot = annot.replace(";", ",") # paranoia
            # save 13% of bytes
            annot = annot.replace("intergenic:", "g:")
            annot = annot.replace("intron:", "i:")
            annot = annot.replace("exon:", "e:")
            otRow = [chrom, start, strand, cfdScore,pam, otSeq, annot]
            otRows.append(otRow)
            
    if len(otRows)!=0:
        writeGuideRow(db, lastGuideSeq, otRows, ofh)

    print "wrote %s" % ofh.name

if __name__ == "__main__":
    main()
