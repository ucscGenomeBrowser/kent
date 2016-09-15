# create a joblist file for parasol
# it runs many little fasta files in specScores/jobFiles/ through crispor and writes the results to
# out/guides and out/offs

# 1st argument: filename with paths of input fasta files
# 2nd argument: name of path with a git clone of the CRISPOR script
# 3rd argument: genome db
# 4th argument: name of output joblist file

import glob, sys
filenameFile, crisporPath, db, joblistPath = sys.argv[1:]

ofh = open(joblistPath, "w")

print "reading jobNames"
fnames = open(filenameFile).read().splitlines()

print "creating jobList"
#for fname in glob.glob(inPath+"/*.fa"):
for fname in fnames:
    fname = fname.replace("specScores/","")
    baseName = fname.split("/")[-1].split(".")[0]
    cmd = "/cluster/software/bin/python %s/crispor.py %s {check in exists %s} {check out exists jobs/outGuides/%s.tab} -o jobs/outOffs/%s.tab" % (crisporPath, db, fname, baseName, baseName)
    ofh.write(cmd+"\n")

print "wrote new file %s" % joblistPath
