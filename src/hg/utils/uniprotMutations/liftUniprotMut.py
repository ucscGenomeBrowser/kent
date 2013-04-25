#!/usr/bin/env python2.7
# a script to lift uniprot mutations output by the publications uniprot parser
# to hg19 and create a bigbed file for them

from os.path import join, isfile
from os import system
from collections import defaultdict

# import the publications libraries
import sys, gzip
sys.path.append("/cluster/home/max/projects/pubs/tools/lib")
import maxCommon, pubConf

# CONSTANTS ---
taxonId = "9606"
db = "hg19"
liftFname = "uniProtToHg19.psl"
# -------------

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

# ------ MAIN ------
if __name__ == '__main__':
    mutData = defaultdict(list)

    cmd = "rm -rf work; mkdir work"
    run(cmd)

    # create seq sizes for all uniprot sequences of this species
    uniprotFa = join(pubConf.dbRefDir, "uniprot.%s.fa" % taxonId)
    uniprotFaGz = uniprotFa+".gz"
    if isfile(uniprotFaGz):
        print "uncompressing uniprot gz file"
        open(uniprotFa, "w").write(gzip.open(uniprotFaGz).read())
    if not isfile(uniprotFa):
        raise Exception("Not found: %s" % uniprotFa)

    cmd = "faSize %s -detailed | gawk '{$2=$2*3; print}'> work/chromSizes" % uniprotFa
    run(cmd)
    # get uniprot IDs 
    speciesAccs = set([line.split()[0] for line in open("work/chromSizes")])

    # read data, write bed to file
    # this annotates mutations on the protein sequence
    ofh = open("work/temp.bed", "w")
    uniProtMutFname = join(pubConf.dbRefDir, "uniprot.mut.tab")
    for mut in maxCommon.iterTsvRows(uniProtMutFname):
        if mut.acc not in speciesAccs:
            continue
        mutName = mut.acc+":"+mut.origAa+mut.position+mut.mutAa
        mutPos = 3*(int(mut.position)-1)
        if mutName not in mutData:
            ofh.write("\t".join([mut.acc, str(mutPos), str(mutPos+3), mutName])+"\n")
        mutData[mutName].append(mut)
    ofh.close()
    
    # lift from protein sequence bed to hg19 bed
    cmd = "bedToPsl work/chromSizes work/temp.bed stdout | pslMap stdin %s stdout | pslToBed stdin stdout | sort -k1,1 -k2,2n > work/temp.lifted.bed" % liftFname
    run(cmd)

    # read lifted bed and add uniprot annotations to it as additional fields
    ofh = open("uniprotMutations.%s.bed" % db, "w")
    count = 0
    for line in open("work/temp.lifted.bed"):
        bed = line.strip().split()
        muts = mutData[bed[3]]
        varIds = set([mut.varId for mut in muts])
        pmids = set([mut.pmid for mut in muts])
        diseases = list(set([mut.disease for mut in muts]))
        acc = muts[0].acc
        comments = [mut.comment for mut in muts]
        dbSnpIds = [mut.dbSnpId for mut in muts]

        # create shorter disease name
        firstDis = diseases[0].split("|")[0].replace("-", " ").replace(" type", "")
        disWords = firstDis.split()
        if len(disWords)==3 and disWords[2]=="of":
            disWords = disWords[:4]
        else:
            disWords = disWords[:3]
        shortDisName = " ".join(disWords)

        bed[3] = shortDisName
        bed.append(",".join(diseases))
        bed.append("position %s, amino acid %s changed to %s" % \
            (mut.position, mut.origAa, mut.mutAa))
        bed.append(", ".join(comments))
        bed.append(htmlLink('var', varIds))
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
    cmd = "bedToBigBed -as=bed12UniProtMut.as uniprotMutations.%s.bed /scratch/data/%s/chrom.sizes uniprotMutations.%s.bb -type=bed12+ -tab" % (db, db, db)
    run(cmd)

    cmd = "rm -rf work"
    run(cmd)
