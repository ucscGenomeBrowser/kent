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
import os
from datetime import datetime

def bash(cmd):
    """Input bash cmd and return stdout"""
    rawOutput = subprocess.run(cmd,check=False, shell=True, stdout=subprocess.PIPE, universal_newlines=True)
    return(rawOutput.stdout.split('\n')[0:-1])


def parseGitFilesAndMd5sums(fileListWithMd5sum, fileNameDic, fileNameHiveMatches, fileNameGitPath):
    for fileMd5 in fileListWithMd5sum:
        md5sum = fileMd5.split(" ")[0]
        gitPath = fileMd5.split("  ")[1]
        fileName = gitPath.split("/")[-1]
        fileNameDic[fileName] = md5sum
        fileNameGitPath[fileName] = gitPath
        fileNameHiveMatches[fileName] = []
    return(fileNameDic, fileNameHiveMatches, fileNameGitPath)


def searchHiveFiles(fileNameDic,fileNameHiveMatches):
    """Find git otto files in the hive otto dir and get md5sums"""
    for fileName in fileNameDic.keys():   
        hiveSearchMd5sum = bash(f"find /hive/data/outside/otto/ -maxdepth 3 -name '{fileName}' 2>/dev/null")
        if hiveSearchMd5sum != []:
            for fileHit in hiveSearchMd5sum:
                fileHit = fileHit.strip()
                if os.path.isfile(fileHit):
                    if os.access(fileHit, os.R_OK):
                        fileHitMd5Sum = bash('md5sum '+fileHit)
                        md5 = fileHitMd5Sum[0].split("  ")[0]
                        fileNameHiveMatches[fileName].append((md5, fileHit))
    return(fileNameHiveMatches)
    
def compareGitMd5sumsToHiveMd5sums(fileNameDic, fileNameHiveMatches, fileNameGitPath):
    """Compare md5sums between files in git and all matching files in hive"""
    headerPrinted = False
    for fileName in fileNameDic.keys():
        hiveMd5s = [m for m, _ in fileNameHiveMatches[fileName]]
        if fileNameDic[fileName] in hiveMd5s:
            continue
        elif not fileNameHiveMatches[fileName]:
            continue
        else:
            if not headerPrinted:
                print("The following otto file(s) were found, but the md5sum of the git file did not match the one running on hive.")
                headerPrinted = True
            gitPath = fileNameGitPath[fileName]
            # Use last commit time, not working-tree mtime: checkout resets mtime to checkout time.
            gitTimeRaw = bash(f"git -C ~/kent log -1 --format=%ct -- {gitPath}")
            gitTime = int(gitTimeRaw[0]) if gitTimeRaw else 0
            print(f"\n======= {fileName} =======")
            print(f"  git:  {gitPath}\n        (last commit: {datetime.fromtimestamp(gitTime)})")
            for md5, hivePath in fileNameHiveMatches[fileName]:
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
    fileNameDic = {}        # basename -> git md5sum
    fileNameHiveMatches = {}  # basename -> [(hive md5sum, hive full path), ...]
    fileNameGitPath = {}    # basename -> git full path
    fileListWithMd5sum[0].split("  ")
    return(fileListWithMd5sum, fileNameDic, fileNameHiveMatches, fileNameGitPath)
    
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
    fileListWithMd5sum, fileNameDic, fileNameHiveMatches, fileNameGitPath = findGitFilesBuildDics()
    parseGitFilesAndMd5sums(fileListWithMd5sum, fileNameDic, fileNameHiveMatches, fileNameGitPath)
    searchHiveFiles(fileNameDic, fileNameHiveMatches)
    compareGitMd5sumsToHiveMd5sums(fileNameDic, fileNameHiveMatches, fileNameGitPath)
    checkCrontabDifferences()

main()
