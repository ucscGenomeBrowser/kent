# split input file with 23bp guide target sequence, remove guides that appear in the file twice
# or that were already done (e.g. in specScores.old.tab)

# arg1: file with all guides, one line per guide sequence
# arg2: output dir
# arg3: file with names of output files that were created

# will not re-calculate guides that are found in specScores*.tab files

import sys, os
import glob
from collections import defaultdict

fname = sys.argv[1]
outDir = sys.argv[2]
outFnamePath = sys.argv[3]

cmd = "rm -rf %s" % outDir
assert(os.system(cmd)==0)

os.mkdir(outDir)

doneGuides = set()
#for fname in glob.glob("specScores*.tab"):
    #print "Parsing %s" % fname
    #for line in open(fname):
        #guide = line.split()[0][:20]
        #assert(len(guide)==20)
        #doneGuides.add(guide)


print "reading guides from %s" % fname
allGuides = set()
guideCounts = defaultdict(int)
for line in open(fname):
    guide = line.strip().split()[0]
    assert(len(guide)==20)
    allGuides.add(guide)
    guideCounts[guide[:20]]+=1

outFnameFh = open(outFnamePath, "w")

print "writing to %s/" % outDir
#gcCount =0
#repCount = 0
nonUniqueCount = 0
chunkId = 0
doneCount = 0
i = 0
chunk = []
chunkSize = 70
for guide in allGuides:
    assert(len(guide)==20)
    if guideCounts[guide[:20]] != 1:
        nonUniqueCount += 1
        continue
    #if guide[:20].count("C") + guide[:20].count("G") >= 15:
    #    gcCount += 1
    #    continue
    # remove repeats
    #if len(set(guide).intersection("actg"))!=0:
        #repCount += 1
        #continue
    if guide in doneGuides:
        doneCount += 1
        continue
    
    chunk.append(guide+"AGG")
    i += 1

    if len(chunk)==chunkSize:
        newFname = outDir+"/%05d.fa" % chunkId
        outFnameFh.write("%s\n" % newFname)
        ofh = open(newFname, "w")
        print "Writing %s" % newFname
        ofh.write(">chunk%d\n" % chunkId)
        ofh.write("NNNNNNN".join(chunk))
        ofh.write("\n")
        ofh.close()
        chunk = []
        chunkId += 1

# write last chunk
newFname = outDir+"/%05d.fa" % chunkId
outFnameFh.write("%s\n" % newFname)
ofh = open(newFname, "w")
ofh.write(">chunk%d\n" % chunkId)
ofh.write("NNNNNNN".join(chunk))

print i, "sequences written"
#print gcCount, "sequences removed due to high GC"
#print repCount, "sequences removed due to repeats"
print nonUniqueCount, "sequences removed as they are non-unique"
print doneCount, "sequences removed as they were already done before"
