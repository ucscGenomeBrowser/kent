#!/usr/bin/env python3
"""
Convert a variant results VCF from one of the Broad exome results browsers
(BipEx, Epi25, IBDseq, all built by the same "exome-results-browsers" code)
into a VCF for the varFreqs container and the varFreqs merge.

These VCFs store case/control allele counts and allele numbers per analysis
group. A record lists its groups in INFO/groups (e.g. "DEE,EPI,GGE,NAFE") and
every per-group field (ac_case, an_case, ac_ctrl, an_ctrl, p-values, ...) has
one value per group, in the same order. The header declares these fields with a
fixed Number (3 or 4) although the real length varies, so they are re-declared
as Number=. here.

This script:
  - fills in the empty INFO Description strings
  - replaces spaces in "consequence" with "_" (VCF INFO must not contain spaces)
  - takes the counts of the main group (all cases vs. all controls, e.g. EPI
    for Epi25, IBD for IBDseq, meta for BipEx) and adds
        AC_CASE, AN_CASE, AF_CASE   cases
        AC_CTRL, AN_CTRL, AF_CTRL   controls
        AC, AN, AF                  cases + controls
  - for every --sub group (disease subtype), adds AC_<G>, AN_<G>, AF_<G> from
    that group's case counts. The controls are the same for all groups.
  - drops records without any counts. The releases contain variants that were
    annotated but not in any analysis group (no groups, no ac/an fields).
  - records whose main group is missing keep their annotation but get no
    AC/AN/AF summary fields; they are counted and reported.

Usage: erbToVcf.py in.vcf.bgz out.vcf MAINGROUP [SUBGROUP,SUBGROUP,...]
"""

import gzip
import re
import sys

DESC = {
    # annotation
    "hgvsp": "HGVS protein change",
    "hgvsc": "HGVS coding change",
    "consequence": "Consequence on the canonical transcript",
    "gene_id": "Ensembl gene ID",
    "mpc": "MPC missense deleteriousness score",
    "alpha_missense": "AlphaMissense pathogenicity score",
    "misfit_s": "MisFit-S missense selection coefficient",
    "pop_eve": "popEVE missense score",
    "misrank_percentile": "Percentile of the combined MPC, AlphaMissense and MisFit-S rank (MeanMis)",
    "polyphen": "PolyPhen-2 prediction",
    "cadd": "CADD score",
    "revel": "REVEL score",
    "splice_ai": "SpliceAI score",
    "sift": "SIFT score",
    "worst_csq_for_variant_canonical": "Most severe consequence on the canonical transcript",
    "missense_passing": "Missense variant passes the damaging missense threshold (1=yes)",
    # per group
    "groups": "Analysis groups (case subsets) that this variant was counted in; the per-group fields list one value per group, in this order",
    "variant_id": "Variant ID, per analysis group",
    "variantID": "Variant ID, per analysis group",
    "ac_case": "Allele count in cases, per analysis group",
    "an_case": "Allele number in cases, per analysis group",
    "ac_ctrl": "Allele count in controls, per analysis group",
    "an_ctrl": "Allele number in controls, per analysis group",
    "mac": "Minor allele count",
    "estimate": "Odds ratio estimate, per analysis group",
    "chi_sq_stat": "Chi-squared statistic, per analysis group",
    "p_value": "P-value, per analysis group",
    "in_analysis": "Variant was included in the gene burden analysis (1=yes), per analysis group",
    "gnomADv4_1_genome_nfe_frq": "gnomAD v4.1 genomes non-Finnish European allele frequency, per analysis group",
    "gnomad_v4_1_genome_nfe_freq": "gnomAD v4.1 genomes non-Finnish European allele frequency, per analysis group",
    "P_meta": "Meta-analysis p-value, per analysis group",
    "BETA_meta": "Meta-analysis effect size (beta), per analysis group",
    "HetP": "Heterogeneity p-value of the meta-analysis, per analysis group",
    "best_variant_in_a_gene": "Variant with the lowest p-value in its gene (1=yes), per analysis group",
}
for batch in ["Twist", "Nextera", "Sanger_WES", "UKBB", "Sanger_WGS", "regeneron"]:
    DESC["P_" + batch] = "P-value in the %s sequencing batch, per analysis group" % batch
    DESC["BETA_" + batch] = "Effect size (beta) in the %s sequencing batch, per analysis group" % batch

SINGLE_NUMBERS = {"0", "1", "A", "R", "G"}

def parseInfo(s):
    keys, d = [], {}
    for it in s.split(";"):
        k, eq, v = it.partition("=")
        keys.append(k)
        d[k] = v if eq else True
    return keys, d

def fmtInfo(keys, d):
    return ";".join(k if d[k] is True else "%s=%s" % (k, d[k]) for k in keys)

def toInt(v):
    return None if v in (".", "", None) else int(v)

def fmtAf(ac, an):
    return "." if not an else ("%.4g" % (ac / an))

def main(inFn, outFn, mainGroup, subGroups):
    nIn = nOut = nNoCounts = nNoMain = 0
    newHdr = [
        '##INFO=<ID=AC,Number=A,Type=Integer,Description="Allele count, cases plus controls (group %s)">' % mainGroup,
        '##INFO=<ID=AN,Number=1,Type=Integer,Description="Allele number, cases plus controls (group %s)">' % mainGroup,
        '##INFO=<ID=AF,Number=A,Type=Float,Description="Allele frequency, cases plus controls (group %s)">' % mainGroup,
        '##INFO=<ID=AC_CASE,Number=1,Type=Integer,Description="Allele count in cases (group %s)">' % mainGroup,
        '##INFO=<ID=AN_CASE,Number=1,Type=Integer,Description="Allele number in cases (group %s)">' % mainGroup,
        '##INFO=<ID=AF_CASE,Number=1,Type=Float,Description="Allele frequency in cases (group %s)">' % mainGroup,
        '##INFO=<ID=AC_CTRL,Number=1,Type=Integer,Description="Allele count in controls">',
        '##INFO=<ID=AN_CTRL,Number=1,Type=Integer,Description="Allele number in controls">',
        '##INFO=<ID=AF_CTRL,Number=1,Type=Float,Description="Allele frequency in controls">',
    ]
    for g in subGroups:
        newHdr += [
            '##INFO=<ID=AC_%s,Number=1,Type=Integer,Description="Allele count in %s cases">' % (g, g),
            '##INFO=<ID=AN_%s,Number=1,Type=Integer,Description="Allele number in %s cases">' % (g, g),
            '##INFO=<ID=AF_%s,Number=1,Type=Float,Description="Allele frequency in %s cases">' % (g, g),
        ]
    perGroup = set()
    with gzip.open(inFn, "rt") as fin, open(outFn, "w") as fout:
        for line in fin:
            if line.startswith("##INFO=<ID="):
                fid = line[len("##INFO=<ID="):].split(",", 1)[0]
                num = re.search(r"Number=([^,]+)", line).group(1)
                if fid != "groups" and num not in SINGLE_NUMBERS:
                    perGroup.add(fid)
                    line = line.replace("Number=%s," % num, "Number=.,")
                if fid in DESC:
                    line = line.replace('Description=""', 'Description="%s"' % DESC[fid])
                fout.write(line)
                continue
            if line.startswith("#CHROM"):
                for h in newHdr:
                    fout.write(h + "\n")
                fout.write(line)
                continue
            if line.startswith("#"):
                fout.write(line)
                continue
            nIn += 1
            parts = line.rstrip("\n").split("\t")
            keys, d = parseInfo(parts[7])
            if "ac_case" not in d or d.get("groups", ".") == ".":
                nNoCounts += 1
                continue
            if "consequence" in d and d["consequence"] is not True:
                d["consequence"] = d["consequence"].replace(" ", "_")
            groups = d["groups"].split(",")
            vals = {}
            for f in ("ac_case", "an_case", "ac_ctrl", "an_ctrl"):
                v = d.get(f, ".").split(",")
                if len(v) != len(groups):
                    sys.exit("%s:%s: %s has %d values for %d groups" %
                             (parts[0], parts[1], f, len(v), len(groups)))
                vals[f] = dict(zip(groups, v))
            add = {}
            if mainGroup in groups:
                acCase, anCase = toInt(vals["ac_case"][mainGroup]), toInt(vals["an_case"][mainGroup])
                acCtrl, anCtrl = toInt(vals["ac_ctrl"][mainGroup]), toInt(vals["an_ctrl"][mainGroup])
                acCase, anCase = acCase or 0, anCase or 0
                acCtrl, anCtrl = acCtrl or 0, anCtrl or 0
                add["AC"] = acCase + acCtrl
                add["AN"] = anCase + anCtrl
                add["AF"] = fmtAf(add["AC"], add["AN"])
                add["AC_CASE"], add["AN_CASE"], add["AF_CASE"] = acCase, anCase, fmtAf(acCase, anCase)
                add["AC_CTRL"], add["AN_CTRL"], add["AF_CTRL"] = acCtrl, anCtrl, fmtAf(acCtrl, anCtrl)
            else:
                nNoMain += 1
            for g in subGroups:
                if g in groups:
                    ac, an = toInt(vals["ac_case"][g]) or 0, toInt(vals["an_case"][g]) or 0
                    add["AC_" + g], add["AN_" + g], add["AF_" + g] = ac, an, fmtAf(ac, an)
            for k, v in add.items():
                keys.append(k)
                d[k] = v
            parts[7] = fmtInfo(keys, d)
            fout.write("\t".join(parts) + "\n")
            nOut += 1
    print("input records: %d, output records: %d, dropped without counts: %d, "
          "kept without group %s: %d" % (nIn, nOut, nNoCounts, mainGroup, nNoMain),
          file=sys.stderr)

if __name__ == "__main__":
    if len(sys.argv) not in (4, 5):
        sys.exit(__doc__)
    subs = [g for g in sys.argv[4].split(",") if g] if len(sys.argv) == 5 else []
    main(sys.argv[1], sys.argv[2], sys.argv[3], subs)
