#!/usr/bin/env python3
"""
Look up all files committed to the otto directory in git:

~/kent/src/hg/utils/otto

And compare their md5sums to the same files in the otto hive directory:

/hive/data/outside/otto/

The goal is to make sure there are no differences between the scripts being
ran by otto (hive) and the scripts committed into the kent tree. This script
only looks 3 directories deep in hive, which catches almost everything and otherwise
starts to search many archive directories. Also there are some exceptions to what
directories it looks at in git, these can be controlled in the function findGitFilesBuildDics.
"""

import subprocess
import getpass
import filecmp
import difflib
import os
from datetime import datetime

# How alike two files must be (fraction of shared lines) before a hive file that
# shares a basename with a git file is treated as a copy of it.  Real pairs that
# have drifted score well above 0.9, unrelated same-name scripts below 0.2.
minSimilarity = 0.5

def bash(cmd):
    """Input bash cmd and return stdout"""
    rawOutput = subprocess.run(cmd,check=False, shell=True, stdout=subprocess.PIPE, universal_newlines=True)
    return(rawOutput.stdout.split('\n')[0:-1])


def readLines(filePath):
    """Read a file as a list of lines, tolerating binary/non-utf8 content"""
    with open(filePath, errors='replace') as f:
        return(f.readlines())


def looksLikeSameFile(gitPath, hivePath):
    """A shared basename does not make two files the same file: hive has
    grcIncidentDb/GRCh37/runUpdate.sh, a per-assembly script unrelated to the
    genArk/asmAlias/runUpdate.sh in git.  Compare the contents so only files
    that really are copies of each other get reported as out of sync."""
    gitLines = readLines(gitPath)
    hiveLines = readLines(hivePath)
    matcher = difflib.SequenceMatcher(None, gitLines, hiveLines)
    # quick_ratio is a cheap upper bound, so a low score rules the pair out
    if matcher.quick_ratio() < minSimilarity:
        return(False)
    return(matcher.ratio() >= minSimilarity)


def parseGitFilesAndMd5sums(fileListWithMd5sum, gitPathDic, gitPathHiveMatches):
    """Key on the full git path, not the basename: several otto dirs hold files
    with the same name (runUpdate.sh, doUpdate.sh, download.sh ...) and keying
    on basename let one git file silently replace the other."""
    for fileMd5 in fileListWithMd5sum:
        md5sum = fileMd5.split(" ")[0]
        gitPath = fileMd5.split("  ")[1]
        gitPathDic[gitPath] = md5sum
        gitPathHiveMatches[gitPath] = []
    return(gitPathDic, gitPathHiveMatches)


def searchHiveFiles(gitPathDic,gitPathHiveMatches):
    """Find git otto files in the hive otto dir and get md5sums"""
    md5sByFileName = {}    # basename -> set of md5sums of every git file with that name
    for gitPath, md5sum in gitPathDic.items():
        md5sByFileName.setdefault(os.path.basename(gitPath), set()).add(md5sum)

    hiveSearchCache = {}   # basename -> [hive paths], the same name is looked up once
    for gitPath in gitPathDic.keys():
        fileName = os.path.basename(gitPath)
        if fileName not in hiveSearchCache:
            hiveSearchCache[fileName] = bash(f"find /hive/data/outside/otto/ -maxdepth 3 -name '{fileName}' 2>/dev/null")
        for fileHit in hiveSearchCache[fileName]:
            fileHit = fileHit.strip()
            if os.path.isfile(fileHit):
                if os.access(fileHit, os.R_OK):
                    fileHitMd5Sum = bash('md5sum '+fileHit)
                    md5 = fileHitMd5Sum[0].split("  ")[0]
                    if md5 != gitPathDic[gitPath] and md5 in md5sByFileName[fileName]:
                        # in sync with a different git file of the same name
                        continue
                    if md5 != gitPathDic[gitPath] and not looksLikeSameFile(gitPath, fileHit):
                        continue
                    gitPathHiveMatches[gitPath].append((md5, fileHit))
    return(gitPathHiveMatches)

def compareGitMd5sumsToHiveMd5sums(gitPathDic, gitPathHiveMatches):
    """Compare md5sums between files in git and all matching files in hive"""
    headerPrinted = False
    for gitPath in gitPathDic.keys():
        hiveMd5s = [m for m, _ in gitPathHiveMatches[gitPath]]
        if gitPathDic[gitPath] in hiveMd5s:
            continue
        elif not gitPathHiveMatches[gitPath]:
            continue
        else:
            if not headerPrinted:
                print("The following otto file(s) were found, but the md5sum of the git file did not match the one running on hive.")
                headerPrinted = True
            # Use last commit time, not working-tree mtime: checkout resets mtime to checkout time.
            gitTimeRaw = bash(f"git -C ~/kent log -1 --format=%ct -- {gitPath}")
            gitTime = int(gitTimeRaw[0]) if gitTimeRaw else 0
            print(f"\n======= {os.path.basename(gitPath)} =======")
            print(f"  git:  {gitPath}\n        (last commit: {datetime.fromtimestamp(gitTime)})")
            for md5, hivePath in gitPathHiveMatches[gitPath]:
                hiveTime = int(os.path.getmtime(hivePath))
                if gitTime > hiveTime:
                    message = "git is newer and hive needs to be updated."
                else:
                    message = "hive is newer and git needs to be updated."
                print(f"\n  hive: {hivePath}\n        (mtime: {datetime.fromtimestamp(hiveTime)})")
                print(f"\n  {message}")



def findGitFilesBuildDics():
    """Find all files in git minus exceptions and get md5sums, build dics"""
    fileListWithMd5sum = bash("find ~/kent/src/hg/utils/otto -type f | grep -Ev 'uniprot|ncbiRefSeq|crontab|README*|clinvarSubLolly|makefile|.c$|sarscov2phylo|nextstrainNcov|knownGene|rsv/exclude.ids|mask.bed|.gitignore|R00000039_repregions.bed' | xargs md5sum")
    gitPathDic = {}         # git full path -> git md5sum
    gitPathHiveMatches = {} # git full path -> [(hive md5sum, hive full path), ...]
    return(fileListWithMd5sum, gitPathDic, gitPathHiveMatches)

def checkCrontabDifferences():
    """Looks for differences between the committed otto crontab and the one running"""
    user = getpass.getuser()

    if user != "otto":
        crontab = bash("ssh otto@hgwdev crontab -l")
    else:
        crontab = bash("crontab -l")
    
    liveCrontab = open("/cluster/home/"+user+"/ottoCrontab.tmp",'w')
    for line in crontab:
        liveCrontab.write(line+"\n")
    liveCrontab.close()

    file1 = "/cluster/home/"+user+"/ottoCrontab.tmp"
    file2 = "/cluster/home/"+user+"/kent/src/hg/utils/otto/otto.crontab"
    comparison = filecmp.cmp(file1, file2)

    if not comparison:
        print("Differences found between running crontab and git crontab.")
        print("Showing: diff liveCrontab gitCrontab\n")
        diffs = bash("diff "+file1+" "+file2+" || :")
        for l in diffs:
            print(l)
        
    bash("rm /cluster/home/"+user+"/ottoCrontab.tmp")

def main():
    """
    Initialized options and calls other functions.
    """
    fileListWithMd5sum, gitPathDic, gitPathHiveMatches = findGitFilesBuildDics()
    parseGitFilesAndMd5sums(fileListWithMd5sum, gitPathDic, gitPathHiveMatches)
    searchHiveFiles(gitPathDic, gitPathHiveMatches)
    compareGitMd5sumsToHiveMd5sums(gitPathDic, gitPathHiveMatches)
    checkCrontabDifferences()

main()
