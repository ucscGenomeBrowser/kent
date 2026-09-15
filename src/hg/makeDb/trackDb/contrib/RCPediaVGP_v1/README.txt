
Track construction:  DONE - Hiram - 2026-09-01

mkdir /hive/data/outside/genark/RCPediaVGP_v1
cd /hive/data/outside/genark/RCPediaVGP_v1

# verify the source track hub
time (hubCheck -verbose=2 \
  'https://genome.ucsc.edu/hubspace/0d/rmercuri/RCPVGP_v0.3/hub.txt') \
     > check.log 2>&1

real    0m12.107s
user    0m8.830s
sys     0m1.093s

# nothing unusual seen in check.log

# fetch the original hub:

time hubClone -download \
   'https://genome.ucsc.edu/hubspace/0d/rmercuri/RCPVGP_v0.3/hub.txt'

real    1m3.135s
user    0m30.869s
sys     0m8.490s

# local data size:

du -hsc RCPediaVGP_v1
80M     RCPediaVGP_v1

# All of the trackDb.txt files can be the same.  Take one of them
#   and edit it down to have generic names instead of everything specific

cp -p RCPediaVGP_v1/GCF_028885655.1/trackDb.GCF_028885655.2.txt \
   ./RCPediaVGP_v1.trackDb.txt

# edit that trackDb.txt to read:

track RCPediaVGP_v1
shortLabel RCPedia VGP
longLabel Retrocopies Encyclopedia - VGP v1 assemblies September 2026
type bigGenePred
bigDataUrl contrib/RCPediaVGP_v1/retrocopies.bb
html contrib/RCPediaVGP_v1/retrocopies.html
group genes
visibility dense

# and check it into the source tree
#   ~/kent/src/hg/makeDb/trackDb/contrib/RCPediaVGP_v1/RCPediaVGP_v1.trackDb.txt

# Same procedure for the html documentation page, copy one:

#  RCPediaVGP_v1/GCF_028885655.1/myTrackDocs.html

# into the source tree and edit to conform to UCSC style in:
#   ~/kent/src/hg/makeDb/trackDb/contrib/RCPediaVGP_v1/retrocopies.html

# the mkSymLinks.sh will transform the user's unique file names into
#    simple generic names the same for every track.
#  ~/kent/src/hg/makeDb/trackDb/contrib/RCPediaVGP_v1/mkSymLinks.sh
