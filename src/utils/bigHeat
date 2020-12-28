#!/usr/bin/env python

import logging, sys, optparse, subprocess, os, math, re
from collections import defaultdict
from os.path import *

# made with: import cm from matplotlib; (cm.get_cmap("viridis")(x)*255)[:, 0:3].astype(int).tolist()
viridisPal = [[68, 1, 84], [71, 18, 101], [72, 35, 116], [69, 52, 127], [64, 67, 135], [58, 82, 139], [52, 94, 141], [46, 107, 142], [41, 120, 142], [36, 132, 141], [32, 144, 140], [30, 155, 137], [34, 167, 132], [47, 179, 123], [68, 190, 112], [94, 201, 97], [121, 209, 81], [154, 216, 60], [189, 222, 38], [223, 227, 24], [253, 231, 36]]

pal = None
binCount = 20

# can be set to something like '/gbdb/wuhCor1/pbm/IgG_Z-score-Healthy_Controls/' if your symlinks are in place
bbDir = None

# ==== functions =====
def parseArgs():
    " setup logging, parse command line arguments and options. -h shows auto-generated help page "
    parser = optparse.OptionParser("""usage: %prog [options] locationBed locationMatrixFnames chromSizes outDir - create one feature
            Duplicate BED features and color by them by the values in locationMatrix.
            Creates new bigBed files in outDir and creates a basic trackDb.ra file there.

    BED file looks like this:

            chr1 1 1000 myGene 0 + 1 1000 0,0,0
            chr2 1 1000 myGene2 0 + 1 1000 0,0,0

    locationMatrix looks like this:

            gene sample1 sample2 sample3
            myGene 1 2 3
            myGene2 0.1 3 10
            myGene2_probe2 0.1 3 10

    This will create a composite with three subtracks (sample1, sample2, sample). Each subtrack will have myGene,
    and colored in intensity with sample3 more intense than sample1 and sample2. Same for myGene2.
    Also can add a bigWig with a summary of all these values, one per nucleotide
    """)

    #parser.add_option("-d", "--debug", dest="debug", action="store_true", help="show debug messages")
    #(options, args) = parser.parse_args()

    parser.add_option("", "--scale", dest="scale", action="store", help="scale the values to 0-1.0 automatically. One of the values 'none' or 'row'. Default is 'none'. If 'none', the options --min/--max/--mult can be used to ajust the scaling manually", default='none')
    parser.add_option("", "--min", dest="min", action="store", type="float", help="minimum limit when transforming values to colors. Can be used to remove the distorting effect of outliers.")
    parser.add_option("", "--max", dest="max", action="store", type="float", help="maximum limit when transforming values to colors")
    parser.add_option("", "--mult", dest="mult", action="store", help="Two comma-separated values multNeg,multPos. If the number is negative, multiNeg is used, otherwise multPos is used. This multiplies all numbers with these factors before doing anything else.")
    parser.add_option("", "--palRange", dest="palRange", action="store", help="Two colon-separated float numbers 0-1.0. by default, the color palette is used from its minimum 0.0 to its maximum 1.0. By setting this value, you can use only some part of the color palette, e.g. '0.1:0.9'")
    parser.add_option("", "--log", dest="doLog", action="store_true", help="Log all values in the matrix. Negative values will be set to 1 and will also trigger a warning message.")
    parser.add_option("", "--order", dest="doOrder", action="store_true", help="Reorder the rows with the optimalLeaf algorithm, like a real heatmap")
    parser.add_option("", "--del", dest="delSamples", action="append", help="Remove these sample. A regex. Can be specified multiple times. Useful to remove outliers.")
    parser.add_option("", "--cmap", dest="cmap", action="store", help="name of colormap. Requires matplotlib to be installed if set to something else than viridis.", default="viridis")
    parser.add_option("-b", "--bbDir", dest="bbDir", action="store", help="when writing trackDb, prefix all bigDataUrl filenames with this. Use this with the /gbdb directory.")
    parser.add_option("", "--bw", dest="bigWig", action="store", help="Also create a bigWig that summarizes all values per nucleotide. Can be one of: 'sum', 'respFreq'")

    parser.add_option("-d", "--debug", dest="debug", action="store_true", help="show debug messages")
    (options, args) = parser.parse_args()

    if args==[]:
        parser.print_help()
        exit(1)

    if options.debug:
        logging.basicConfig(level=logging.DEBUG)
        logging.getLogger().setLevel(logging.DEBUG)
    else:
        logging.basicConfig(level=logging.INFO)
        logging.getLogger().setLevel(logging.INFO)

    return args, options

def parseBed(bedFname):
    beds = {}
    for line in open(bedFname):
        row = line.rstrip("\n").split("\t")
        assert(len(row)>=9)
        row = row[:9]
        beds[row[3]] = row
    return beds

def toBigBed(bedFnames, sampleOrder, chromSizes):
    " convert to bigBed "
    bbFnames = {}
    for sampleName in sampleOrder:
        fname = bedFnames[sampleName]
        bbFname = splitext(fname)[0]+".bb"
        bbFnames[sampleName] = bbFname
        print("Converting %s to %s" % (fname, bbFname))
        cmd = ["bedSort", fname, fname]
        assert(subprocess.call(cmd)==0)
        cmd = ["bedToBigBed", fname, chromSizes, bbFname, "-tab"]
        assert(subprocess.call(cmd)==0)
        os.remove(fname)
    return bbFnames

def makeTrackDb(bbFnames, sampleOrder, outDir, parentName):
    " write a trackDb file to outDir "
    global bbDir
    tdbFn = join(outDir, "trackDb.ra")
    tdbFh = open(tdbFn, "w")

    label = parentName.replace("_", " ")
    tdbFh.write("track %s\n" % parentName)
    tdbFh.write("shortLabel %s\n" % label)
    tdbFh.write("longLabel %s\n" % label)
    tdbFh.write("type bed 9\n")
    tdbFh.write("compositeTrack on\n")
    tdbFh.write("visibility dense\n")
    tdbFh.write("labelOnFeature on\n")
    tdbFh.write("itemRgb on\n")
    tdbFh.write("\n")

    for prio, sampleName in enumerate(sampleOrder):
        bbFname = bbFnames[sampleName]
        trackName = splitext(basename(bbFname))[0]
        trackLabel = trackName.replace("_", " ")
        fullTrackName = parentName+"_"+trackName # avoid subtrack name clashes
        if bbDir:
            bbPath = join(bbDir, basename(bbFname))
        else:
            bbPath = bbFname
        tdbFh.write("    track %s\n" % fullTrackName)
        tdbFh.write("    shortLabel %s\n" % trackLabel)
        tdbFh.write("    longLabel %s\n" % trackLabel)
        tdbFh.write("    type bigBed 9\n")
        tdbFh.write("    priority %d\n" % prio)
        tdbFh.write("    bigDataUrl %s\n" % bbPath)
        tdbFh.write("    parent %s on\n" % parentName)
        tdbFh.write("\n")

    if bbDir:
        bwPath = join(bbDir, "allSum.bw")
    else:
        bwPath = bbFname

    tdbFh.write("track %sAllSum\n" % parentName)
    tdbFh.write("shortLabel %s Summary\n" % label)
    tdbFh.write("longLabel %s Sum of all scores per nucleotide\n" % label)
    tdbFh.write("type bigWig\n")
    tdbFh.write("visibility full\n")
    tdbFh.write("autoScale on\n")
    tdbFh.write("maxHeightPixels 100:28:8\n")
    tdbFh.write("bigDataUrl %s\n" % bwPath)
    tdbFh.write("\n")

    tdbFh.close()

def optimalLeaf(fname):
    " return optimal order of the columns in fname (tsv)"
    logging.info("Reordering leaves")
    import pandas as pd
    from scipy.cluster import hierarchy
    t = pd.read_csv(fname, sep="\t", index_col=0)
    mat = t.values
    mat = mat.transpose()
    Z = hierarchy.ward(mat)
    Z = hierarchy.ward(mat)
    order = hierarchy.leaves_list(hierarchy.optimal_leaf_ordering(Z, mat))
    #labels = t.index
    labels = t.columns
    assert(len(labels)==len(order))
    labelsOrdered = [labels[i] for i in list(order)]
    return labelsOrdered

def parseChromSizes(fname):
    chromSizes = {}
    for line in open(fname):
        chrom, size = line.rstrip().split()
        size = int(size)
        chromSizes[chrom] = size
    return chromSizes

def makeBigWig(beds, nameValSum, chromSizeFname, bwFname):
    " write a bigWig with the value for each bed to bwFname "
    chromSizes = parseChromSizes(chromSizeFname)

    nuclScores = {}
    for chrom, size in chromSizes.items():
        nuclScores[chrom] = [0.0]*size

    for name, val in nameValSum.items():
        if name not in beds:
            name = name.split("_")[0]
            if name not in beds:
                logging.warning("Cannot map sample name %s to genome, not found in bed file." % name)
                continue

        chrom, start, end = beds[name][:3]
        chromScores = nuclScores[chrom]

        for i in range(int(start), int(end)):
            chromScores[i] += val

    wigFname = bwFname+".wig"
    ofh = open(wigFname, "w")
    for chrom, chromScores in nuclScores.items():
        ofh.write("variableStep  chrom=%s\n" % chrom)
        for pos, score in enumerate(chromScores):
            if score!=0.0:
                ofh.write("%d %f\n" % (pos, score))
    ofh.close()
    print("Created %s" % wigFname)

    cmd = ["wigToBigWig", wigFname, chromSizeFname, bwFname]
    subprocess.check_call(cmd)
    print("Created %s" % bwFname)

def calcRespFreq(sampleNames, vals):
    " response frequency as defined in https://www.nature.com/articles/s41423-020-00523-5, supplemental methods "
    import numpy as np
    assert(len(sampleNames)==len(vals))
    # sort values into control and disease
    ctrlVals = []
    disVals = []
    for sampleName, val in zip(sampleNames, vals):
        if "Ctrl" in sampleName:
            ctrlVals.append(val)
        else:
            disVals.append(val)

    mean = np.mean(vals)
    stdev = np.std(ctrlVals)
    cutoff = mean+(3*stdev)
    logging.info("ctrl: %s" % ctrlVals)
    logging.info("vals: %s" % disVals)
    disHigher = [x for x in disVals if x > cutoff]
    logging.info("higher: %s" % repr(disHigher))
    respFreq = float(len(disHigher)) / len(disVals)
    logging.info("Mean=%f, Stdev=%f, Cutoff=%f, gtThanCutoff=%d, totalCount=%d, resp frequency: %f" % (mean, stdev, cutoff, len(disHigher), len(disVals), respFreq))
    assert(respFreq < 1.0)
    return respFreq

def bigHeat(beds, fname, chromSizes, scale, minVal, maxVal, mult, doLog, doOrder, delSamples, sumFunc, outDir):
    " create bed files, convert them to bigBeds and make trackDb.ra in the directory "
    global bbDir
    global pal

    if pal is None:
        pal = viridisPal
        binCount = 10

    colorCount = len(pal)
    colSteps = 1.0/colorCount
    palStrs = [ ",".join([str(x) for x in col]) for col in pal]

    multNeg, multPos = None, None
    if mult:
        x, y = mult.split(",")
        multNeg = float(x)
        multPos = float(y)

    baseIn = basename(fname).split(".")[0]
    if not isdir(outDir):
        os.makedirs(outDir)

    print("Processing %s to output directory %s" % (fname, outDir))
    ofhs = {}
    data = []

    useGlobal = False
    globalMin, globalMax = 9999999999, -999999999

    sampleNames = None
    logFailCount = 0
    for line in open(fname):
        row = line.rstrip("\n").split("\t")

        if sampleNames is None:
            sampleNames = row[1:]
            # create output file handles
            for sn in sampleNames:
                ofn = join(outDir, sn+".bed")
                ofhs[sn] = open(ofn, "w")
            continue
        if row[0].startswith("Seq.") or "protein" in row[0] or row[0]=="":
            continue

        name = row[0]
        values = row[1:]
        assert(len(values)==len(sampleNames))
        nums = [float(x) for x in values]

        data.append( (name, nums) )

        if scale=="none":
            globalMin = min(globalMin, *nums)
            globalMax = max(globalMax, *nums)
            useGlobal = True

    logging.warning("Count not take log of negative numbers, used 0 instead, in %d cases" % logFailCount)
    if minVal is not None:
        globalMin = minVal
    if maxVal is not None:
        globalMax = maxVal

    if useGlobal or minVal or maxVal:
        globalSpan = globalMax-globalMin
        print("Global min/max/span values:", globalMin, globalMax, globalSpan)

    nameVals = defaultdict(list)

    for name, nums in data:
        # calculate the summary on the raw values
        # store the sum of all values
        if name in nameVals:
            logging.warning("%s already seen before. Will use the mean of all values. Check the input file." % name)
        if sumFunc=="sum":
            sumVal = sum(nums)
        elif sumFunc=="respFreq":
            sumVal = calcRespFreq(sampleNames, nums)
            assert(sumVal < 1.0)
        nameVals[name].append(sumVal)

        # first take the log
        if doLog:
            newNums = []
            for x in nums:
                if x <= 0:
                    logFailCount += 1
                    x = 0
                else:
                    x = math.log2(x)
                newNums.append(x)
            nums = newNums

        # then scale by user-provided factors
        if multNeg:
            newNums = []
            for x in nums:
                if x < 0:
                    x = x*multNeg
                else:
                    x = x*multPos
                newNums.append(x)
            nums = newNums



        if useGlobal:
            minX = globalMin
            maxX = globalMax
            span = globalSpan
            # limit the range of values to strictly globalMin - globalMax
            nums = [min(max(globalMin, x), globalMax) for x in nums]
        else:
            minX = min(nums)
            maxX = max(nums)
            span = maxX - minX

        # then bin the values and get their colors
        logging.debug("%s - numerical values: %s"% (name, nums))
        ratios = [(x - minX)/span for x in nums]
        logging.debug("%s - scaled values: %s"% (name, ratios))
        ratios = [min(0.999999, x) for x in ratios] # make sure that 1.0 doesn't appear
        binIdxList = [int(float(x) / colSteps) for x in ratios]
        logging.debug("%s - bin indices: %s"% (name, binIdxList))
        colors = [palStrs[x] for x in binIdxList]

        if name in beds:
            bed = beds[name]
        else:
            newName = name.split("_")[0]
            if not newName in beds:
                logging.error("No entry in BED file for sample name %s" % repr(name))
                continue
            else:
                bed = beds[newName]

        for sampleName, rgbCol in zip(sampleNames, colors):
            bed[8] = rgbCol
            ofh = ofhs[sampleName]
            ofh.write("\t".join(bed))
            ofh.write("\n")

    # close the bed files
    bedFnames = {}
    for sampleName, ofh in ofhs.items():
        bedFnames[sampleName] = ofh.name
        ofh.close()

    # reorder sample names with optimal leaf
    sampleOrder = sampleNames
    if doOrder:
        sampleOrder = optimalLeaf(fname)
        print(sampleOrder)
        print(sampleNames)
        assert(set(sampleOrder)==set(sampleNames))

    # remove a few samples if user wants that
    if delSamples and len(delSamples)>0:
        logging.info("Filtering: %d sample names" % len(sampleOrder))
        ros = []
        for reStr in delSamples:
            ro = re.compile(reStr)
            ros.append(ro)

        newSampleNames = []
        for sn in sampleOrder:
            skipThis = False
            for ro in ros:
                if ro.match(sn) is not None:
                    logging.info("Removed sample %s due to --del" % sn)
                    skipThis = True
                    break
            if not skipThis:
                newSampleNames.append(sn)
        sampleOrder = newSampleNames
        logging.info("After filtering: %d sample names" % len(sampleOrder))

    bwFname = join(outDir, "allSum.bw")

    if sumFunc is not None:
        nameValSum = {}
        for name, vals in nameVals.items():
            nameValSum[name] = sum(vals)/float(len(vals))
        makeBigWig(beds, nameValSum, chromSizes, bwFname)

    bbFnames = toBigBed(bedFnames, sampleOrder, chromSizes)

    makeTrackDb(bbFnames, sampleOrder, outDir, baseIn)

# ----------- main --------------
def main():
    global bbDir
    global pal
    args, options = parseArgs()

    bedFname = args[0]
    matrixFname = args[1]
    chromSizes = args[2]
    outDir = args[3]

    bbDir = options.bbDir
    scale = options.scale
    cmap  = options.cmap
    minVal = options.min
    maxVal = options.max
    mult = options.mult
    palRange = options.palRange
    doLog = options.doLog
    doOrder = options.doOrder
    delSamples = options.delSamples
    sumFunc = options.bigWig

    if cmap!="viridis":
        from matplotlib import cm
        import numpy as np
        if palRange:
            palStart, palEnd = palRange.split(":")
            palStart = float(palStart)
            palEnd = float(palEnd)
        else:
            palStart = 0.0
            palEnd = 1.0
        x = np.linspace(palStart, palEnd, binCount+1) # [0.0, 0.1, 0.2 ... 1.0]
        pal = (cm.get_cmap(cmap)(x)*255)[:, 0:3].astype(int).tolist()

    beds = parseBed(bedFname)
    bigHeat(beds, matrixFname, chromSizes, scale, minVal, maxVal, mult, doLog, doOrder, delSamples, sumFunc, outDir)

main()
