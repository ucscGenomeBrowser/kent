#!/usr/bin/env python3
"""Convert the MitImpact CSV (all possible chrM single-nucleotide variants with
their pathogenicity predictions) to a bed9+ file for bedToBigBed.

One bed row per CSV row, so variants in the two overlapping gene pairs
(MT-ATP8/MT-ATP6, MT-ND4L/MT-ND4) and at a few tRNA boundaries appear twice,
once per gene. Every CSV column is kept, as a string field, for the Table
Browser and the API. Only the MitImpact columns about the variant, the gene
and the effect, dbSNP, and the APOGEE 2 / nAPOGEE columns are written (KEEP);
the other columns are checked but dropped, as their licenses may not allow
redistribution (#38617).

Items are colored by the APOGEE 2 class (missense variants) or the nAPOGEE
class (rRNA and tRNA variants), unified into one field so a single filter
covers both. Variants that are in two overlapping genes (two rows at one
position and allele) are colored magenta instead, to see their effect.

Usage: mitImpactToBed.py MitImpact4_V059.csv chrM.fa out.bed [--asOut x.as]
"""

import argparse
import csv
import html
import json
import sys
from collections import Counter, OrderedDict

# CSV column -> (autoSql field name, label on the details page). The names in
# the CSV contain '-', '/' or start with a digit, which autoSql does not allow.
COLUMNS = [
    ("MitImpact_id", "mitImpactId", "MitImpact ID"),
    ("Chr", "csvChrom", "Chromosome"),
    ("Start", "csvPos", "Position|1-based"),
    ("Ref", "ref", "Reference allele"),
    ("Alt", "alt", "Alternative allele"),
    ("HGVS_genomic", "hgvs", "HGVS"),
    ("class", "regionClass", "Region type"),
    ("Gene_symbol", "gene", "Gene"),
    ("extended_annotation", "geneDesc", "Gene description"),
    ("gene_start", "geneStart", "Gene start"),
    ("gene_end", "geneEnd", "Gene end"),
    ("gene_strand", "geneStrand", "Gene strand"),
    ("gene_position", "genePos", "Position in gene"),
    ("functional_effect_general", "effect", "Effect|SnpEff, general"),
    ("functional_effect_detailed", "effectDetailed", "Effect|SnpEff"),
    ("codon_substitution", "codon", "Codon change"),
    ("ref_aa", "refAa", "Reference amino acid"),
    ("alt_aa", "altAa", "Alternative amino acid"),
    ("aa_pos", "aaPos", "Amino acid position"),
    ("Respiratory_Chain_complex", "rcComplex", "Respiratory chain complex"),
    ("Ensembl_gene_id", "ensGene", "Ensembl gene"),
    ("Ensembl_transcript_id", "ensTranscript", "Ensembl transcript"),
    ("Ensembl_protein_id", "ensProtein", "Ensembl protein"),
    ("Uniprot_id", "uniprotId", "UniProt"),
    ("Uniprot_name", "uniprotName", "UniProt name"),
    ("NCBI_Gene_Id", "ncbiGene", "NCBI Gene"),
    ("NCBI_protein_id", "ncbiProtein", "NCBI protein"),
    ("HGNC_Id_mtDNA", "hgncId", "HGNC"),
    ("OMIM_Id", "omimId", "OMIM gene"),
    ("dbSNP_157", "dbSnp", "dbSNP|build 157"),
    ("PhyloP_100V", "phyloP100", "PhyloP|100 vertebrates"),
    ("PhastCons_100V", "phastCons100", "PhastCons|100 vertebrates"),
    ("PhyloP_470Way", "phyloP470", "PhyloP|470 mammals"),
    ("PhastCons_470Way", "phastCons470", "PhastCons|470 mammals"),
    ("APOGEE2_score", "apogee2Score", "APOGEE 2 score|missense variants"),
    ("APOGEE2_probability", "apogee2Prob", "APOGEE 2 probability|of pathogenicity"),
    ("APOGEE2", "apogee2Class", "APOGEE 2 class"),
    ("nAPOGEE_score", "napogeeScore", "nAPOGEE score|rRNA and tRNA variants"),
    ("nAPOGEE_probability", "napogeeProb", "nAPOGEE probability|of pathogenicity"),
    ("nAPOGEE", "napogeeClass", "nAPOGEE class"),
    ("APOGEE1_score", "apogee1Score", "APOGEE 1 score"),
    ("APOGEE1", "apogee1", "APOGEE 1"),
    ("PolyPhen2_score", "polyphen2Score", "PolyPhen-2 score"),
    ("PolyPhen2", "polyphen2", "PolyPhen-2"),
    ("SIFT_score", "siftScore", "SIFT score"),
    ("SIFT", "sift", "SIFT"),
    ("SIFT4G_score", "sift4gScore", "SIFT4G score"),
    ("SIFT4G", "sift4g", "SIFT4G"),
    ("PROVEAN_score", "proveanScore", "PROVEAN score"),
    ("PROVEAN", "provean", "PROVEAN"),
    ("MutationAssessor_score", "mutAssessorScore", "MutationAssessor score"),
    ("MutationAssessor", "mutAssessor", "MutationAssessor"),
    ("MutationTaster_score", "mutTasterScore", "MutationTaster score"),
    ("MutationTaster", "mutTaster", "MutationTaster"),
    ("MutationTaster_model", "mutTasterModel", "MutationTaster model"),
    ("EFIN_SP_score", "efinSpScore", "EFIN SP score"),
    ("EFIN_SP", "efinSp", "EFIN SP"),
    ("EFIN_HD_score", "efinHdScore", "EFIN HD score"),
    ("EFIN_HD", "efinHd", "EFIN HD"),
    ("PANTHER_score", "pantherScore", "PANTHER score"),
    ("PANTHER", "panther", "PANTHER"),
    ("PhD-SNP_score", "phdSnpScore", "PhD-SNP score"),
    ("PhD-SNP", "phdSnp", "PhD-SNP"),
    ("LIST-S2_score", "listS2Score", "LIST-S2 score"),
    ("LIST-S2", "listS2", "LIST-S2"),
    ("ESM1b_score", "esm1bScore", "ESM1b score"),
    ("ESM1b", "esm1b", "ESM1b"),
    ("VARITY_R_score", "varityRScore", "VARITY_R score"),
    ("VARITY_R", "varityR", "VARITY_R"),
    ("VARITY_ER_score", "varityErScore", "VARITY_ER score"),
    ("VARITY_ER", "varityEr", "VARITY_ER"),
    ("SNPDryad_score", "snpDryadScore", "SNPDryad score"),
    ("SNPDryad", "snpDryad", "SNPDryad"),
    ("Mitoclass1", "mitoclass1", "Mitoclass.1"),
    ("CADD_score", "caddScore", "CADD raw score"),
    ("CADD_phred_score", "caddPhred", "CADD Phred score"),
    ("CADD", "cadd", "CADD"),
    ("AlphaMissense_score", "alphaMissenseScore", "AlphaMissense score"),
    ("AlphaMissense", "alphaMissense", "AlphaMissense"),
    ("VEST_pvalue", "vestPval", "VEST p-value"),
    ("VEST_FDR", "vestFdr", "VEST FDR"),
    ("VEST", "vest", "VEST"),
    ("MLC_score", "mlcScore", "MLC score"),
    ("MLC", "mlc", "MLC"),
    ("CAROL_score", "carolScore", "CAROL score"),
    ("CAROL", "carol", "CAROL"),
    ("Condel_score", "condelScore", "Condel score"),
    ("Condel", "condel", "Condel"),
    ("COVEC_WMV_score", "covecScore", "COVEC WMV score"),
    ("COVEC_WMV", "covec", "COVEC WMV"),
    ("Meta-SNP_score", "metaSnpScore", "Meta-SNP score"),
    ("Meta-SNP", "metaSnp", "Meta-SNP"),
    ("MtoolBox_score", "mtoolboxScore", "MToolBox score"),
    ("MtoolBox", "mtoolbox", "MToolBox"),
    ("DEOGEN2_score", "deogen2Score", "DEOGEN2 score"),
    ("DEOGEN2", "deogen2", "DEOGEN2"),
    ("PolyPhen2_transf_score", "polyphen2TransfScore", "PolyPhen-2 TransFIC score"),
    ("PolyPhen2_transf", "polyphen2Transf", "PolyPhen-2 TransFIC"),
    ("SIFT_transf_score", "siftTransfScore", "SIFT TransFIC score"),
    ("SIFT_transf", "siftTransf", "SIFT TransFIC"),
    ("MutationAssessor_transf_score", "mutAssessorTransfScore", "MutationAssessor TransFIC score"),
    ("MutationAssessor_transf", "mutAssessorTransf", "MutationAssessor TransFIC"),
    ("CHASM_pvalue", "chasmPval", "CHASM p-value"),
    ("CHASM_FDR", "chasmFdr", "CHASM FDR"),
    ("CHASM", "chasm", "CHASM"),
    ("MitoTIP_Score", "mitoTipScore", "MitoTIP score|tRNA variants"),
    ("Mitotip_Quartile", "mitoTipQuartile", "MitoTIP quartile|Q1 most likely pathogenic"),
    ("Mitotip_Count", "mitoTipCount", "MitoTIP GenBank count"),
    ("Mitotip_Percentage", "mitoTipPercent", "MitoTIP GenBank percentage"),
    ("Mitotip_Mitomap_Status", "mitoTipMitomapStatus", "MitoTIP MITOMAP status"),
    ("PONtRNA_mt-tRNA", "ponTrnaTrna", "PON-mt-tRNA tRNA"),
    ("PONtRNA_Classification", "ponTrnaClass", "PON-mt-tRNA class"),
    ("PONtRNA_ML_probability_of_pathogenicity", "ponTrnaProb", "PON-mt-tRNA probability"),
    ("PONtRNA_Prediction_of_Kondrashov_method", "ponTrnaKondrashov", "PON-mt-tRNA Kondrashov method"),
    ("MITOMAP_Disease_Status", "mitomapStatus", "MITOMAP disease status"),
    ("MITOMAP_Disease_Clinical_info", "mitomapDisease", "MITOMAP disease"),
    ("MITOMAP_Disease_Hom/Het", "mitomapHomHet", "MITOMAP reported plasmy|homoplasmic/heteroplasmic"),
    ("MITOMAP_Disease_GenBank_Freq", "mitomapDiseaseGbFreq", "MITOMAP disease GenBank frequency"),
    ("MITOMAP_Disease_GenBank_Seqs", "mitomapDiseaseGbSeqs", "MITOMAP disease GenBank sequences"),
    ("Clinvar_CLNSIG", "clinvarSig", "ClinVar significance"),
    ("Clinvar_CLNSIGCONF", "clinvarSigConf", "ClinVar conflicting classifications"),
    ("Clinvar_CLNDN", "clinvarDisease", "ClinVar condition"),
    ("Clinvar_ID", "clinvarId", "ClinVar variation"),
    ("Clinvar_ALLELEID", "clinvarAlleleId", "ClinVar allele ID"),
    ("Clinvar_CLNDISDB", "clinvarDiseaseDb", "ClinVar condition IDs"),
    ("Clinvar_CLNVI", "clinvarOtherIds", "ClinVar other IDs"),
    ("MITOMAP_General_Variants", "mitomapGeneral", "In MITOMAP general variants"),
    ("MITOMAP_General_GenBank_Freq", "mitomapGbFreq", "MITOMAP GenBank frequency"),
    ("MITOMAP_General_GenBank_Seqs", "mitomapGbSeqs", "MITOMAP GenBank sequences"),
    ("HelixMTdb_AF_hom", "helixAfHom", "HelixMTdb AF homoplasmic"),
    ("HelixMTdb_AC_hom", "helixAcHom", "HelixMTdb AC homoplasmic"),
    ("HelixMTdb_AF_het", "helixAfHet", "HelixMTdb AF heteroplasmic"),
    ("HelixMTdb_AC_het", "helixAcHet", "HelixMTdb AC heteroplasmic"),
    ("HelixMTdb_mean_ARF", "helixMeanArf", "HelixMTdb mean heteroplasmy"),
    ("HelixMTdb_max_ARF", "helixMaxArf", "HelixMTdb max heteroplasmy"),
    ("AF_hom_gnomad", "gnomadAfHom", "gnomAD AF homoplasmic"),
    ("AC_hom_gnomad", "gnomadAcHom", "gnomAD AC homoplasmic"),
    ("AF_het_gnomad", "gnomadAfHet", "gnomAD AF heteroplasmic"),
    ("AC_het_gnomad", "gnomadAcHet", "gnomAD AC heteroplasmic"),
    ("AN_gnomad", "gnomadAn", "gnomAD AN"),
    ("FILTER_gnomad", "gnomadFilter", "gnomAD filter"),
    ("61KJPN_AF", "jpn61kAf", "ToMMo 61KJPN AF"),
    ("61KJPN_AC", "jpn61kAc", "ToMMo 61KJPN AC"),
    ("61KJPN_AN", "jpn61kAn", "ToMMo 61KJPN AN"),
]

# CSV columns that are written to the bigBed. The rest of the CSV is checked only.
KEEP = set([
    "MitImpact_id", "Chr", "Start", "Ref", "Alt", "HGVS_genomic", "class", "Gene_symbol",
    "extended_annotation", "gene_start", "gene_end", "gene_strand", "gene_position",
    "functional_effect_general", "functional_effect_detailed", "codon_substitution",
    "ref_aa", "alt_aa", "aa_pos", "Respiratory_Chain_complex", "Ensembl_gene_id",
    "Ensembl_transcript_id", "Ensembl_protein_id", "Uniprot_id", "Uniprot_name",
    "NCBI_Gene_Id", "NCBI_protein_id", "HGNC_Id_mtDNA", "OMIM_Id", "dbSNP_157",
    "APOGEE2_score", "APOGEE2_probability", "APOGEE2",
    "nAPOGEE_score", "nAPOGEE_probability", "nAPOGEE",
])
OUTCOLUMNS = [c for c in COLUMNS if c[0] in KEEP]

OVERLAPCOLOR = "255,0,255"

# predictors for the details page table (not used any more, see KEEP): (name, score column, prediction column)
PREDICTORS = [
    ("APOGEE 1", "APOGEE1_score", "APOGEE1"),
    ("AlphaMissense", "AlphaMissense_score", "AlphaMissense"),
    ("CADD (Phred)", "CADD_phred_score", "CADD"),
    ("CAROL", "CAROL_score", "CAROL"),
    ("CHASM (p-value)", "CHASM_pvalue", "CHASM"),
    ("Condel", "Condel_score", "Condel"),
    ("COVEC WMV", "COVEC_WMV_score", "COVEC_WMV"),
    ("DEOGEN2", "DEOGEN2_score", "DEOGEN2"),
    ("EFIN HD", "EFIN_HD_score", "EFIN_HD"),
    ("EFIN SP", "EFIN_SP_score", "EFIN_SP"),
    ("ESM1b", "ESM1b_score", "ESM1b"),
    ("LIST-S2", "LIST-S2_score", "LIST-S2"),
    ("Meta-SNP", "Meta-SNP_score", "Meta-SNP"),
    ("Mitoclass.1", None, "Mitoclass1"),
    ("MLC", "MLC_score", "MLC"),
    ("MToolBox", "MtoolBox_score", "MtoolBox"),
    ("MutationAssessor", "MutationAssessor_score", "MutationAssessor"),
    ("MutationAssessor TransFIC", "MutationAssessor_transf_score", "MutationAssessor_transf"),
    ("MutationTaster", "MutationTaster_score", "MutationTaster"),
    ("PANTHER", "PANTHER_score", "PANTHER"),
    ("PhD-SNP", "PhD-SNP_score", "PhD-SNP"),
    ("PolyPhen-2", "PolyPhen2_score", "PolyPhen2"),
    ("PolyPhen-2 TransFIC", "PolyPhen2_transf_score", "PolyPhen2_transf"),
    ("PON-mt-tRNA", "PONtRNA_ML_probability_of_pathogenicity", "PONtRNA_Classification"),
    ("PROVEAN", "PROVEAN_score", "PROVEAN"),
    ("SIFT", "SIFT_score", "SIFT"),
    ("SIFT TransFIC", "SIFT_transf_score", "SIFT_transf"),
    ("SIFT4G", "SIFT4G_score", "SIFT4G"),
    ("SNPDryad", "SNPDryad_score", "SNPDryad"),
    ("VARITY_ER", "VARITY_ER_score", "VARITY_ER"),
    ("VARITY_R", "VARITY_R_score", "VARITY_R"),
    ("VEST (p-value)", "VEST_pvalue", "VEST"),
]

# allele frequencies for the details page table:
# (database, AF column, AC column, function returning the extra details text)
FREQS = [
    ("gnomAD v3.1, homoplasmic", "AF_hom_gnomad", "AC_hom_gnomad",
        lambda r: joinNonEmpty(["AN " + r["AN_gnomad"] if r["AN_gnomad"] else "",
                                "filter " + r["FILTER_gnomad"] if r["FILTER_gnomad"] else ""])),
    ("gnomAD v3.1, heteroplasmic", "AF_het_gnomad", "AC_het_gnomad",
        lambda r: "AN " + r["AN_gnomad"] if r["AN_gnomad"] else ""),
    ("HelixMTdb, homoplasmic", "HelixMTdb_AF_hom", "HelixMTdb_AC_hom", lambda r: ""),
    ("HelixMTdb, heteroplasmic", "HelixMTdb_AF_het", "HelixMTdb_AC_het",
        lambda r: joinNonEmpty(["mean heteroplasmy " + r["HelixMTdb_mean_ARF"] if r["HelixMTdb_mean_ARF"] else "",
                                "max " + r["HelixMTdb_max_ARF"] if r["HelixMTdb_max_ARF"] else ""])),
    ("ToMMo 61KJPN", "61KJPN_AF", "61KJPN_AC",
        lambda r: "AN " + r["61KJPN_AN"] if r["61KJPN_AN"] else ""),
    ("MITOMAP GenBank", "MITOMAP_General_GenBank_Freq", "MITOMAP_General_GenBank_Seqs", lambda r: ""),
]

# APOGEE 2 and nAPOGEE spell their classes differently; one spelling for both
CLASSNORM = {
    "Benign": "Benign", "benign": "Benign",
    "Likely-benign": "Likely benign", "likely benign": "Likely benign",
    "VUS-": "VUS-", "VUS": "VUS", "VUS+": "VUS+",
    "Likely-pathogenic": "Likely pathogenic", "likely pathogenic": "Likely pathogenic",
    "Pathogenic": "Pathogenic", "pathogenic": "Pathogenic",
}

NOTSCORED = "Not scored"

# ordered from benign to pathogenic; also the order of the filter menu
CLASSCOLORS = OrderedDict([
    ("Benign", "0,107,27"),
    ("Likely benign", "0,170,60"),
    ("VUS-", "130,170,40"),
    ("VUS", "120,120,120"),
    ("VUS+", "210,140,0"),
    ("Likely pathogenic", "240,80,0"),
    ("Pathogenic", "200,0,0"),
    (NOTSCORED, "190,190,190"),
])

def joinNonEmpty(parts, sep=", "):
    return sep.join(p for p in parts if p)

def jsonTable(header, rows):
    """Encode rows for the detailsDynamicTable trackDb setting. See
    episignatures/methaDoryToBed.py: hgc.js makes one table row per key, the
    keys are not drawn, non-integer keys keep their order."""
    out = OrderedDict()
    for i, row in enumerate([header] + rows):
        out["r%03d" % i] = row
    return json.dumps(out, ensure_ascii=True, separators=(",", ":"))

def predictorJson(r):
    rows = []
    for name, scoreCol, predCol in PREDICTORS:
        score = r[scoreCol] if scoreCol else ""
        pred = r[predCol].replace("_", " ")
        if score or pred:
            rows.append([name, score, pred])
    if not rows:
        return ""
    return jsonTable(["Predictor", "Score", "Prediction"], rows)

def freqJson(r):
    rows = []
    for name, afCol, acCol, detailFunc in FREQS:
        af, ac = r[afCol], r[acCol]
        if af or ac:
            rows.append([name, af, ac, detailFunc(r)])
    if not rows:
        return ""
    return jsonTable(["Database", "Allele frequency", "Allele count", "Details"], rows)

def aaChangeStr(r):
    """ Protein change like p.L156R, p.L156= or p.M1*. MitImpact fills aa_pos
    only for missense and some start codon variants, so for the others the
    codon number comes from the position in the gene, which gives the same
    number as aa_pos for all rows that have one (checked on V059). """
    ref, alt = r["ref_aa"], r["alt_aa"]
    if not (ref and alt):
        return ""
    aaPos = r["aa_pos"]
    if not aaPos:
        aaPos = str((int(r["gene_position"]) - 1) // 3 + 1)
    elif aaPos != str((int(r["gene_position"]) - 1) // 3 + 1):
        sys.exit("aa_pos %s does not match the gene position of %s" % (aaPos, r["MitImpact_id"]))
    if ref == alt:
        alt = "="
    return "p.%s%s%s" % (ref, aaPos, alt)

def mouseOver(r, name, acmgClass, model, prob, overlap):
    e = html.escape
    lines = []
    gene = r["Gene_symbol"]
    effect = r["functional_effect_detailed"].replace("_", " ").replace("&", ", ")
    aa = aaChangeStr(r)
    lines.append("<b>%s</b> %s" % (e(name), e(joinNonEmpty([gene, effect, aa], " "))))
    if model:
        lines.append("<b>%s:</b> %s (probability %s)" % (model, e(acmgClass), e(prob)))
    if overlap:
        lines.append("<b>Overlapping genes:</b> listed once per gene")
    return "<br>".join(lines)

def writeAs(fname):
    fields = [
        ("string", "chrom", "Reference sequence chromosome or scaffold"),
        ("uint", "chromStart", "Start position in chromosome"),
        ("uint", "chromEnd", "End position in chromosome"),
        ("string", "name", "Variant (HGVS on NC_012920.1)"),
        ("uint", "score", "Not used, always 0"),
        ("char[1]", "strand", "Not used, always ."),
        ("uint", "thickStart", "Same as chromStart"),
        ("uint", "thickEnd", "Same as chromEnd"),
        ("uint", "reserved", "RGB color, from the APOGEE class, magenta if the genes overlap"),
        ("string", "acmgClass", "APOGEE class|APOGEE 2 or nAPOGEE"),
        ("string", "apogeeModel", "APOGEE model"),
        ("string", "apogeeProb", "APOGEE probability|of pathogenicity"),
        ("string", "aaChange", "Amino acid change"),
        ("string", "overlap", "Overlapping genes|variant is listed once per gene"),
        ("lstring", "_mouseOver", "Mouseover text"),
        ("string", "_mitImpactQuery", "MitImpact search query"),
    ]
    for csvCol, asName, label in OUTCOLUMNS:
        typ = "lstring" if asName in LONGFIELDS else "string"
        fields.append((typ, asName, label))
    with open(fname, "w") as ofh:
        ofh.write('table mitImpact\n')
        ofh.write('"MitImpact: APOGEE 2 and nAPOGEE for all possible chrM single-nucleotide variants"\n')
        ofh.write('    (\n')
        for typ, name, label in fields:
            ofh.write('    %s %s; "%s"\n' % (typ, name, label))
        ofh.write('    )\n')

# CSV columns that can be longer than 255 characters
LONGFIELDS = {"clinvarDisease", "clinvarDiseaseDb", "clinvarOtherIds"}

def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("csv", help="MitImpact CSV file")
    parser.add_argument("chrMFa", help="chrM sequence of the assembly, fasta, to check the reference alleles")
    parser.add_argument("outBed", help="output bed file")
    parser.add_argument("--asOut", help="also write the autoSql file to this name")
    args = parser.parse_args()

    if args.asOut:
        writeAs(args.asOut)

    seq = "".join(l.strip() for l in open(args.chrMFa) if not l.startswith(">")).upper()

    reader = csv.DictReader(open(args.csv, newline=""))
    expCols = [c[0] for c in COLUMNS]
    if reader.fieldnames != expCols:
        missing = set(expCols) - set(reader.fieldnames)
        extra = set(reader.fieldnames) - set(expCols)
        sys.exit("CSV columns differ from the expected ones. Missing: %s, new: %s, "
                 "or the order changed" % (sorted(missing), sorted(extra)))

    classCounts = Counter()
    overlapRows = 0
    rows = []
    csvRows = 0
    allRows = list(reader)
    # variants with two rows at one position and allele are in two overlapping genes
    pairCounts = Counter((r["Start"], r["Alt"]) for r in allRows)
    for r in allRows:
        csvRows += 1
        if r["Chr"] != "chrM":
            sys.exit("Unexpected chromosome %s in %s" % (r["Chr"], r["MitImpact_id"]))
        pos = int(r["Start"])
        if pos < 1 or pos > len(seq):
            sys.exit("Position %d outside of chrM (length %d) in %s" % (pos, len(seq), r["MitImpact_id"]))
        if seq[pos-1] != r["Ref"]:
            sys.exit("Reference allele %s of %s does not match the genome (%s) at %d" %
                     (r["Ref"], r["MitImpact_id"], seq[pos-1], pos))

        name = "m.%d%s>%s" % (pos, r["Ref"], r["Alt"])

        if r["APOGEE2"] and r["nAPOGEE"]:
            sys.exit("%s has both an APOGEE 2 and an nAPOGEE class" % r["MitImpact_id"])
        if r["APOGEE2"]:
            model, acmgClass, prob = "APOGEE 2", CLASSNORM[r["APOGEE2"]], r["APOGEE2_probability"]
        elif r["nAPOGEE"]:
            model, acmgClass, prob = "nAPOGEE", CLASSNORM[r["nAPOGEE"]], r["nAPOGEE_probability"]
        else:
            model, acmgClass, prob = "", NOTSCORED, ""
        classCounts[(model, acmgClass)] += 1

        aaChange = aaChangeStr(r)
        isOverlap = pairCounts[(r["Start"], r["Alt"])] > 1
        color = OVERLAPCOLOR if isOverlap else CLASSCOLORS[acmgClass]
        if isOverlap:
            overlapRows += 1

        query = "%d%s%%3E%s" % (pos, r["Ref"], r["Alt"])

        bed = ["chrM", str(pos-1), str(pos), name, "0", ".", str(pos-1), str(pos),
               color, acmgClass, model, prob, aaChange,
               "yes" if isOverlap else "",
               mouseOver(r, name, acmgClass, model, prob, isOverlap), query]
        for csvCol, asName, label in OUTCOLUMNS:
            val = r[csvCol]
            if "\t" in val or "\n" in val:
                sys.exit("Tab or newline in column %s of %s" % (csvCol, r["MitImpact_id"]))
            if asName not in LONGFIELDS and len(val) > 255:
                sys.exit("Column %s of %s longer than 255 characters" % (csvCol, r["MitImpact_id"]))
            bed.append(val)
        rows.append((pos, r["Alt"], bed))

    rows.sort(key=lambda x: (x[0], x[1]))
    with open(args.outBed, "w") as ofh:
        for pos, alt, bed in rows:
            ofh.write("\t".join(bed) + "\n")

    print("%d CSV rows, %d bed rows written to %s" % (csvRows, len(rows), args.outBed))
    for (model, acmgClass), n in sorted(classCounts.items()):
        print("  %-10s %-18s %d" % (model or "-", acmgClass, n))
    overlapPairs = [k for k, n in pairCounts.items() if n > 1]
    print("%d (position, allele) pairs at %d positions are in overlapping genes, %d rows, colored magenta" %
          (len(overlapPairs), len(set(k[0] for k in overlapPairs)), overlapRows))

if __name__ == "__main__":
    main()
