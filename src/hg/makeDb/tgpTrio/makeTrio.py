#!/cluster/software/bin/python3

#######
# For a three column file relationship file:
#child parent1,parent2 label
#
# Search for the child in the primary 1000 Genomes VCF, and the parents
# in the supporting files, and make a merged VCF, one per chromsome, of
# the trio data. Remove variants where all 3 individuals are homozygous
# reference.
#
# The program relies on bcftools, make sure it's in your path
#######

import argparse,os,sys,subprocess
#from collections import defaultdict

validConfSettings = [
    'primaryVCF', # the VCF file(s) of unrelated 1000 Genomes data
    'suppVCF', # the VCF file of supplementary 1000 Genomes data
    'relationshipFile' # the definition of child and parent sample names
]

# tabix and bcftools commands we will use:
bcftoolsPath = "/hive/users/chmalee/bcftools/bin/bcftools"
tabixCmd = "tabix -p vcf "
viewCmd = bcftoolsPath + " view --no-version -s "
mergeCmd = bcftoolsPath + " merge --no-version "
filterCmd = " | " + bcftoolsPath + " view --no-version -i \"GT[*]!='RR'\" "
zipPipe = " | bgzip -c > "

#####
# setup command line options
#####

def parseCommandLine():
    """Parse command line, handle program usage, and help"""
    parser = argparse.ArgumentParser(
        description = "Filter and merge VCFs into trio VCFs, one directory per trio, one VCF per chromosome", add_help=True,
        prefix_chars = "-", usage = "%(prog)s [options]")

    parser.add_argument('confFile', action='store', default=None,
        help="File with various config settings, such as the primary and supplementary"
            "VCF and the relationship definitions")
    parser.add_argument('-v', "--verbose", action='store_true', default=False,
        help="Turn on extra logging to stderr")
    parser.add_argument('-n', "--dry-run", action="store_true", default=False,
        help="Dry-run mode, create work dirs and jobList but don't execute any jobs.")

    args = parser.parse_args()
    if not os.path.isfile(args.confFile):
        sys.stderr.write("ERROR: config file '%s' does not exist.\n" % args.confFile)
        sys.exit(1)
    return args

#####
# PARASOL RELATED UTILITY FUNCTIONS
#####

def runBatch(jobDir, verbose=False):
    """ssh to ku, run the para commands and return when jobs have finished running."""
    if verbose:
        sys.stderr.write("Running jobs in dir: %s\n" % (jobDir))
    cmd = "ssh ku 'cd %s; para stop; para flushResults; para resetCounts; para freeBatch; para clearSickNodes; para create jobList; para push'" % jobDir
    print(cmd)
    runCmd(cmd)

def createParasolJobList(jobDir, jobFileList, verbose=False):
    """Create the jobList parasol needs from a list of scripts to execute. Skips jobs if they've 
    already been run."""
    jobLines = []
    for script,outputName in jobFileList:
        if os.path.exists(outputName):
            if verbose:
                sys.stderr.write("output file already exists, skipping job: %s %s\n" % (script, outputName))
            continue
        cmdLine = "bash %s {check out exists+ %s}\n" % (script, outputName)
        jobLines.append(cmdLine)
    jobList = os.path.join(jobDir, "jobList")
    if verbose:
        sys.stderr.write("writing %d jobs to jobFile: %s\n" % (len(jobLines), jobList))
    with open(jobList, "w") as jlfh:
        for cmdLine in jobLines:
            jlfh.write(cmdLine)

def writeToJobFile(fh, cmdList):
    """Write command line strings in cmdList to open file handle fh."""
    cmds = "\n".join(cmdList)
    fh.write(cmds)

#####
# General util functions
#####

def cleanUpFiles(fileList, verbose=False):
    """Remove temporary vcf files and the tabix index files."""
    for f in fileList:
        if f.startswith("/") or f.startswith(".."): # only delete files in the current directory
            sys.stderr.write("ERROR: trying to remove non-temp file %s. Skipping for now\n" % f)
            continue
        elif os.path.exists(f):
            if verbose:
                sys.stderr.write("removing work file %s\n" % f)
            os.remove(f)
            os.remove(f+".tbi")
        else:
            sys.stderr.write("would have removed %s\n" % f)

def runCmd(cmdString, verbose=False):
    """Thin wrapper around subprocess.run() for clean error reporting."""
    try:
        if verbose:
            sys.stderr.write("Running command: %s\n" % cmdString)
            sys.stderr.flush()
        subprocess.run(cmdString, shell=True)
    except subprocess.CalledProcessError as e:
        sys.stderr.write("ERROR: error running command: %s\n" % cmdString)
        sys.stderr.write(e.msg + '\n')
        sys.exit(1)

def makeChromList():
    chromosomes = []
    for c in range(1,23):
        chromosomes.append("chr"+str(c))
    chromosomes.append("chrX")
    return chromosomes

#####
# End utility functions
#####

def defineTrios(relationshipFname, verbose=False):
    """parse out the trio definitions in relationshipFname"""
    trioDefs = {}
    labelSet = set()
    with open(relationshipFname) as infh:
        if verbose:
            sys.stderr.write("reading relationship file: '%s'\n" % relationshipFname)
        lineCount = 1
        for line in infh:
            if line.startswith("#"):
                lineCount += 1
                continue
            splitLine = line.strip().split()
            child = splitLine[0]
            parents = splitLine[1]
            label = splitLine[2]
            if label in labelSet:
                sys.stderr.write("ERROR: label '%s' not unique %s:%d\n" % (label,relationshipFname,lineCount))
                sys.exit(1)
            labelSet.add(label)
            lineCount += 1
            trioDefs[(child, label)] = parents
    return trioDefs

def parseConf(confFname,verbose=False):
    """Parse conf file, lines starting with '#' are ignored. If the relationshipFile setting
        is present parse it into a dictionary as well.
        Valid settings are:
        relationshipFile: the 3 column file defining a child sample, the parents, and a label
        primaryVCF: the path to the primary 1000 Genomes dataset
        suppVCF: the path to the supplementary VCF with related individuals"""
    ret = {}
    with open(confFname) as f:
        for line in f:
            if line.startswith('#'):
                continue
            trimmed = line.strip()
            split = trimmed.split('=')
            key = split[0].strip()
            if key not in validConfSettings:
                if verbose:
                    sys.stderr.write("WARNING: config setting '%s' not supported, skipping\n")
                continue
            val = split[1].strip()
            if key == "relationshipFile":
                key = "trios"
                val = defineTrios(val,verbose)
            ret[key] = val
    return ret

def subFileName(conf, chromosome):
    """Replace the '*' in a conf file with the current chromosome"""
    primary = conf["primaryVCF"].replace("*", chromosome)
    supp = conf["suppVCF"].replace("*", chromosome)
    return primary, supp

def checkTrioExists(conf, childName, parentNames, chromosome):
    """Return the right VCF file paths or None if both the parent and child sample IDS
    can't be found."""
    primary, supp = subFileName(conf, chromosome)
    findCmdStr1 = "bcftools query -l " + primary + " | grep "
    findCmdStr2 = "bcftools query -l " + supp + " | grep "
    childRet = None
    par1Ret = None
    par2Ret = None

    # make sure the child sample exists:
    if subprocess.run(findCmdStr1+childName,stdout=subprocess.DEVNULL,shell=True).returncode == 0:
        childRet = primary
    elif subprocess.run(findCmdStr2+childName,stdout=subprocess.DEVNULL,shell=True).returncode == 0:
        childRet = supp
    else:
        sys.stderr.write("WARNING: '%s' does not exist in VCFs, "
            "skipping trio: %s, %s\n" % (childName, childName, parentNames))
        return (None, None, None)

    # make sure the parent samples exist
    parList = parentNames.split(',')
    for ix in range(len(parList)):
        par = parList[ix]
        if subprocess.run(findCmdStr1+par, stdout=subprocess.DEVNULL, shell=True).returncode == 0:
            tmpParRet = primary 
            #if tmpParRet != parRet and parRet != None:
            #    sys.stderr.write("ERROR: parent sample IDs: '%s' found in separate files. "
            #        "Trio %s, %s\n" % (parentNames, childName, parentNames))
            #    sys.exit(1)
        elif subprocess.run(findCmdStr2+par, stdout=subprocess.DEVNULL, shell=True).returncode == 0:
            tmpParRet = supp
            #if tmpParRet != parRet and parRet != None:
            #    sys.stderr.write("ERROR: parent sample IDs: '%s' found in separate files. "
            #        "Trio %s, %s\n" % (parentNames, childName, parentNames))
            #    sys.exit(1)
        else:
            sys.stderr.write("WARNING: '%s' does not exist in VCFs, "
                "skipping trio: %s, %s\n" % (par, childName, parentNames))
            return (None, None, None)
        if ix == 0: par1Ret = tmpParRet
        else: par2Ret = tmpParRet
    return (childRet, par1Ret, par2Ret)

def setupWorkDirs(verbose=False):
    """Create work and output dirs if they don't exist and return their names."""
    workDir = os.path.join(os.getcwd(),"work")
    finalDir = os.path.join(os.getcwd(), "output")
    if verbose:
        sys.stderr.write("temp directory with intermediate VCFs: %s/\n" % (workDir))
        sys.stderr.write("directory with final VCFs: %s/\n" % (finalDir))
    if not os.path.exists(workDir):
        os.mkdir(workDir)
    if not os.path.exists(finalDir):
        os.mkdir(finalDir)
    return workDir, finalDir

def genTrioScript(workDir, finalDir, trioName, childStr, parentStr, chromosome,
                    childFname, parent1Fname, parent2Fname):
    """Build up the string of bcftools commands to make a trio VCF."""
    ret = "#!/bin/bash\n"
    ret += "set -beEu -o pipefail\n"
    ret += "temp1=$1\n" # parasol throws the output file on for some reason so catch it
    outParent1Fname = os.path.join(workDir, trioName + "Parent1." + chromosome + ".vcf.gz")
    outParent2Fname = os.path.join(workDir, trioName + "Parent2." + chromosome + ".vcf.gz")
    outChildFname = os.path.join(workDir, trioName + "Child." + chromosome + ".vcf.gz")
    finalOutFname = os.path.join(finalDir, trioName + "Trio." + chromosome + ".vcf.gz")
    parent1, parent2 = parentStr.split(',')
    parent1Cmd = viewCmd + "'" + parent1 + "' " + parent1Fname + zipPipe + outParent1Fname
    parent2Cmd = viewCmd + "'" + parent2 + "' " + parent2Fname + zipPipe + outParent2Fname
    childCmd = viewCmd + "'" + childStr + "' " + childFname + zipPipe + outChildFname
    mergeFilterCmd = mergeCmd + outChildFname + " " + outParent1Fname + " " + outParent2Fname + filterCmd + zipPipe + finalOutFname
    ret += parent1Cmd + "\n"
    ret += parent2Cmd + "\n"
    ret += childCmd + "\n"
    ret += tabixCmd + outParent1Fname + "\n"
    ret += tabixCmd + outParent2Fname + "\n"
    ret += tabixCmd + outChildFname + "\n"
    ret += mergeFilterCmd + "\n"
    ret += tabixCmd + finalOutFname + "\n"
    return ret

def makeTrios(conf, dryRun=False, verbose=False):
    """Given the trio definitions in conf['trios'], check the trios actually exist in
    1000 Genomes data, find which samples are in which VCFs, and generate per chromosome
    trio VCFs."""
    baseWorkDir, baseFinalDir = setupWorkDirs(verbose)
    chromosomes = makeChromList()
    trioDefs = conf["trios"]
    jobTupleList = [] # a list of (scriptName, outFile) tuples for parasol
    for trio in trioDefs:
        trioName = trio[1]
        childStr = trio[0]
        parentsStr = trioDefs[trio]
        workDir = os.path.join(baseWorkDir, trioName)
        finalDir = os.path.join(baseFinalDir, trioName)
        childFname, parent1Fname, parent2Fname = checkTrioExists(conf, childStr, parentsStr, "chrX")
        if childFname is not None and parent1Fname is not None and parent2Fname is not None:
            if not os.path.exists(workDir):
                os.mkdir(workDir)
            if not os.path.exists(finalDir):
                os.mkdir(finalDir)
            for c in chromosomes:
                childFname, parent1Fname, parent2Fname = checkTrioExists(conf, childStr, parentsStr, c)
                jobFile = os.path.join(workDir, trio[1] + "." + c + ".sh")
                with open(jobFile, "w+") as jfh:
                    cmdScript = genTrioScript(workDir, finalDir, trioName, childStr,
                        parentsStr, c, childFname, parent1Fname, parent2Fname)
                    jfh.write(cmdScript)
                    finalOutFname = os.path.join(finalDir, trioName + "Trio." + c + ".vcf.gz")
                    jobTupleList.append((jobFile, finalOutFname))

    # send all the jobs we want to run over to ku
    createParasolJobList(baseWorkDir, jobTupleList, verbose)
    if not dryRun:
        runBatch(baseWorkDir,verbose)

def main():
    args = parseCommandLine()
    conf = parseConf(args.confFile)
    makeTrios(conf, args.dry_run, args.verbose)

if __name__=="__main__":
    main()
