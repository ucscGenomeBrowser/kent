import os
import re
import gzip
import argparse
import pandas as pd
import numpy as np

from collections import defaultdict

def get_args():

    """
    Parse command line arguments
    """

    parser = argparse.ArgumentParser(description="Method to create track for escape mutations")
    parser.add_argument("-xlsx", help="file containing all the data")
    parser.add_argument("-pid", help="pep to number", default="prot_names_pids_8.txt")
    parser.add_argument("-gb_tools", help="path to gb_tools", default="./")
    args = parser.parse_args()

    return args

def read_pid(args):

    inputfilehandler = open(args.pid, 'r')
    pid = {}
    aaid = {}
    nucid = {}
    for line in inputfilehandler:
        line = line.strip()
        fields = line.split()
        peptide = fields[0]
        pid[peptide] = fields[1]
        nucid[peptide] = fields[2]
        aaid[peptide] = fields[3]
    inputfilehandler.close()

    return (pid, aaid, nucid)

def get_start_pos(peptide, pid, aaid, nucid):

    first_eight = ''.join(list(peptide)[0:8])
    if first_eight in pid:
        return nucid[first_eight]

    return -1

def main(args):

    (pid, aaid, nucid) = read_pid(args)

    cd8_epitopes = pd.read_excel(args.xlsx,
                                 skiprows=0,
                                 header=0,
                                 index_col=None)

    print (cd8_epitopes.columns)

    outfiletag = 'escape_mutations'
    beddetailfilename = outfiletag+'.beddetail'
    bedfilename = outfiletag+'.bed'
    bbfilename = outfiletag+'.bb'

    #print (cd8_epitopes['Probable Infection Location'])
    #print (cd8_epitopes['Gene'])
    #print (cd8_epitopes['Position of Mutation'])
    #print (cd8_epitopes['AA Change'])
    #print (cd8_epitopes['Codon Change'])
    #print (cd8_epitopes['Wildtype Sequence'])
    #print (cd8_epitopes['Mutant Sequence 1'])
    #print (cd8_epitopes['Mutant Sequence 2'])

    wt_mt = defaultdict(list)
    mutations = []

    beddetailfilehandler = open(beddetailfilename, 'w')
    for i in range(0, len(cd8_epitopes['Position of Mutation'])):

        chrom = "NC_045512v2"
        reserved = 0
        score = 1000
        strand = '+'
        pom = cd8_epitopes['Position of Mutation'][i]
        gene = cd8_epitopes['Gene'][i]
        pil = cd8_epitopes['Probable Infection Location'][i]
        aa_change = cd8_epitopes['AA Change'][i]
        c_change = cd8_epitopes['Codon Change'][i]

        if gene+'_'+c_change+'_'+aa_change not in mutations:
            mutations.append(gene+'_'+c_change+'_'+aa_change)

        if ';' not in cd8_epitopes['Wildtype Sequence'][i]:
            chromStart = get_start_pos(cd8_epitopes['Wildtype Sequence'][i], pid, aaid, nucid)
            if chromStart != -1:
                chromEnd = str(len(list(cd8_epitopes['Wildtype Sequence'][i]))*3+int(chromStart))
                thickStart = str(chromStart)
                thickEnd = str(chromEnd)
                wt_pep = cd8_epitopes['Wildtype Sequence'][i]
                mt_pep = cd8_epitopes['Mutant Sequence 1'][i]

                if wt_pep not in wt_mt:
                    wt_mt[wt_pep].append(mt_pep)
                else:
                    if mt_pep in wt_mt[wt_pep]:
                        continue

                beddetailfilehandler.write(chrom+'\t'+
                        str(chromStart)+'\t'+
                        str(chromEnd)+'\t'+
                        wt_pep+'\t'+
                        str(score)+'\t'+
                        strand+'\t'+
                        thickStart+'\t'+
                        thickEnd+'\t'+
                        str(pom)+'\t'+
                        str(gene)+'\t'+
                        str(pil)+'\t'+
                        aa_change+'\t'+
                        c_change+'\t'+
                        mt_pep+"\n")
        else:
            wt_pep = cd8_epitopes['Wildtype Sequence'][i]
            wt1_pep = wt_pep.split(';')[0]
            wt2_pep = wt_pep.split(';')[1]
            mt1_pep = cd8_epitopes['Mutant Sequence 1'][i]
            mt2_pep = cd8_epitopes['Mutant Sequence 2'][i]

            chromStart = get_start_pos(wt1_pep, pid, aaid, nucid)
            if chromStart != -1:
                chromEnd = str(len(list(wt1_pep))*3+int(chromStart))
                thickStart = chromStart
                thickEnd = chromEnd

                if wt1_pep not in wt_mt:
                    wt_mt[wt_pep].append(mt_pep)
                else:
                    if mt1_pep in wt_mt[wt1_pep]:
                        continue

                beddetailfilehandler.write(chrom+'\t'+
                        str(chromStart)+'\t'+
                        str(chromEnd)+'\t'+
                        wt1_pep+'\t'+
                        str(score)+'\t'+
                        strand+'\t'+
                        thickStart+'\t'+
                        thickEnd+'\t'+
                        str(pom)+'\t'+
                        str(gene)+'\t'+
                        str(pil)+'\t'+
                        aa_change+'\t'+
                        c_change+'\t'+
                        mt1_pep+"\n")

            chromStart = get_start_pos(wt2_pep, pid, aaid, nucid)
            if chromStart != -1:
                chromEnd = str(len(list(wt2_pep))*3+int(chromStart))
                thickStart = chromStart
                thickEnd = chromEnd

                if wt2_pep not in wt_mt:
                    wt_mt[wt_pep].append(mt_pep)
                else:
                    if mt2_pep in wt_mt[wt2_pep]:
                        continue

                beddetailfilehandler.write(chrom+'\t'+
                        str(chromStart)+'\t'+
                        str(chromEnd)+'\t'+
                        wt2_pep+'\t'+
                        str(score)+'\t'+
                        strand+'\t'+
                        thickStart+'\t'+
                        thickEnd+'\t'+
                        str(pom)+'\t'+
                        str(gene)+'\t'+
                        str(pil)+'\t'+
                        aa_change+'\t'+
                        c_change+'\t'+
                        mt2_pep+"\n")


    beddetailfilehandler.close()
    print (len(mutations))

    # use gbtools to convert from beddetail to bed and bigbed
    os.system(f"bedSort {beddetailfilename} {bedfilename}")
    os.system(f"bedToBigBed {bedfilename} wuhCor1.sizes {bbfilename} -tab -type=bed9+ -as=escape_mutants.as")



if __name__ == "__main__":

    main(get_args())
