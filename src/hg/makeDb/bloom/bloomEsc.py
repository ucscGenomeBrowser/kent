# for SARS-CoV-2 the Bloom lab published a series of four papers that all use the same data format. 
# They can all be treated with the same converter
from os import system
from collections import defaultdict
from os.path import dirname, basename
import sys

chrom = "NC_045512v2"
sStart = 21562 # start of the S protein, 0-based counting
# spike ends at 25381, open-end, 0-based 
sSeq = """MFVFLVLLPLVSSQCVNLTTRTQLPPAYTNSFTRGVYYPDKVFRSSVLHSTQDLFLPFFSNVTWFHAIHVSGTNGTKRFDNPVLPFNDGVYFASTEKSNIIRGWIFGTTLDSKTQSLLIVNNATNVVIKVCEFQFCNDPFLGVYYHKNNKSWMESEFRVYSSANNCTFEYVSQPFLMDLEGKQGNFKNLREFVFKNIDGYFKIYSKHTPINLVRDLPQGFSALEPLVDLPIGINITRFQTLLALHRSYLTPGDSSSGWTAGAAAYYVGYLQPRTFLLKYNENGTITDAVDCALDPLSETKCTLKSFTVEKGIYQTSNFRVQPTESIVRFPNITNLCPFGEVFNATRFASVYAWNRKRISNCVADYSVLYNSASFSTFKCYGVSPTKLNDLCFTNVYADSFVIRGDEVRQIAPGQTGKIADYNYKLPDDFTGCVIAWNSNNLDSKVGGNYNYLYRLFRKSNLKPFERDISTEIYQAGSTPCNGVEGFNCYFPLQSYGFQPTNGVGYQPYRVVVLSFELLHAPATVCGPKKSTNLVKNKCVNFNFNGLTGTGVLTESNKKFLPFQQFGRDIADTTDAVRDPQTLEILDITPCSFGGVSVITPGTNTSNQVAVLYQDVNCTEVPVAIHADQLTPTWRVYSTGSNVFQTRAGCLIGAEHVNNSYECDIPIGAGICASYQTQTNSPRRARSVASQSIIAYTMSLGAENSVAYSNNSIAIPTNFTISVTTEILPVSMTKTSVDCTMYICGDSTECSNLLLQYGSFCTQLNRALTGIAVEQDKNTQEVFAQVKQIYKTPPIKDFGGFNFSQILPDPSKPSKRSFIEDLLFNKVTLADAGFIKQYGDCLGDIAARDLICAQKFNGLTVLPPLLTDEMIAQYTSALLAGTITSGWTFGAGAALQIPFAMQMAYRFNGIGVTQNVLYENQKLIANQFNSAIGKIQDSLSSTASALGKLQDVVNQNAQALNTLVKQLSSNFGAISSVLNDILSRLDKVEAEVQIDRLITGRLQSLQTYVTQQLIRAAEIRASANLAATKMSECVLGQSKRVDFCGKGYHLMSFPQSAPHGVVFLHVTYVPAQEKNFTTAPAICHDGKAHFPREGVFVSNGTHWFVTQRNFYEPQIITTDNTFVSGNCDVVIGIVNNTVYDPLQPELDSFKEELDKYFKNHTSPDVDLGDISGINASVVNIQKEIDRLNEVAKNLNESLIDLQELGKYEQYIKWPWYIWLGFIAGLIAIVMVTIMLCCMTSCCSCLKGCCSCGSCCKFDEDDSEPVLKGVKLHYT
"""
sLen = len(sSeq)

outFhs = {}
outMaxFhs = {}
lastSite = None

topPos = defaultdict(dict)

#allAa = "ACDEFGHIKLMNPQRSTVWY"
cutoff = 0.18

#inFname = sys.argv[1]
inFnames = [("human_sera_raw_data.csv", "Serum"),
        ("MAP_paper_antibodies_raw_data.csv", "MAB"),
        ("REGN_and_LY-CoV016_raw_data.csv", "MAB")
        ]

rbdLen = 531-331
scoreSumMab = [0.0]*(rbdLen) # for each position, the sum of all total scores for this position
scoreSumSerum = [0.0]*(rbdLen) # for each position, the sum of all total scores for this position

lastSite = None # the position of the line before this one, to check if it has changed
mabTrackCount = 0
serumTrackCount = 0
for inFname, prefix in inFnames:
    lastSite = None
    if prefix=="MAB":
        mabTrackCount += 1
    else:
        serumTrackCount += 1
    for line in open(inFname):
        row = line.rstrip().split(",")
        # ['condition', 'site', 'wildtype', 'mutation', 'mut_escape', 'site_total_escape', 'site_max_escape']
        # ['subject H (day 152)', '331', 'N', 'A', '0.00202', '0.04926', '0.007632']
        cond, site, wt, mut, mutEsc, totEsc, maxEsc = row
        if cond=="condition":
            continue
        if " + " in cond:
            cond = cond.replace(" + ", "-")

        subj = cond
        if "subject" in subj:
            subj = cond.split()[1]+"_"+cond.split()[3].rstrip(")")
            patient = cond.split()[1]
            day = cond.split()[3].rstrip(")")
            shortCond = patient+"/day"+day
        else:
            shortCond = cond

        if not subj in outFhs:
            outFhs[subj] = open("bed/"+subj+".tot.bed", "w")
            outMaxFhs[subj] = open("bed/"+subj+".max.bed", "w")

        start = sStart+(int(site)-1)*3
        end = start+3

        if float(maxEsc)>cutoff:
            topPos[(start, end, site, wt)].setdefault(mut, [])
            topPos[(start, end, site, wt)][mut].append( (shortCond, maxEsc) )

        # STOP MOST PROCESSING HERE - we need only one score per position for doing the bigWig files
        if lastSite == site:
            continue
        lastSite = site

        summPos = int(site)-1-331
        if prefix=="MAB":
            scoreSumMab[summPos] += float(totEsc)
        else:
            scoreSumSerum[summPos] += float(totEsc)

        ofh = outFhs[subj]
        outRow = [chrom, str(start), str(end), totEsc]
        ofh.write("\t".join(outRow))
        ofh.write("\n")

        ofh = outMaxFhs[subj]
        outRow = [chrom, str(start), str(end), maxEsc]
        ofh.write("\t".join(outRow))
        ofh.write("\n")


bedFh = open("bed/top.bed", "w")

for (start, end, site, wt), mutData in topPos.items():
    annots = []
    diffConds = set()
    for mut, condEscs in mutData.items():
       conds = []
       for cond, mutEsc in condEscs:
            if float(mutEsc) > cutoff:
               conds.append("%s=%s" % (cond, mutEsc))
            diffConds.add(cond)
       if len(conds)>0:
           annots.append("<b>mutation %s to %s:</b> %s" % (wt, mut, ", ".join(conds)))
    annot = "<br>".join(annots)
    start = str(start)
    end = str(end)
    bedName = sSeq[int(site)-1]+site
    sampleCount = str(len(diffConds))
    mouseOver = "Found in %s samples with escape score > %0.2f" % (sampleCount, cutoff)
    bed = [chrom, start, end, bedName, sampleCount, ".", start, end, "0", site, annot, mouseOver]
    bedFh.write("\t".join(bed))
    bedFh.write("\n")
bedFh.close()

cmd = "bedSort bed/top.bed bed/top.bed"
system(cmd)

inDir = dirname(__file__)

cmd = "bedToBigBed bed/top.bed -tab ../../chrom.sizes bbi/top.bb -as=%s/bloom2.as -type=bed8+" %inDir
system(cmd)

tracks = []
for key, ofh in outFhs.items():
    ofh.close()
    outMaxFhs[key].close()

    fname = ofh.name
    subj = basename(fname).split(".")[0]
    tracks.append(subj)

    cmd = "bedGraphToBigWig bed/%s.tot.bed ../../chrom.sizes bbi/%s.tot.bw" % (subj, subj)
    print(cmd)
    system(cmd)

    cmd = "bedGraphToBigWig bed/%s.max.bed ../../chrom.sizes bbi/%s.max.bw" % (subj, subj)
    print(cmd)
    system(cmd)


rbdStart = (sStart+1)+(331*3)
ofh = open("bed/mabAvg.wig", "w")
ofh.write("fixedStep chrom=%s start=%d step=3\n" % (chrom, rbdStart)) # wig is 1-based !!
for score in scoreSumMab:
    val = str(score / mabTrackCount)
    ofh.write(val)
    ofh.write("\n")
ofh.close()

ofh = open("bed/serumAvg.wig", "w")
ofh.write("fixedStep chrom=%s start=%d step=3\n" % (chrom, rbdStart)) # wig is 1-based !!
for score in scoreSumSerum:
    val = str(score / serumTrackCount)
    ofh.write(val)
    ofh.write("\n")
ofh.close()

cmd = "wigToBigWig bed/mabAvg.wig ../../chrom.sizes bbi/mabAvg.bw"
system(cmd)
cmd = "wigToBigWig bed/serumAvg.wig ../../chrom.sizes bbi/serumAvg.bw"
system(cmd)


# from here on it's all about trackDb generation

tdb = open("bloomEsc.ra", "w")

tdb.write("""track bloomEsc
shortLabel Bloom RBD Escape 2
longLabel Bloom Lab: S RBD-mutation antibody escape - total and maximum score per amino acid
group immuno
type bigWig
visibility hide
superTrack on show

track bloomTop
shortLabel Bloom RBD Top Mutations
longLabel Bloom Lab: S RBD-mutation antibody escape - positions with max score > 0.18
parent bloomEsc
bigDataUrl /gbdb/wuhCor1/bloomEsc/top.bb
visibility pack
type bigBed 8 +

""")

tdb.write("""track bloomEscTotal
shortLabel Bloom Total Escape
longLabel Bloom Lab: S RBD-mutation antibody escape - total escape score per amino acid
type bigWig
autoScale group
compositeTrack on
visibility dense
parent bloomEsc

""")

tracks.sort()
for track in tracks:
    tdb.write("   track %sTotal\n" % track)
    if "_" in track:
        subj, day = track.split("_")
        day = int(day)
        tdb.write("   shortLabel Serum: subj %s, day %03d\n" % (subj, day))
        tdb.write("   longLabel Bloom antibody escape - Total Score - Subject %s, Day %s\n" % (subj, day))
    else:
        tdb.write("   shortLabel %s %s total\n" % (prefix, track))
        tdb.write("   longLabel Bloom antibody escape - Total Score - %s \n" % track)

    tdb.write("   parent bloomEscTotal on\n")
    tdb.write("   visibility dense\n")
    tdb.write("   type bigWig\n")
    tdb.write("   bigDataUrl /gbdb/wuhCor1/bloomEsc/%s.tot.bw\n" % track)
    tdb.write("\n")

tdb.write("""track bloomEscMax
shortLabel Bloom Max Escape
longLabel Bloom Lab: Antibody escape - max escape score per amino acid
type bigWig
autoScale group
visibility dense
compositeTrack on
parent bloomEsc

""")

for track in tracks:
    tdb.write("   track %sMax\n" % track)

    if "_" in track:
        subj, day = track.split("_")
        day = int(day)
        tdb.write("   shortLabel Serum: subj %s, day %03d Max\n" % (subj, day))
        tdb.write("   longLabel Bloom antibody escape -  Max Score - %s \n" % track)
    else:
        tdb.write("   shortLabel %s %s\n" % (prefix, track))
        tdb.write("   longLabel Bloom antibody escape - Max Score - %s %s\n" % (prefix, track))

    tdb.write("   parent bloomEscMax on\n")
    tdb.write("   visibility dense\n")
    tdb.write("   type bigWig\n")
    tdb.write("   bigDataUrl /gbdb/wuhCor1/bloomEsc/%s.max.bw\n" % track)
    tdb.write("\n")

