# convert avada to bed
import gzip
#import unidecode # install with 'pip install unidecode'
alreadyDone = set()
for line in gzip.open("avada_v1.00_2016.vcf.gz", "rb"):
    # strange hack, but was necessary
    line = line.replace(b"\xe2\xac\x8e", b">") # very weird - what type of encoding was this before? 
    ucLine = line.decode("utf8")
    if ucLine.startswith("#"):
        continue
    row = ucLine.rstrip("\n").split("\t")
    # ['1', '45797228', '.', 'C', 'T', '.', '.', 'PMID=23361220;GENE_SYMBOL=MUTYH;ENSEMBL_ID=ENSG00000132781;ENTREZ_ID=4595;REFSEQ_ID=NM_001128425.1;STRAND=-;ORIGINAL_VARIANT_STRING=c.1187G4A']
    chrom, start, qual, ref, alt, dot1, dot2, otherStr = row
    otherParts = otherStr.split(';')
    annots = {}
    for p in otherParts:
        key, val = p.split("=")
        annots[key] = val

    # if a string from a paper has already been mapped to the same position and gene, then don't 
    # convert it again. Variants can be mapped through two similar transcripts, to the same location.
    annotKey = (chrom, start, annots["PMID"], annots["GENE_SYMBOL"], annots["ORIGINAL_VARIANT_STRING"])
    if annotKey in alreadyDone:
        #print("annot %s already done" % repr(annotKey))
        continue
    alreadyDone.add(annotKey)

    # bed9
    chrom = "chr"+chrom
    start = int(start)-1 # VCF is 1-based. Argh.
    end = start+len(ref)
    name = annots["ORIGINAL_VARIANT_STRING"]
    score = "0"
    strand = annots["STRAND"]
    thickStart = start
    thickEnd = end
    itemRgb = "0"
    # extraFields:
    pmid = annots["PMID"]
    variant = name # easier for users, so variant is together with gene / paper
    geneSym = annots["GENE_SYMBOL"]
    ensId = annots["ENSEMBL_ID"]
    entrez = annots["ENTREZ_ID"]
    refseq = annots["REFSEQ_ID"]
    
    bed = [chrom, str(start), str(end), name, score, strand, str(thickStart), str(thickEnd), itemRgb, \
            geneSym, variant, ensId, entrez, refseq, pmid]

    print("\t".join(bed))

