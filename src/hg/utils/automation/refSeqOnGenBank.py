#!/usr/bin/env python3

### refSeqOnGenBank.py
### take RefSeq genes from a GCF refseq assembly and add them as tracks
### to the equivalent GCA genbank assembly

### also used is a botique module 'icecream' which is not typically
### available.

import sys
import os
import re
import subprocess
import csv

#############################################################################
### helper functions

### additional error handling could be done here to output information
### in case of script crashing with subprocess.CalledProcessError
### maybe later
def subProcess(cmd):
  if sys.version_info >= (3, 7):
    rtn = subprocess.run(cmd, stdout=subprocess.PIPE, text=True)
  else:
    rtn = subprocess.run(cmd, stdout=subprocess.PIPE, universal_newlines=True)
  if rtn.returncode != 0:
    print(f"ERROR: subProcess({cmd})")
    sys.exit(-1)

  return rtn.stdout.strip(), rtn.stderr
  # the strip() removes any leading or trailing whitespace
  # use just rtn.stdout if you want the precise output

def buildPath(asmId):
  gcX = asmId[0:3]
  d0 = asmId[4:7]
  d1 = asmId[7:10]
  d2 = asmId[10:13]
  tP = "/hive/data/genomes/asmHubs/allBuild/" + "/".join([gcX,d0,d1,d2,asmId])
  buildPath, stdErr = subProcess(["realpath", tP])
  if not os.path.isdir(buildPath):
    print(f"ERROR: can not find build directory {tP}")
    sys.exit(-1)
  return buildPath

#############################################################################

hostEnv, stdErr = subProcess(["uname", "-n"])
### to use all the special stuff when hgwdev /usr/bin/python3 is used
### this allows it to find the icecream module
if re.match(r'hgwdev*', hostEnv):
  sys.path.append("/cluster/software/lib/python3.6/site-packages")
  
### icecream is a convenient debug printout system using the ic() function
from icecream import install
install()
### ic(someArg) will print out the name of the argument and its contents
###             for any type of object
ic(sys.argv)
ic(sys.version_info)

# Check if the correct number of command line arguments is provided
if len(sys.argv) != 3:
    print("Usage: refSeqOnGenBank.py <refSeqAsmId> <genBankAsmId>")
    print("will add the RefSeq gene track to the GenBank assembly")
    print("when the refSeq genes exist on that refSeq assembly")
    print(" e.g.:\nrefSeqOnGenBank.py GCF_029281585.2_NHGRI_mGorGor1-v2.0_pri \\\n\tGCA_029281585.2_NHGRI_mGorGor1-v2.0_pri")
    sys.exit(-1)

refSeqAsmId = sys.argv[1]
genBankAsmId = sys.argv[2]

if not refSeqAsmId.startswith("GCF_"):
  print(f"ERROR: refSeqAsmId does not start with GCF_: {refSeqAsmId}")
  sys.exit(-1)

if not genBankAsmId.startswith("GCA_"):
  print(f"ERROR: genBankAsmId does not start with GCA_: {genBankAsmId}")
  sys.exit(-1)

# obtain the directories where the assembly build exists
rBuildPath = buildPath(refSeqAsmId)
ic(rBuildPath)
gBuildPath = buildPath(genBankAsmId)
ic(gBuildPath)

# verify refSeq gene track is present on the RefSeq assembly
refSeqBb = rBuildPath + "/trackData/ncbiRefSeq/" + refSeqAsmId + ".ncbiRefSeq.bb"

if not os.path.isfile(refSeqBb):
  print(f"ERROR: refSeq gene track not present on {refSeqAsmId}")
  print(f"{refSeqBb}")
  sys.exit(-1)

genBankWorkDir = gBuildPath + "/trackData/ncbiRefSeq"
if os.path.isdir(genBankWorkDir):
  print(f"ERROR: genBank ncbiRefSeq directory already exists")
  print(f"{genBankWorkDir}")
  sys.exit(-1)

os.makedirs(genBankWorkDir, exist_ok=True)
print(f"working in {genBankWorkDir}", file=sys.stderr)
gcfToGcaLift = genBankWorkDir + "/GCF.to.GCA.lift"

chromAlias = rBuildPath + "/trackData/chromAlias/" + refSeqAsmId + ".chromAlias.txt"
ic(chromAlias)
aliasBed = rBuildPath + "/trackData/chromAlias/" + refSeqAsmId + ".chromAlias.bed"
ic(aliasBed)

aliasHeaderLine, stdErr = subProcess(["head", "-1", chromAlias])
ic(aliasHeaderLine)
aliasFieldList = aliasHeaderLine.split('\t')
# this genbankIndex is the column in the bedAlias file, hence the + 3
try:
  genbankIndex = aliasFieldList.index("genbank") + 3
except ValueError:
  print(f"ERROR: can not find 'genbank' in alias field list")
  sys.exit(-1)

if 3 == genbankIndex:
  print(f"ERROR: found 'genbank' in alias list field as first one ?")
  sys.exit(-1)

ic(genbankIndex)

aliasLines = 0
liftLines = 0
with open(gcfToGcaLift, 'w') as gcfToGca:
  with open(aliasBed, 'r') as bedAlias:
    lines = csv.reader(bedAlias, delimiter='\t')
    for line in lines:
      aliasLines += 1
      if len(line[genbankIndex]) > 0:
        gcfToGca.write(f"{line[1]}\t{line[0]}\t{line[2]}\t{line[genbankIndex]}\t{line[2]}\n")
        liftLines += 1
#        print(f"{line[1]}\t{line[0]}\t{line[2]}\t{line[genbankIndex]}\t{line[2]}")

print(f"written\n{gcfToGcaLift}", file=sys.stderr)

lostLines = aliasLines - liftLines
if lostLines > 2:
  print(f"ERROR: more than two sequence aliases do not match")
  print(f"not matched: {lostLines} = {aliasLines} - {liftLines}")
  sys.exit(-1)

print(f"# refseq sequence count {aliasLines}, genbank sequence count {liftLines}", file=sys.stderr)

runSh = genBankWorkDir + "/run.sh"
with open(runSh, 'w') as doCmd:
  outStr = f"""#!/bin/bash
set -beEu -o pipefail

cd {genBankWorkDir}
export srcAsm="{refSeqAsmId}"
export targetAsm="{genBankAsmId}"
export liftFile="`pwd`/GCF.to.GCA.lift"

doNcbiRefSeq.pl -buildDir=`pwd` -assemblyHub \\
  -liftFile="${{liftFile}}" \\
  -target2bit="{gBuildPath}/{genBankAsmId}.2bit" "${{srcAsm}}" "${{targetAsm}}" \\
     > do.log 2>&1

cd {gBuildPath}
./doTrackDb.bash

"""
  doCmd.write(f"{outStr}")

print(f"running:\n{runSh}")
os.chmod(runSh, 0o775)

stdOut, stdErr = subProcess([runSh])
print(f"output from run.sh:\n{stdOut}\n{stdErr}")

sys.exit(0)
