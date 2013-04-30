#!/usr/bin/env python2.7
# a script to lift uniprot annotations created by the publications uniprot parser
# to hg19 and create a bigbed file for them

from os.path import join, isfile
from os import system
from collections import defaultdict

# import the publications libraries
import sys, gzip, optparse
sys.path.append("/cluster/home/max/projects/pubs/tools/lib")
import maxCommon, pubConf

# CONSTANTS ---
taxonId = "9606"
db = "hg19"
liftFname = "uniProtToHg19.psl"
# -------------

pmidBlackList = set(["17344846", "15489334", "16959974", "14702039", "17974005"])

# by default, only output identifiers, not html links to them
# html links are good for custom tracks
# identifers are good for local spMut tracks
createLinks = False

urls = {"var": "http://web.expasy.org/cgi-bin/variant_pages/get-sprot-variant.pl?",
        "uniProt" : "http://www.uniprot.org/uniprot/",
        "pubmed" : "http://www.ncbi.nlm.nih.gov/pubmed/"
        }

def htmlLink(urlType, accs):
    if createLinks:
        strList = []
        for acc in accs:
            strList.append('<a href="%s%s">%s</a>' % (urls[urlType], acc, acc))
        return ", ".join(strList)
    else:
        return ",".join(accs)

def run(cmd):
    ret = system(cmd)
    if ret!=0:
        print "Could not run %s" % cmd
        sys.exit(1)

threeToOne = \
    {'Cys': 'C', 'Asp': 'D', 'Ser': 'S', 'Gln': 'Q', 'Lys': 'K',
     'Ile': 'I', 'Pro': 'P', 'Thr': 'T', 'Phe': 'F', 'Asn': 'N', 
     'Gly': 'G', 'His': 'H', 'Leu': 'L', 'Arg': 'R', 'Trp': 'W', 
     'Ala': 'A', 'Val':'V',  'Glu': 'E', 'Tyr': 'Y', 'Met': 'M'}

oneToThree = dict([[v,k] for k,v in threeToOne.items()])

def aaToLong(seq):
    res = []
    for aa in seq:
        longAa = oneToThree.get(aa, aa)
        if longAa==aa:
            print "unknown iupac", aa
        res.append(longAa)
    return "-".join(res)

featTypeColors = { "modified residue" : "200,200,0"
}


# ------ MAIN ------
if __name__ == '__main__':
    parser = optparse.OptionParser("usage: %prog [options] - lift uniprot variant annotations to genome") 
    parser.add_option("", "--annot", dest="annot", action="store_true", help="lift other, non-variant annotations") 
    (options, args) = parser.parse_args()

    if options.annot:
        outFname = "uniprotAnnotations."+db
    else:
        outFname = "uniprotMutations."+db

    mutData = defaultdict(list)

    cmd = "rm -rf work; mkdir work"
    run(cmd)

    # create seq sizes for all uniprot sequences of this species
    uniprotFa = join(pubConf.dbRefDir, "uniprot.%s.fa" % taxonId)
    uniprotFaGz = uniprotFa+".gz"
    if not isfile(uniprotFa):
        print "uncompressing uniprot gz fa"
        open(uniprotFa, "w").write(gzip.open(uniprotFaGz).read())
    if not isfile(uniprotFa):
        raise Exception("Not found: %s" % uniprotFa)

    cmd = "faSize %s -detailed | gawk '{$2=$2*3; print}'> work/chromSizes" % uniprotFa
    run(cmd)
    # get uniprot IDs 
    speciesAccs = set([line.split()[0] for line in open("work/chromSizes")])

    # read data, write bed to file
    # this annotates mutations on the protein sequence
    print "converting tab to bed"
    ofh = open("work/temp.bed", "w")
    uniProtMutFname = join(pubConf.dbRefDir, "uniprot.mut.tab")
    for mut in maxCommon.iterTsvRows(uniProtMutFname):
        if mut.acc not in speciesAccs:
            continue
        if options.annot and mut.featType in ["mutagenesis site", "sequence variant"]:
            continue
        mutName = mut.featType.replace(" ", "_")+":"+mut.acc+":"+mut.origAa+mut.begin+mut.mutAa
        mutPos = 3*(int(mut.begin)-1)
        mutPosEnd = 3*(int(mut.end)-1)
        if mutName not in mutData:
            ofh.write("\t".join([mut.acc, str(mutPos), str(mutPosEnd), mutName])+"\n")
        mutData[mutName].append(mut)
    ofh.close()
    
    # lift from protein sequence bed to hg19 bed
    print "lifting"
    cmd = "bedToPsl work/chromSizes work/temp.bed stdout | pslMap stdin %s stdout | pslToBed stdin stdout | sort -k1,1 -k2,2n > work/temp.lifted.bed" % liftFname
    run(cmd)

    print "adding extra fields"
    # read lifted bed and add uniprot annotations to it as additional fields
    ofh = open("%s.bed" % outFname, "w")
    count = 0
    blackCount = 0
    noInfoCount = 0
    for line in open("work/temp.lifted.bed"):
        bed = line.strip().split()
        muts = mutData[bed[3]]
        varIds = set([mut.varId for mut in muts])
        pmids = set()
        for mut in muts:
            pmids.update(mut.pmid.split(","))

        if len(pmidBlackList.intersection(pmids))==len(pmids):
            blackCount +=1
            continue

        if not options.annot and not mut.featType in ["mutagenesis site", "sequence variant"]:
            continue
        if options.annot and mut.featType in ["mutagenesis site", "sequence variant"]:
            continue

        disStatus = set([mut.disRelated for mut in muts])
        comments = [mut.comment for mut in muts if mut.comment!=""]
        # skip if it's not disease related 
        if not "disRelated" in disStatus and mut.featType=="sequence variant" \
            and len(comments)==0 and mut.disease=="":
            noInfoCount +=1
            continue
            
        diseases = list(set([mut.disease for mut in muts]))
        acc = muts[0].acc
        dbSnpIds = [mut.dbSnpId for mut in muts]

        # create shorter disease name
        firstDis = diseases[0].split("|")[0].replace("-", " ").replace(" type", "")
        disWords = firstDis.split()
        if len(disWords)==3 and disWords[2]=="of":
            disWords = disWords[:4]
        else:
            disWords = disWords[:3]
        shortDisName = " ".join(disWords)

        # set the bed name field to a disease, to the mutation or something else
        bed[3] = shortDisName
        if len(bed[3])==0:
            if mut.featType=="mutagenesis site" and mut.origAa=="":
                bed[3] = "deletion"
            else:
                bed[3] = "%s:%s->%s" % (mut.begin, mut.origAa, mut.mutAa)

        if mut.featType=="sequence variant":
            varType = "Naturally occuring sequence variant"
            bed[8] = "200,0,0"
        elif mut.featType=="mutagenesis site":
            varType = "Experimental mutation of amino acids"
            bed[8] = "0,100,0"
        else:
            bed[3] = mut.shortFeatType
            varType = mut.featType
            bed[8] = featTypeColors.get(mut.featType, "0,0,0")

        bed.append(varType)
        if not options.annot:
            bed.append(",".join(diseases))

        if not options.annot:
            # create description of mutation
            if int(mut.end)-int(mut.begin)==1:
                posStr = "position %s" % mut.begin
            else:
                posStr = "position %s-%s" % (mut.begin, mut.end)

            if mut.origAa=="":
                bed.append("%s, removal of amino acids" % (posStr))
            else:
                bed.append("%s, %s changed to %s" % \
                    (posStr, aaToLong(mut.origAa), aaToLong(mut.mutAa)))
        else:
            #  just position of feature
            if int(mut.end)-int(mut.begin)==1:
                posStr = "amino acid %s" % mut.begin
            else:
                posStr = "amino acids %s-%s" % (mut.begin, str(int(mut.end)-1))
            posStr += " on protein %s" % mut.acc
            bed.append(posStr)

        comments = [com for com in comments if com.strip()!=""]
        bed.append(", ".join(comments))
        if not options.annot:
            bed.append(htmlLink('var', varIds))
            dbSnpIds = [id for id in dbSnpIds if id.strip()!=""]
            bed.append(",".join(dbSnpIds))
        bed.append(htmlLink('uniProt', [acc]))
        bed.append(htmlLink('pubmed', pmids))
        bed[5] = "."
        bedLine = "\t".join(bed)+"\n"
        ofh.write(bedLine)
        count += 1

    print "%d features written to %s" % (count, ofh.name)
    ofh.close()

    #print "%d variants not mapped to genome" % len(notMapped)
    if options.annot:
        asFname = "bed12UniProtAnnot.as"
    else:
        asFname = "bed12UniProtMut.as"

    cmd = "bedToBigBed -as=%s %s.bed /scratch/data/%s/chrom.sizes %s.bb -type=bed12+ -tab" % (asFname, outFname, db, outFname)
    run(cmd)

    cmd = "rm -rf work"
    run(cmd)
    print "%d features written to %s.bed and %s.bed" % (count, outFname, outFname)
    print "%d features skipped because of blacklisted PMIDS" % (blackCount)
    print "%d features skipped because no info (no disease evidence, no comment)" % (noInfoCount)
