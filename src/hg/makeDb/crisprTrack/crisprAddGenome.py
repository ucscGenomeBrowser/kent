#!/usr/bin/env python

import logging, sys, optparse, os, shutil, glob, json, urllib2
from collections import namedtuple
from os.path import join, isdir, isfile, getsize, basename

import MySQLdb # install with "sudo apt-get install python-mysqldb"


# temp dir on local disk
tmpDir = "/dev/shm"

global debugMode
global crisprGenomeDir

# directory where the genomes are stored for the crispr tool
crisprGenomeDir = "/hive/data/outside/crisprTrack/crispor/genomes"

debugMode = False

ENSGRELEASE = "26"
# === COMMAND LINE INTERFACE, OPTIONS AND HELP ===
parser = optparse.OptionParser("""usage: %prog [options] ens|ucsc|fasta <taxId1> <taxId2> ... - add genome to crispr site

queries UCSC or ensembl database for all assemblies of genome, 
takes the newest one, downloads it, indexes it with bwa,
tries to find some gene model set and moves everything into the crispr directories 

if mode is "fasta":
- instead of taxon ID, the fasta file has to be provided as the first argument
- fasta file can be local or a http or ftp URL, can be gzipped
- downloads or copies fasta file, indexes it
- takes genome info from the --desc option

Example:
sudo crisprAddGenome.py fasta /tmp2/LAEVIS_7.1.repeatMasked.fa --desc 'xenBaseLaevis71|Xenopus laevis|X. laevis|Xenbase V7.1'
""")

parser.add_option("-d", "--debug", dest="debug", action="store_true", help="show debug messages")
parser.add_option("-a", "--auto", dest="auto", action="store_true", help="auto-mode: don't ask the user, always choose the newest assembly")
parser.add_option("-g", "--genes", dest="onlyGenes", action="store_true", help="download only gene models, not the assembly and replace the existing gene models. Also write assembly info file genomeInfo.tab")
parser.add_option("", "--geneTable", dest="geneTable", action="store", help="in UCSC mode: use another gene table than ensGene, e.g. refGene or wgEncodeGencodeBasicV22", default="ensGene")
parser.add_option("", "--symFixFile", dest="symFixFile", action="store", help="in UCSC mode: add a file with some manual geneId<tab>symbol mappings to fix the provided ones")
parser.add_option("", "--desc", dest="desc", action="store", help="in manual mode: description of genome, a | separated string of dbName, scientificName, commonName, dbVersion. Make sure to quote this string. Example: 'hg19|Homo sapiens|Human|Feb 2009 (GRCh37)'")
parser.add_option("", "--baseDir", dest="baseDir", action="store", help="baseDir for genomes, default is %default", default=crisprGenomeDir)
#parser.add_option("-f", "--file", dest="file", action="store", help="run on file") 
#parser.add_option("", "--test", dest="test", action="store_true", help="do something") 
(options, args) = parser.parse_args()

if options.debug:
    logging.basicConfig(level=logging.DEBUG)
else:
    logging.basicConfig(level=logging.INFO)
# ==== FUNCTIONs =====
def runCmd(cmd, mustRun=True):
    " wrapper around os.system that prints error messages "
    if debugMode:
        print cmd
    ret = os.system(cmd)
    if ret!=0 and mustRun:
        print "Could not run command %s" % cmd
        sys.exit(0)
    return ret
    
def sqlConnect(db):
    """ connect to ucsc sql """
    #if name=="public":
    host, user, passwd = "genome-mysql.cse.ucsc.edu", "genomep", "password"
    #elif name=="local":
        #cfg = parseHgConf()
        #host, user, passwd = cfg["db.host"], cfg["db.user"], cfg["db.password"]
    conn = MySQLdb.connect(host=host, user=user, passwd=passwd, db=db)
    return conn

def querySql(conn, query):
    " return all rows for query as namedtuples "
    cursor = conn.cursor()
    rows = cursor.execute(query)
    data = cursor.fetchall()
    colNames = [desc[0] for desc in cursor.description]
    Rec = namedtuple("MysqlRow", colNames)
    recs = [Rec(*row) for row in data]
    cursor.close()
    return recs

def selectUcscGenomes(taxIds, quiet):
    " given a list of taxIds, let the user select an assembly and return a list of dbDb rows for them "
    conn = sqlConnect("hgcentral")
    data = {}
    dbRows = []
    for taxId in taxIds:
        print "taxon ID:",taxId
        print
        query = 'SELECT * FROM dbDb WHERE taxId="%s" ORDER BY orderKey DESC' % taxId
        rows = querySql(conn, query)

        if len(rows)==0:
            print "Sorry, this taxon ID is not served by UCSC"
            continue

        choice = None
        possChoices = range(1, len(rows)+1)
        if quiet:
            choice = possChoices[-1]
        while choice not in possChoices:
            print "Please select the assembly:"
            print
            for i, row in enumerate(rows):
                print i+1, "- ", row.name, " - ", row.description
            print
            print "Type the number of the assembly and press Return:"
            print "(If you just enter Return, number %d will be used)" % possChoices[-1]
            num = raw_input()
            if num=="":
                num = str(possChoices[-1])
            if not num.isdigit():
                print "error: you did not type a number"
                continue

            num = int(num)
            if num in possChoices:
                choice = num
            else:
                print "error: not a valid number you can select, select one of", possChoices
        dbRows.append(rows[choice-1])

    return dbRows

def printMsg(msg):
    " print with some highlighting "
    print " ==== "+msg+" ==== "

def moveToDest(outFnames, finalDbDir):
    if not isdir(finalDbDir):
        os.makedirs(finalDbDir)
    for outFname in outFnames:
        finalPath = join(finalDbDir, basename(outFname))
        if isfile(finalPath):
            if not isfile(outFname):
                print "does not exist, not copying %s" % outFname
            else:
                print "already exists: copying %s to %s " % (outFname, finalPath)
                shutil.copyfile(outFname, finalPath)
        else:
            print "moving %s to crispr genome dir %s" % (outFname, finalDbDir)
            shutil.move(outFname, finalDbDir)

def writeGenomeInfo(row, addUcsc=False):
    " write basic genome assembly info to finalDbDir "
    db = row.name
    genomeInfoPath = join(crisprGenomeDir, db, "genomeInfo.tab")
    giFh = open(genomeInfoPath, "w")

    headers = list(row._fields)
    if addUcsc:
        headers.append("server")
    giFh.write("#"+"\t".join(headers))
    giFh.write("\n")

    row = [str(x) for x in row]
    giFh.write("\t".join(row))
    if addUcsc:
        giFh.write("\t"+"ucsc")
    giFh.write("\n")

    giFh.close()
    print("wrote %s" % genomeInfoPath)

def indexFasta(faTmp, db, twoBitTmp, sizesTmp):
    " index with bwa and make chrom sizes "
    printMsg("Indexing fasta with BWA")
    cmd = "bwa index %s" % faTmp
    runCmd(cmd)

    printMsg("Deleting fasta")
    cmd = "rm %s" % faTmp
    runCmd(cmd)

    printMsg("Making chrom sizes")
    cmd = "twoBitInfo %s %s" % (twoBitTmp, sizesTmp)
    runCmd(cmd)

    return sizesTmp

def fixSymFile(accToSymTmp, symFixFile):
    " take a key,val file, append the key,vals from symFixFile to it "
    data = {}
    for line in open(accToSymTmp):
        key, val = line.strip().split("\t")
        data[key] = val
    for line in open(symFixFile):
        key, val = line.strip().split("\t")
        data[key] = val

    ofh = open(accToSymTmp, "w")
    for key, val in data.iteritems():
        ofh.write("%s\t%s\n" % (key, val))
    ofh.close()


def getUcscGenomes(rows, geneTable, symFixFile):
    " download and index genomes, geneFnames can be a comma-sep string with genePredTable,geneSyms "
    checkDir = True

    for row in rows:
        db = row.name
        dbTmpDir = join(tmpDir, db)
        finalDbDir = join(crisprGenomeDir, db)
        if checkDir:
            if isdir(dbTmpDir):
                print ("there exists already a directory %s" % dbTmpDir)
                print ("are you sure there is not already a process in a different terminal that indexes this genome?")
                print ("if you are sure, remove the directory with 'rm -rf %s' and retry" % dbTmpDir)
                sys.exit(1)
            else:
                os.makedirs(dbTmpDir)

            if isdir(finalDbDir):
                print "the directory %s already exists" % finalDbDir
                print "looks like this genome has already been indexed"
                continue

        printMsg("Genome: " + db)
        printMsg("Downloading")
        twoBitTmp = join(dbTmpDir, db+".2bit")
        cmd = "wget http://hgdownload.cse.ucsc.edu/goldenpath/%s/bigZips/%s.2bit -O %s" % (db, db, twoBitTmp)
        runCmd(cmd)

        printMsg("Converting to fasta")
        faTmp = join(dbTmpDir, db+".fa")
        cmd = "twoBitToFa %s %s" % (twoBitTmp, faTmp)
        runCmd(cmd)

        sizesTmp = join(dbTmpDir, db+".sizes")
        indexFasta(faTmp, db, twoBitTmp, sizesTmp)

        accToSymTmp = join(dbTmpDir, db+".ensemblToGeneName.txt")
        if geneTable!="ensGene":
            printMsg("Downloading mapping of %s transcript IDs to gene symbols" % geneTable)
            cmd = "wget http://hgdownload.cse.ucsc.edu/goldenPath/%s/database/%s.txt.gz -O - | zcat | cut -f2,13 | sort -u > %s" % (db, geneTable, accToSymTmp)
        else:
            printMsg("Downloading ensembl transcripts to gene name table")
            cmd = "wget http://hgdownload.cse.ucsc.edu/goldenPath/%s/database/ensemblToGeneName.txt.gz -O - | zcat > %s" % (db, accToSymTmp)
        runCmd(cmd)
        assert(getsize(accToSymTmp)!=0) # no symbols - email Max!

        if symFixFile:
            fixSymFile(accToSymTmp, symFixFile)

        printMsg("Downloading gene models and converting to segments")
        segTmp = join(dbTmpDir, "%s.segments.bed" % db)
        cmd = "wget http://hgdownload.cse.ucsc.edu/goldenPath/%(db)s/database/%(geneTable)s.txt.gz -O - | gunzip -c | cut -f2- | genePredToBed stdin stdout | bedToExons stdin stdout | lstOp replace stdin %(accToSymTmp)s | sort -u | bedSort stdin stdout | bedOverlapMerge /dev/stdin /dev/stdout | bedBetween stdin /dev/stdout -a -s %(sizesTmp)s | bedSort stdin %(segTmp)s" % locals()
        runCmd(cmd)
        assert(getsize(segTmp)!=0) # no segments - email Max!

        cmd = "rm %s" % accToSymTmp
        runCmd(cmd)

        printMsg("Moving output files into crispr tool directory")
        outFnames = glob.glob(join(dbTmpDir, "*.fa.*")) # bwa output files
        outFnames.extend([segTmp, sizesTmp, twoBitTmp])
        if not isdir(finalDbDir):
            os.makedirs(finalDbDir)
        moveToDest(outFnames, finalDbDir)

        cmd = "rm -rf %s" % dbTmpDir
        runCmd(cmd)

        writeGenomeInfo(row, addUcsc=True)

def selectEnsGenomes(taxIds):
    " given a list of taxIds, get dbDb-like rows from Ensembl for them "
    # download ensembl, ensMetazoan, ensPlants species info JSONs
    # #http://rest.ensembl.org/info/species?content-type=application/json
    # create dbDb-like rows from json


    headers = ["name", "description", "nibPath", "organism", "defaultPos", "active", "orderKey", "genome", "scientificName", "htmlPath", "hgNearOk", "hgPbOk", "sourceName", "taxId", "server"]
    Rec = namedtuple("dbDb", headers)

    url1 = "http://rest.ensembl.org/info/species?content-type=application/json"
    url2 = "http://rest.ensemblgenomes.org/info/species?content-type=application/json"

    fnames = [("ensMain.json", url1), ("ensOther.json", url2)]
    rows = []
    for fname, url in fnames:
        path = join(crisprGenomeDir, fname)
        logging.info("Reading %s" % path)
        if not isfile(path):
            logging.info("Could not find %s, downloading %s" % (path, url))
            data = urllib2.urlopen(url).read()
            open(path, "w").write(data)
            
        content = open(path).read()
        orgDicts = json.loads(content)

        newOrgs = []
        for od in orgDicts["species"]:
            taxId = od["taxon_id"]
            if taxId not in taxIds:
                continue

            no = {}
            no["server"] = od["division"]
            scName = od["name"].replace("_", " ").capitalize()
            parts = scName.split()
            shortName = "".join(parts[0][:3]).title()+"".join(parts[1][:3]).title()

            no["name"] = "ens"+str(od["release"])+shortName
            no["description"] = "%s %s (%s)" % (od["division"], od["release"], od["assembly"])
            no["nibPath"] = od["release"]
            commName = od["common_name"]
            if commName==None:
                commName = scName
            no["organism"] = commName.capitalize()
            no["defaultPos"] = ""
            no["active"] = ""
            no["orderKey"] = "0"
            no["genome"] = commName.capitalize()
            no["scientificName"] = scName
            no["htmlPath"] = ""
            no["hgNearOk"] = ""
            no["hgPbOk"] = ""
            no["sourceName"] = od["assembly"]
            no["taxId"] = od["taxon_id"]
            row = [no[k] for k in headers]
            row = Rec(*row)
            rows.append(row)

    return rows

def getDownloadUrl(division, org, assembly, release, dataType):
    " get the ftp url for a given ensembl species file, dataType is fasta or gtf"
    # ftp://ftp.ensemblgenomes.org/pub/current/plants/fasta/arabidopsis_thaliana/dna/Arabidopsis_thaliana.TAIR10.23.dna_sm.toplevel.fa.gz
    org = org.replace(" ", "_").lower()
    orgCap = org.capitalize()
    division=division.lower()

    if division=="ensembl":
        url = "ftp://ftp.ensembl.org/pub/current_%(dataType)s/%(org)s/" % locals()
    else:
        divDir = division.replace("ensembl","")
        url = "ftp://ftp.ensemblgenomes.org/pub/current/%(divDir)s/%(dataType)s/%(org)s/" % locals()
        release = ENSGRELEASE # XX hard-coded, bug in ensembl gives wrong rel codes right now

    if dataType=="fasta":
        url += "dna/"

    url += "%(orgCap)s.%(assembly)s" % locals()

    if division!="ensembl" or dataType!="fasta":
        url += ".%(release)s" % locals()

    if dataType=="fasta":
        url += ".dna_sm.toplevel.fa.gz"
    elif dataType=="gtf":
        url += ".gtf.gz" % locals()

    url = url.replace("_bd","") # July 2015, was necessary for a genome
    return url
        

def getEnsGenomes(genomeRows, onlyGenes):
    " download and index ensembl genomes from dbDb-like rows. "
    for row in genomeRows:
        db = row.name
        dbTmpDir = join(tmpDir, db)
        finalDbDir = join(crisprGenomeDir, db)

        if isdir(dbTmpDir):
            print ("there exists already a directory %s" % dbTmpDir)
            print ("are you sure there is not already a process in a different terminal that indexes this genome?")
            print ("if you are sure, remove the directory with 'rm -rf %s' and retry" % dbTmpDir)
            sys.exit(1)
        else:
            os.makedirs(dbTmpDir)

        if isdir(finalDbDir) and not onlyGenes:
            print "the directory %s already exists" % finalDbDir
            print "looks like this genome has already been indexed"
            continue

        sizesTmp = join(dbTmpDir, db+".sizes")
        if not onlyGenes:
            printMsg("Genome: " + db)
            printMsg("Downloading")
            faTmp = join(dbTmpDir, db+".fa")
            url = getDownloadUrl(row.server, row.scientificName, row.sourceName, row.nibPath, "fasta")
            cmd = "wget %s -O - | zcat > %s" % (url, faTmp)
            runCmd(cmd)

            printMsg("Converting to 2bit")
            twoBitTmp = join(dbTmpDir, db+".2bit")
            cmd = "faToTwoBit %s %s" % (faTmp, twoBitTmp)
            runCmd(cmd)

            indexFasta(faTmp, db, twoBitTmp, sizesTmp)
        else:
            origSizes = join(finalDbDir, db+".sizes")
            cmd = "cp %s %s" % (origSizes, sizesTmp)
            runCmd(cmd)

        # get transcript -> gene table and put in gene symbols where we have them
        printMsg("Downloading ensembl transcripts to gene name table")
        ensToGeneTmp = join(dbTmpDir, db+".ensemblToGeneName.txt")
        parts = row.scientificName.lower().split()
        martPrefix = parts[0][0]+parts[1]

        if row.server=="Ensembl":
            martName = "%s_gene_ensembl" % martPrefix
        else:
            martName = "%s_eg_gene" % martPrefix

        martOpt = ""
        if row.server!="Ensembl":
            egType = row.server.replace("Ensembl","").lower()
            martOpt = "--biomartServer=%s.ensembl.org" % egType
            martOpt += " --schema=%s_mart_%s" %  (egType, ENSGRELEASE)

        cmd = """retrBiomart %s %s ensembl_transcript_id ensembl_gene_id external_gene_name | gawk 'BEGIN {OFS="\t"; FS="\t"} {if ($3!="") {$2=$3}; print}' | cut -f1,2 > %s""" % (martOpt, martName, ensToGeneTmp)
        runCmd(cmd, mustRun=False)
        # a hack for ensG deviating again from ensg
        if getsize(ensToGeneTmp)==0:
            cmd = cmd.replace('external_gene_name', "external_gene_id")
            runCmd(cmd)
        assert(isfile(ensToGeneTmp) and getsize(ensToGeneTmp)!=0)

        # segment genome into named exons/non-exons
        printMsg("Downloading gene models and converting to segments")
        segTmp = join(dbTmpDir, "%s.segments.bed" % db)
        gtfUrl = getDownloadUrl(row.server, row.scientificName, row.sourceName, row.nibPath, "gtf")
        gpTmp = join(dbTmpDir, "%s.gp" % db)
        cmd    = """wget %(gtfUrl)s -O - | zcat | grep -v 'exon_number "-1"' | gtfToGenePred stdin stdout > %(gpTmp)s""" % locals()
        runCmd(cmd)

        # might use this for assembly hub one day
        bgPred = join(dbTmpDir, "%s.bgp" % db)
        cmd = "genePredToBigGenePred %(gpTmp)s stdout | sort -k1,1 -k2,2n > %(bgPred)s" % locals()
        runCmd(cmd)

        bbTmp = join(dbTmpDir, "%s.bb" % db)
        cmd = "bedToBigBed -type=bed12+ -as=/usr/local/bin/bigGenePred.as %(bgPred)s %(sizesTmp)s %(bbTmp)s" % locals()
        runCmd(cmd)

        cmd    = "genePredToBed %(gpTmp)s stdout | bedToExons stdin stdout | lstOp replace stdin %(ensToGeneTmp)s | sort -u | bedSort stdin stdout | bedOverlapMerge /dev/stdin /dev/stdout | bedBetween stdin /dev/stdout -a -s %(sizesTmp)s | bedSort stdin %(segTmp)s" % locals()
        runCmd(cmd)

        cmd = "rm %s" % ensToGeneTmp
        runCmd(cmd)

        printMsg("Moving output files into crispr tool directory")
        outFnames = glob.glob(join(dbTmpDir, "*.fa.*")) # bwa output files
        outFnames.extend([segTmp, sizesTmp, bbTmp, bgPred])

        if not onlyGenes:
            outFnames.append(twoBitTmp)
            os.makedirs(finalDbDir)

        moveToDest(outFnames, finalDbDir)

        cmd = "rm -rf %s" % dbTmpDir
        runCmd(cmd)

        writeGenomeInfo(row)

def selectManualGenome(taxIds, options):
    assert(len(taxIds)==1) # can only process one url at a time

    descs = options.desc.split("|")
    name, scName, commonName, description = descs
    # description : mis-nomer, description should be called "version"
    url = taxIds[0]
    server = "manual"

    headers = ["name", "description", "genome", "scientificName", "url", "server"]
    Rec = namedtuple("genomeDesc", headers)
    return [Rec(name, description, commonName, scName, url, server)]

def getFasta(genomeRows):
    """
    download fasta file and index it
    """
    for row in genomeRows:
        db = row.name
        dbTmpDir = join(tmpDir, db)
        finalDbDir = join(crisprGenomeDir, db)

        if isdir(dbTmpDir):
            print ("there exists already a directory %s" % dbTmpDir)
            print ("are you sure there is not already a process in a different terminal that indexes this genome?")
            print ("if you are sure, remove the directory with 'rm -rf %s' and retry" % dbTmpDir)
            sys.exit(1)
        else:
            os.makedirs(dbTmpDir)

        if isdir(finalDbDir):
            print "the directory %s already exists" % finalDbDir
            print "looks like this genome has already been indexed"
            continue

        sizesTmp = join(dbTmpDir, db+".sizes")

        printMsg("Genome: " + db)
        printMsg("Downloading")
        faTmp = join(dbTmpDir, db+".fa")

        # download file
        if row.url.startswith("http://") or row.url.startswith("ftp://"):
            if row.url.endswith(".gz"):
                cmd = "wget %s -O - | zcat > %s" % (row.url, faTmp)
            else:
                cmd = "wget %s -O %s" % (row.url, faTmp)
            runCmd(cmd)
        else:
            # local file, just copy
            shutil.copy(row.url, faTmp)

        printMsg("Converting to 2bit")
        twoBitTmp = join(dbTmpDir, db+".2bit")
        cmd = "faToTwoBit %s %s" % (faTmp, twoBitTmp)
        runCmd(cmd)

        indexFasta(faTmp, db, twoBitTmp, sizesTmp)

        outFnames = glob.glob(join(dbTmpDir, "*.fa.*")) # bwa output files
        outFnames.extend([sizesTmp, twoBitTmp])
        moveToDest(outFnames, finalDbDir)

        writeGenomeInfo(row)

# ----------- MAIN --------------
if args==[]:
    parser.print_help()
    exit(1)

if options.debug:
    debugMode = True

crisprGenomeDir = options.baseDir

site = args[0]
taxIds = args[1:]

print site
print taxIds

if site=="ucsc":
    genomeRows = selectUcscGenomes(taxIds, options.auto)
    getUcscGenomes(genomeRows, options.geneTable, options.symFixFile)

elif site=="ens":
    genomeRows = selectEnsGenomes(taxIds)
    getEnsGenomes(genomeRows, options.onlyGenes)

elif site=="fasta":
    genomeRows = selectManualGenome(taxIds, options)
    getFasta(genomeRows)
else:
    raise Exception("site has to be ucsc or ens")
    
    # import httplib2, sys
