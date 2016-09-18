# cluster job script:
# given a db name and bed file with 100mers, calculate the eff scores and write to a tab file
# arguments: crisporLibDir, db, inFname, outFname

import sys, os, logging
import tempfile

sys.path.append(sys.argv[1])
import crisporEffScores # if this fails, the crisporEffScores.py could not be found.

logging.basicConfig(level=logging.DEBUG)

def parseFastaAsList(fileObj):
    " parse a fasta file, return list (id, seq) "
    seqs = []
    parts = []
    seqId = None
    for line in fileObj:
        line = line.rstrip("\n")
        if line.startswith(">"):
            if seqId!=None:
                seqs.append( (seqId, "".join(parts)) )
            seqId = line.lstrip(">")
            parts = []
        else:
            parts.append(line)
    if len(parts)!=0:
        seqs.append( (seqId, "".join(parts)) )
    return seqs

def main():
    crisporDir, db, inFname, outFname = sys.argv[1:]

    tempFaFile = tempfile.NamedTemporaryFile()
    cmd = "twoBitToFa /hive/data/genomes/%s/%s.2bit -bed=%s %s" % (db, db, inFname, tempFaFile.name)
    print "running %s" % cmd
    assert(os.system(cmd)==0)

    print "parsing fasta file %s" % tempFaFile.name
    seqs = parseFastaAsList(tempFaFile)
    justSeqs = [s[1] for s in seqs]

    # wuCrispr is for some reason really really slow on the cluster. removing it.
    print "starting eff score calculations with %d sequences" % len(seqs)
    effScores = crisporEffScores.calcAllScores(justSeqs, skipScores=["wuCrispr"])

    ofh = open(outFname, "w")
    headers = ["guideId"]
    headers.extend(effScores.keys())
    ofh.write("\t".join(headers))
    ofh.write("\n")
    scoreNames = effScores.keys()

    for i in range(len(seqs)):
        seqId, seq = seqs[i]
        row = [seqId]
        for scoreName in scoreNames:
            #print scoreName, effScores[scoreName]
            row.append(str(effScores[scoreName][i]))
        ofh.write("\t".join(row))
        ofh.write("\n")
    print "wrote %s" % ofh.name

main()
