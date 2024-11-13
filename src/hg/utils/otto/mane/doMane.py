#Ottomized by Lou 9/14/23

import subprocess, os

def bash(cmd):
    """Run the cmd in bash subprocess"""
    try:
        rawBashOutput = subprocess.run(cmd, check=True, shell=True,\
                                       stdout=subprocess.PIPE, universal_newlines=True, stderr=subprocess.STDOUT)
        bashStdoutt = rawBashOutput.stdout
    except subprocess.CalledProcessError as e:
        raise RuntimeError("command '{}' return with error (code {}): {}".format(e.cmd, e.returncode, e.output))
    return(bashStdoutt)

def doMane():
    latestRelease = bash('curl -s https://ftp.ncbi.nlm.nih.gov/refseq/MANE/trackhub/data/ | grep "release_" | tail -1')
    dirName = latestRelease.split(">")[1].split("<")[0]
    version = dirName.split("_")[1].split("/")[0]
    latestReleaseUrl = "https://ftp.ncbi.nlm.nih.gov/refseq/MANE/trackhub/data/"+dirName+"MANE.GRCh38.v"+version+".ensembl.bb"
    workindDir = "/hive/data/outside/otto/mane/mane."+version
    filePathBB = workindDir+"/our.bb"
    filePathBed = workindDir+"/our.bed"
    filePathArchive = "/usr/local/apache/htdocs-hgdownload/goldenPath/archive/hg38/mane/mane."+version+".bb"
    if not os.path.isfile(filePathBB):
        print("New MANE update: "+latestReleaseUrl)
        bash("mkdir -p "+workindDir)
        bash("bigBedToBed "+latestReleaseUrl+" "+filePathBed)
        bash("bedToBigBed -extraIndex=name "+filePathBed+" /cluster/data/hg38/chrom.sizes "+filePathBB+" -type=bed12+13 -as=$HOME/kent/src/hg/lib/mane.as -tab")
        bash("mkdir -p /gbdb/hg38/mane")
        print("Previous version itemCount:")
        print(bash("bigBedInfo /gbdb/hg38/mane/mane.bb | grep -i itemCount"))
        bash("rm -f /gbdb/hg38/mane/mane.bb")
        bash("ln -s "+filePathBB+" /gbdb/hg38/mane/mane.bb")
        bash("ln -sf "+filePathBB+" "+filePathArchive)
        print("New version itemCount:")
        print(bash("bigBedInfo /gbdb/hg38/mane/mane.bb | grep -i itemCount"))
        bash("tawk '{print $13, $18, $19, $21, $22, $23, $24}' "+filePathBed+" > "+workindDir+"/our.ixInput")
        bash("ixIxx "+workindDir+"/our.ixInput "+workindDir+"/mane.ix "+workindDir+"/mane.ixx")
        bash("rm -f /gbdb/hg38/mane/mane.ix /gbdb/hg38/mane/mane.ixx")
        bash("ln -s "+workindDir+"/mane.ix /gbdb/hg38/mane")
        bash("ln -s "+workindDir+"/mane.ixx /gbdb/hg38/mane")
        bash("wget https://ftp.ncbi.nlm.nih.gov/refseq/MANE/MANE_human/release_"+version+"/README_versions.txt -O "+workindDir+"/README_versions.txt")
        bash("rm -f /gbdb/hg38/mane/README_versions.txt")
        bash("ln -s "+workindDir+"/README_versions.txt /gbdb/hg38/mane")

doMane()
