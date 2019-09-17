# convert avada to bed
import gzip
for line in gzip.open("avada_v1.00_2016.vcf.gz", "rt"):
    if line.startswith("#"):
        continue
    row = line.rstrip("\n").split("\t")
    # ['1', '45797228', '.', 'C', 'T', '.', '.', 'PMID=23361220;GENE_SYMBOL=MUTYH;ENSEMBL_ID=ENSG00000132781;ENTREZ_ID=4595;REFSEQ_ID=NM_001128425.1;STRAND=-;ORIGINAL_VARIANT_STRING=c.1187G4A']
    chrom, start, qual, ref, alt, dot1, dot2, otherStr = row
    otherParts = otherStr.split(';')
    annots = {}
    for p in otherParts:
        key, val = p.split("=")
        annots[key] = val

    # bed9
    chrom = "chr"+chrom
    start = int(start)
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
            pmid, geneSym, variant, ensId, entrez, refseq]

    print("\t".join(bed))

