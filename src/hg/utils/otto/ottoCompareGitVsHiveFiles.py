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

def bash(cmd):
    """Input bash cmd and return stdout"""
    rawOutput = subprocess.run(cmd,check=True, shell=True, stdout=subprocess.PIPE, universal_newlines=True)
    return(rawOutput.stdout.split('\n')[0:-1])


def parseGitFilesAndMd5sums(fileListWithMd5sum,fileNameDic,fileNameHiveMatches):
    for fileMd5 in fileListWithMd5sum:
        md5sum = fileMd5.split(" ")[0]
        fileName = fileMd5.split("  ")[1].split("/")[-1]
        fileNameDic[fileName] = md5sum
        fileNameHiveMatches[fileName] = []
    return(fileNameDic,fileNameHiveMatches)


def searchHiveFiles(fileNameDic,fileNameHiveMatches):
    """Find git otto files in the hive otto dir and get md5sums"""
    for fileName in fileNameDic.keys():   
        hiveSearchMd5sum = bash("find /hive/data/outside/otto/ -maxdepth 3 -name "+fileName)
        if hiveSearchMd5sum != []:
            for fileHit in hiveSearchMd5sum:
                fileHitMd5Sum = bash('md5sum '+fileHit)
                fileNameHiveMatches[fileName].append(fileHitMd5Sum[0].split("  ")[0])
    return(fileNameHiveMatches)
    
def compareGitMd5sumsToHiveMd5sums(fileNameDic,fileNameHiveMatches):
    """Compare md5sums between files in git and all matching files in hive"""
    for fileName in fileNameDic.keys():
        if fileNameDic[fileName] in fileNameHiveMatches[fileName]:
            continue
            print("Found matching file: "+fileName)
        elif fileNameHiveMatches[fileName] == []:
            continue
            print("File was never found: "+fileName)
        else:
            print("The following otto file was found, but the md5sum of the git file did not match the one running on hive: "+fileName)
            

def findGitFilesBuildDics():
    """Find all files in git minus exceptions and get md5sums, build dics"""
    fileListWithMd5sum = bash("find ~/kent/src/hg/utils/otto -type f | grep -v 'uniprot\|ncbiRefSeq\|crontab\|README*\|clinvarSubLolly\|makefile\|.c$\|sarscov2phylo\|nextstrainNcov\|knownGene\|rsv/exclude.ids\|mask.bed' | xargs md5sum")
    fileNameDic = {}
    fileNameHiveMatches = {}
    fileListWithMd5sum[0].split("  ")
    return(fileListWithMd5sum,fileNameDic,fileNameHiveMatches)
    
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
    fileListWithMd5sum,fileNameDic,fileNameHiveMatches = findGitFilesBuildDics()
    parseGitFilesAndMd5sums(fileListWithMd5sum,fileNameDic,fileNameHiveMatches)
    searchHiveFiles(fileNameDic,fileNameHiveMatches)
    compareGitMd5sumsToHiveMd5sums(fileNameDic,fileNameHiveMatches)
    checkCrontabDifferences()

main()
