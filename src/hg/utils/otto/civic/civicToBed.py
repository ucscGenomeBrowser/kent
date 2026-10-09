# /// script
# requires-python = ">=3.12"
# dependencies = [
#     "pandas",
# ]
# [tool.uv]
# exclude-newer = "2024-11-20T00:00:00Z"
# ///

"""Download CIViC DB files and convert into bigBed12 tracks

This script is meant to be run with `uv run civicToBed.py` which
will create a reproducible Python environment to run.

EXTERNAL DEPENDENCIES: this script depends on several kent binaries,
as well as data files. See the sections below with comment titles
"External Shell Command Dependencies" and "Local Data File
Dependencies"

This script compares against the prior month of data and checks to see
that there's not a drop of more than 10% of records, or and addition
of greater than 100%.

The primary database entry that has a genomic location is a Variant,
which is in the VariantSummaries table.

There are three types of "features" in CIViC in the VariantSummaries
table:

1. Gene features - point mutations, insertions, deletions,
   amplification, overexpression, etc. Results in a single BED entry

2. Fusion features - a structural variant creating a gene
   fusion. Results in two BED entries, one for each fusion
   partner. Each BED entry will consist of a thick part for the fused
   exon, and a thin part for the intron where the fusion takes place.

   The website shows genomic locations for the exon of interest in
   both hg38 and hg19, but these coordinates do not make it to the
   downloadable TSVs. Instead, an ENST identifier plus an exon index
   are used. There are a few transcript identifiers that are not
   located in our GENCODE transcripts, unfortunately.

3. Factor features - (Ignored) complex cellular states without a
   genomic locus, such as kataegsis or a methylation signature.

Additionally, there several other tables of interest that add
disease and therapy information to each variant:

- MolecularProfile - a summary of a set (possibly a singleton) of
  various variants that go together; example 1: BCR-ABL gene fusions
  with a point mutation in ABL1 p.T315I, example 2: KRAS G12 mutation
  when there's not a methylation signature. This is the logical
  structure that actually gets assigned evidence from
  ClinicalEvidenceSummaries or AssertionSummaries

- ClinicalEvidenceSummaries - a statement made in
  papers/abstracts/trials that connects a molecular profile to a
  disease or prognosis or therapy.

- AssertionSummary - an about a molecular profile that's an editor's
  summary of mulitple bits of clinical evidence.

"""

from collections import defaultdict
from contextlib import closing
from copy import deepcopy
import dataclasses
import datetime
import logging
import os
import ssl
import subprocess
from typing import Callable, Final, Generator, Sequence
import urllib.request

import numpy as np
import pandas as pd

##
## External Shell Command Dependencies
##
BED_TO_BIG_BED_CMD: Final = "bedToBigBed"
LIFT_OVER_CMD: Final = "liftOver"
BED_SORT_CMD: Final = "bedSort"

##
## Local Data File Dependencies
##
LIFTOVER_CHAINS: Final = [
    ["hg19", "hg38", "/hive/data/gbdb/hg19/liftOver/hg19ToHg38.over.chain.gz"],
    ["hg38", "hg19", "/hive/data/gbdb/hg38/liftOver/hg38ToHg19.over.chain.gz"],
]
GENCODE_UCSC_FN: Final = {
    "hg38": "/hive/data/genomes/hg38/bed/gencodeV47/build/ucscGenes.bed",
    "hg19": "/hive/data/genomes/hg19/bed/gencodeV47lift37/build/ucscGenes.bed",
}
CHROM_SIZES: Final = {
    "hg38": "/hive/data/genomes/hg38/chrom.sizes",
    "hg19": "/hive/data/genomes/hg19/chrom.sizes",
}

logger = logging.getLogger(__name__)
logging.basicConfig(level=logging.INFO)

DOWNLOAD_BASE_URL: Final = "https://civicdb.org/downloads"

DATA_TABLES: Final = [
    "MolecularProfileSummaries",
    "VariantSummaries",
    "ClinicalEvidenceSummaries",
    "AssertionSummaries",
]

## Maximum lengeth of a string (e.g. insAGCATGACCAG...) before
## being truncated and appended with an ellipsis
MAX_VARIANT_LENGTH: Final = 20
MAX_LONG_BED_FIELD_LENGTH: Final = 5000

## Default color for items in output bed
DEFAULT_BED_VARIANT_COLOR: Final = "0"

## Fusion partners are colored by the best CIViC evidence level of the
## variant. The colors are from the viridis palette, which gets lighter
## from A to E, so the levels can be told apart in grayscale, too.
EVIDENCE_LEVEL_COLORS: Final = {
    "A": "68,1,84",  # validated association
    "B": "59,82,139",  # clinical evidence
    "C": "33,145,140",  # case study
    "D": "94,201,98",  # preclinical evidence
    "E": "170,220,50",  # inferential association
    "": "170,170,170",  # no evidence items
}
EVIDENCE_LEVEL_NAMES: Final = {
    "A": "A, validated association",
    "B": "B, clinical evidence",
    "C": "C, case study",
    "D": "D, preclinical evidence",
    "E": "E, inferential association",
    "": "",
}

## Decorator glyph on the breakpoint end of a fusion partner:
## a black bar on a half-transparent orange band
FUSION_BAR_COLOR: Final = "0,0,0"
FUSION_BAND_COLOR: Final = "232,89,12,128"
DECORATION_AS: Final = "decoration.as"

DOWNLOAD_DIR: Final = "downloads"

# these are in percentages
MAX_ROW_INCREASE: Final = 100.0
MAX_ROW_DECREASE: Final = -10.0

# Minimum fraction of ENSEMBL transcripts found for fusions for success
MIN_ENSEMBL_TX = 0.90

##
## Not currently using this, but is the start of detecting variants
## without genomic coordinates...
##
AMINO_ACIDS: Final = "ARNDCEQGHILKMFPSTWYVX"
HGVS_STOP: Final = "fs|*"
SIMPLE_HGVS_REGEX = f"[{AMINO_ACIDS}][0-9][0-9]*[{AMINO_ACIDS}]"


def expect(condition: bool, message: str):
    if not condition:
        logging.error(f"Expectation failed: {message}")
        assert condition


def transform_gene_variant_summaries_to_loci_2024_10(df: pd.DataFrame):
    """DEPRECATED---This transformation only works on
    VariantSummary.txt up until Oct 2024 and is included only for
    historical purposes

    Stages of processing:
    1. filter out flagged variants
    2. filter out variants without a reference build
    3. Independently for locus1 and locus2, if one of chromosome,
    start, or stop is missing for the locus, make all three values missing
    4. Convert each variant into upto 2 different VarianLocus entries,
    using the variant id as the base, and adding `_1` and `_2` for
    for each locus used in the variant

    """
    logging.info(f"Initial Variants dataframe shape: {df.shape}")
    ## filter 1
    df = df.loc[(df.is_flagged == False), :]
    logging.info(f"Dataframe shape of variants after filtering flagged: {df.shape}")

    ## filter 2
    df = df.loc[~df.reference_build.isnull(), :]
    logging.info(f"Dataframe shape of variants after missing ref genome: {df.shape}")

    ## filter 3
    locus1_cols = ["chromosome", "start", "stop"]
    locus2_cols = [x + "2" for x in locus1_cols]

    print(df.keys())
    locus1_null = df.loc[:, locus1_cols].isnull().sum(1) > 0
    locus2_null = df.loc[:, locus2_cols].isnull().sum(1) > 0
    # Clear out partially entered genomic locations
    df.loc[locus1_null, locus1_cols] = np.nan
    df.loc[locus2_null, locus2_cols] = np.nan

    assert ((df[locus2_cols].isnull().sum(1) > 0) == locus2_null).all()
    assert ((df[locus1_cols].isnull().sum(1) > 0) == locus1_null).all()

    keep_cols = [
        "variant_id",
        "gene",
        "variant",
        "reference_bases",
        "variant_bases",
        "reference_build",
    ]

    vldf1 = df.loc[~locus1_null, keep_cols + locus1_cols]
    vldf2 = df.loc[~locus2_null, keep_cols + locus2_cols]
    vldf2.rename(columns=dict(zip(vldf2.keys(), vldf1.keys())), inplace=True)

    vldf1["chromosome"] = "chr" + vldf1.chromosome.astype("string")
    vldf2["chromosome"] = "chr" + vldf2.chromosome.astype("string")

    vldf1["variant_locus_id"] = vldf1.variant_id.astype("string") + "_1"
    vldf2["variant_locus_id"] = vldf2.variant_id.astype("string") + "_2"

    vldf = pd.concat((vldf1, vldf2))
    logging.info(
        f"Variants with a locus1 specified (1172 in Oct2024): {vldf1.shape[0]}"
    )
    logging.info(f"Variants with a locus2 specified (4 in Oct2024): {vldf2.shape[0]}")
    logging.info(
        f"Variants with both locus1 & locus2 (2 in Oct2024): {sum(~(locus1_null | locus2_null))}"
    )

    expect(
        set(vldf.reference_build) == set(["GRCh37", "GRCh38"]),
        message="Only two reference builds allowed, 'GRCh37', 'GRCh38'",
    )

    return vldf


def annotate_bed12(bed12df: pd.DataFrame, sourcedf: pd.DataFrame) -> pd.DataFrame:
    """Given a bed12 dataframe, and a fuller variant summary dataframe
    where the rows match up, transfer annotations from the variant
    summaries to make bed12+

    This is used both on the gene variant bed12 and on fusion variant
    bed12

    """
    assert bed12df.shape[0] == sourcedf.shape[0]

    cvids = sourcedf["clinvar_ids"].fillna("").replace("^NONE FOUND$", "", regex=True)
    level_names = sourcedf["best_evidence_level"].fillna("").map(EVIDENCE_LEVEL_NAMES)
    level_html = level_names.where(
        level_names == "", "<br><b>Best evidence level:</b> " + level_names
    )
    expect(
        cvids.str.match("[^0-9]").sum() == 0,
        "At least one clinvar_ids entry contains a non-numeric digit, other than 'NONE "
        "FOUND'. The parsing code must be updated to handle a non-numeric value",
    )
    return (
        bed12df.assign(
            variant_link=(
                sourcedf.variant_id.astype("str")
                + "|"
                + sourcedf.feature_name
                + " "
                + sourcedf.variant
            )
        )
        .assign(reference_build=sourcedf["reference_build"])
        .assign(allele_registry_id=sourcedf["allele_registry_id"].fillna(""))
        .assign(clinvar_id=cvids)
        .assign(last_review_date=sourcedf["last_review_date"].fillna(""))
        .assign(disease_link=sourcedf["disease_links"])
        .assign(therapies=sourcedf["therapies"])
        .assign(
            mouseOverHTML="<b>Associated therapies:</b> "
            + sourcedf["therapies"].str.replace(",", ", ")
            + "<br><b>Associated diseases:</b> "
            + sourcedf["disease_html"]
            + level_html
        )
        .assign(evidence_level=level_names)
    )
    ## These are includde sometimes for debugging purposes...
    # "variant_id": gdf.variant_id,
    # "reference_bases": gdf.reference_bases,
    # "variant_bases": gdf.variant_bases,


def extract_variant_name(gdf: pd.DataFrame) -> pd.Series:
    """
    Make a name for a variant to show in the browser, summarizing
    the genomic change (sequence change, fusion partners, expression,
    etc.)
    """
    # this got a bit ugly, but functional if statements be like that
    full_variant_string = pd.Series(
        np.where(
            gdf.feature_type == "Fusion",
            gdf.feature_name,  # fusions have very descriptive names.
            np.where(  # these are the gene type variants
                gdf.reference_bases.isnull(),
                np.where(  # empty variant bases --> variant descrption or an insertion
                    gdf.variant_bases.isnull(),
                    gdf.gene + " " + gdf.variant,
                    gdf.gene + " ins" + gdf.variant_bases,
                ),
                np.where(  # specified variant bases --> deleteion or variant
                    gdf.variant_bases.isnull(),
                    gdf.gene + " " + "del" + gdf.reference_bases,
                    gdf.gene + " " + gdf.reference_bases + ">" + gdf.variant_bases,
                ),
            ),
        )
    )

    return pd.Series(
        np.where(
            full_variant_string.str.len() > MAX_VARIANT_LENGTH,
            full_variant_string.str.slice(start=0, stop=MAX_VARIANT_LENGTH - 3) + "...",
            full_variant_string,
        )
    )


def transform_gene_variant_summaries(df: pd.DataFrame) -> pd.DataFrame:
    """Convert the gene variants from a cleaned/augmented
    VariantSummaries into bed12+

    With the November 2024 release, the second locus columns were removed.
    Fusion genes instead refer to exon locations in Ensembl transcripts.

    Stages of processing:
    1. filter out variants without a reference build
    2. filter out partially entered locus info (missing chrom, start, or end)
    3. construct a bed12+ DataFrame
    """

    ## filter 1
    df = df.loc[~df.reference_build.isnull(), :]
    logging.info(f"Dataframe shape of variants after missing ref genome: {df.shape}")

    ## filter 2
    locus1_cols = ["chromosome", "start", "stop"]

    locus1_null = df.loc[:, locus1_cols].isnull().sum(1) > 0
    # Clear out partially entered genomic locations
    df.loc[locus1_null, locus1_cols] = np.nan

    assert ((df[locus1_cols].isnull().sum(1) > 0) == locus1_null).all()

    gdf = df.loc[~locus1_null, :].reset_index(drop=True)

    bed_dict = {
        "chrom": "chr" + gdf.chromosome.astype("string"),
        "start": -1 + gdf.start.astype("Int64"),
        "end": gdf.stop.astype("Int64"),
        "name": gdf.short_variant_string,
        "score": 1,
        "strand": ".",
        "thickStart": 0,  # gdf.start.astype("Int64"),
        "thickEnd": 0,  # gdf.stop.astype("Int64"),
        "itemRgb": DEFAULT_BED_VARIANT_COLOR,
        "blockCount": 1,
        "blockSizes": gdf.stop.astype("Int64") - gdf.start.astype("Int64") + 1,
        "blockStarts": 0,  # gdf.start.astype("Int64"),
    }

    vldf = annotate_bed12(pd.DataFrame(bed_dict), gdf)

    col_has_null = vldf.isnull().sum() > 0
    col_has_null = list(col_has_null[col_has_null].index.values)
    expect(
        not col_has_null,
        f"Found null entries in variant locus bed columns: {col_has_null}",
    )

    logging.info(f"Variants with a locus specified (1172 in Oct2024): {vldf.shape[0]}")

    expected_references = set(["GRCh37", "GRCh38"])
    unexpected_references = set(vldf.reference_build) - expected_references
    expect(
        len(unexpected_references) == 0,
        message=f"Only know about genome references {expected_references}, but found {unexpected_references}",
    )

    return vldf


def split_one_to_n_map(
    df: pd.DataFrame, one_col: str, n_col: str, sep: str = ", ", value_transform=int
):
    d = {}
    for k, val_list in zip(df[one_col], df[n_col]):
        if k != k or val_list != val_list:  ## skip missing values
            continue
        d[k] = [value_transform(x) for x in val_list.split(sep)]
    return d


def parse_bed_int_list(l: str) -> list[int]:
    return [int(x) for x in l.rstrip(",").split(",")]


@dataclasses.dataclass
class Bed:
    chrom: str
    chromStart: int
    chromEnd: int
    name: str
    score: int
    strand: str
    thickStart: int
    thickEnd: int  #
    itemRgb: str
    blockCount: int
    blockSizes: list[int]
    blockStarts: list[int]

    def as_str_dict(self) -> dict[str, str]:
        def c(field, val) -> str:
            if field.type == list[int]:
                return ",".join([str(x) for x in val]) + ","
            if field.type == int:
                return str(val)
            return val

        return {
            f.name: c(f, self.__getattribute__(f.name)) for f in dataclasses.fields(Bed)
        }

    @classmethod
    def from_line(cls, line: str) -> "Bed":
        fields = dataclasses.fields(Bed)
        v = line.rstrip("\n").split("\t")
        assert len(fields) == len(v)
        return Bed(
            v[0],
            int(v[1]),
            int(v[2]),
            v[3],
            int(v[4]),
            v[5],
            int(v[6]),
            int(v[7]),
            v[8],
            int(v[9]),
            parse_bed_int_list(v[10]),
            parse_bed_int_list(v[11]),
        )


def read_bed12(fn: str) -> Generator[Bed, None, None]:
    with closing(open(fn)) as f:
        for line in f:
            yield Bed.from_line(line)


def gene_bed_to_kept_part(bed: Bed, index1: int, is_5prime: bool, name: str) -> tuple[Bed, bool] | None:
    """Given a transcript Bed, return the part of it that is kept in a
    fusion: for the 5' partner from the transcript start to the end of
    exon `index1`, for the 3' partner from the start of exon `index1` to
    the transcript end. Exons are 1-based from the 5' end of the
    transcript. Also returns True if the breakpoint is at the high
    (right) end of the returned Bed. Returns None if the exon does not
    exist in the transcript.

    >>> b = Bed("chr1", 100, 1000, "tx", 0, "+", 150, 900, "0", 3, [100, 100, 100], [0, 400, 800])
    >>> kept, bar_right = gene_bed_to_kept_part(b, 2, True, "A::B")
    >>> (kept.chromStart, kept.chromEnd, kept.blockCount, kept.blockStarts, kept.thickEnd, bar_right)
    (100, 600, 2, [0, 400], 600, True)
    >>> kept, bar_right = gene_bed_to_kept_part(b, 2, False, "A::B")
    >>> (kept.chromStart, kept.chromEnd, kept.blockStarts, kept.thickStart, bar_right)
    (500, 1000, [0, 400], 500, False)
    >>> b.strand = "-"
    >>> kept, bar_right = gene_bed_to_kept_part(b, 1, True, "A::B")
    >>> (kept.chromStart, kept.chromEnd, kept.blockCount, bar_right)
    (900, 1000, 1, False)
    """
    index1 = int(index1)
    if index1 < 1 or index1 > bed.blockCount:
        return None
    genomic_idx = index1 - 1 if bed.strand == "+" else bed.blockCount - index1
    # the 5' partner on + and the 3' partner on - keep the low end of the transcript
    keep_low = is_5prime == (bed.strand == "+")
    if keep_low:
        keep = range(0, genomic_idx + 1)
    else:
        keep = range(genomic_idx, bed.blockCount)

    starts = [bed.chromStart + bed.blockStarts[i] for i in keep]
    ends = [s + bed.blockSizes[i] for s, i in zip(starts, keep)]
    out = deepcopy(bed)
    out.name = name
    out.chromStart = starts[0]
    out.chromEnd = ends[-1]
    out.blockCount = len(starts)
    out.blockSizes = [e - s for s, e in zip(starts, ends)]
    out.blockStarts = [s - out.chromStart for s in starts]
    out.thickStart = max(bed.thickStart, out.chromStart)
    out.thickEnd = min(bed.thickEnd, out.chromEnd)
    if out.thickStart >= out.thickEnd:  # no coding part left
        out.thickStart = out.thickEnd = out.chromStart
    return out, keep_low


def transform_fusion_variants(
    df: pd.DataFrame, ensembl_bed_fn: str
) -> tuple[pd.DataFrame, pd.DataFrame]:
    """Convert the fusion type variants from VariantSummaries into a bed12+ data frame"""

    def remove_ens_version(x: str) -> str:
        return x.rpartition(".")[0] if x == x else ""

    fdf = df.loc[df["feature_type"] == "Fusion"].reset_index(drop=True)
    fdf = fdf.assign(
        upstream_tx=[remove_ens_version(x) for x in fdf["5_prime_transcript"]],
        upstream_exon=fdf["5_prime_end_exon"],
        downstream_tx=[remove_ens_version(x) for x in fdf["3_prime_transcript"]],
        downstream_exon=fdf["3_prime_start_exon"],
        reference_build="GENCODE",
    )

    eset = set(fdf.upstream_tx) | set(fdf.downstream_tx)
    ens_transcript_names = set([x for x in eset if x])  # remove "" from null transcript

    logging.info(f"Reading BED `{ensembl_bed_fn}` to get Ensembl transcripts")
    ens2bed = {}
    for b in read_bed12(ensembl_bed_fn):
        b.name = remove_ens_version(str(b.name))
        if b.name in ens_transcript_names:
            ens2bed[b.name] = b
    logging.info(
        f"Found {len(ens2bed)} transcripts from {len(ens_transcript_names)} specified in CIViC"
    )
    expect(
        float(len(ens2bed)) / len(ens_transcript_names) > MIN_ENSEMBL_TX,
        "Not enough matches in Ensembl transcripts, parser or input fix needed",
    )

    # Each partner is drawn as the part of its transcript that is kept in the
    # fusion, colored by evidence level, with a decorator glyph on the breakpoint end.
    vldfs = []
    decorations = []
    for tx_col, exon_col, is_5prime in [
        ("upstream_tx", "upstream_exon", True),
        ("downstream_tx", "downstream_exon", False),
    ]:
        valid = fdf[tx_col].isin(ens2bed) & ~fdf[exon_col].isnull()
        variants = fdf.loc[valid, :].reset_index(drop=True)
        bed12 = []
        kept_rows = []
        for i, r in enumerate(variants.itertuples()):
            tx = ens2bed[getattr(r, tx_col)]
            res = gene_bed_to_kept_part(
                tx, getattr(r, exon_col), is_5prime, r.short_variant_string
            )
            if res is None:
                logging.warning(
                    f"Fusion {r.short_variant_string}: exon {getattr(r, exon_col)} not in {tx.name}, skipped"
                )
                continue
            kept, bar_right = res
            kept.itemRgb = EVIDENCE_LEVEL_COLORS[r.best_evidence_level]
            bed12.append(kept.as_str_dict())
            kept_rows.append(i)
            decorated = f"{kept.chrom}:{kept.chromStart}-{kept.chromEnd}:{kept.name}"
            decorations.append(
                [kept.chrom, kept.chromStart, kept.chromEnd, "breakpoint", 0,
                 kept.strand, kept.chromStart, kept.chromEnd, FUSION_BAR_COLOR,
                 1, kept.chromEnd - kept.chromStart, 0, decorated, "glyph",
                 FUSION_BAND_COLOR, "BarRight" if bar_right else "BarLeft"]
            )
        variants = variants.iloc[kept_rows].reset_index(drop=True)
        vldfs.append(annotate_bed12(pd.DataFrame(bed12), variants))

    # identical features (same name and coordinates) share one decoration
    dec_df = pd.DataFrame(decorations).drop_duplicates(subset=[12])
    return pd.concat(vldfs), dec_df


def write_df_to_bed(vldf: pd.DataFrame, outfn: str) -> None:
    # Make sure no characters in a string can screw up the rest of the table...
    tsv_trans = str.maketrans("\n\t", "  ")
    with closing(open(outfn, mode="w")) as o:
        for _, v in vldf.iterrows():
            o.write("\t".join([str(x).translate(tsv_trans) for x in v]) + "\n")


def write_fusion_beds(
    fusion_dfs: dict[str, pd.DataFrame],
    out_fn_template: str = "civic.fusion.{db}.bed",
) -> dict[str, str]:
    beds = {}
    for ref, df in fusion_dfs.items():
        beds[ref] = out_fn_template.format(db=ref)
        write_df_to_bed(df, beds[ref])
    return beds


def transform_assertion_summaries(df: pd.DataFrame, mpdf: pd.DataFrame) -> pd.DataFrame:
    # Check overlap with molecular profiles
    assertions_with_mps = df["molecular_profile_id"].isin(mpdf["molecular_profile_id"])
    assertions_with_mps_p = assertions_with_mps.mean() * 100
    assertions_without_mps = str(list(df["assertion_id"][~assertions_with_mps]))
    logging.info(
        f"AssertionSummaries whose molecular_profile_id exists: {assertions_with_mps_p:.2f}%, assertion_ids with missing:{assertions_without_mps}"
    )
    expect(
        assertions_with_mps.mean() > 0.95,
        message="At least 95% of Assertions should have an existing MolecularProfile",
    )

    df["disease"] = df["disease"].where(df["disease"].isnull(), df["disease"].str.replace(',', '&#44;'))
    df["disease_html"] = "<i>" + df["disease"] + "</i>"
    doid = df["doid"].astype("Int64").astype("str")
    df["disease_link"] = doid.where(doid.notnull(), df["disease"]).where(
        doid.isnull(), doid + "|" + df["disease"]
    )
    return df


def transform_clinical_evidence(df: pd.DataFrame, mpdf: pd.DataFrame):
    evidence_with_mps = df["molecular_profile_id"].isin(mpdf["molecular_profile_id"])
    evidence_with_mps_p = evidence_with_mps.mean() * 100
    evidence_without_mps = str(list(df["evidence_id"][~evidence_with_mps]))
    logging.info(
        f"ClincialEvidenceSummaries whose molecular_profile_id exists: {evidence_with_mps_p:.2f}%, evidence_ids with missing:{evidence_without_mps}"
    )
    expect(
        evidence_with_mps.mean() > 0.95,
        message="At least 95% of Evidence should have an existing MolecularProfile",
    )

    df["disease"] = df["disease"].where(df["disease"].isnull(), df["disease"].str.replace(',', '&#44;'))
    df["disease_html"] = "<i>" + df["disease"] + "</i>"
    doid = df["doid"]
    df["disease_link"] = doid.where(doid.notnull(), df["disease"]).where(
        doid.isnull(), doid + "|" + df["disease"]
    )
    return df


def load_dataframes(table_dict: dict[str, str]) -> dict[str, pd.DataFrame]:
    """Load several dataframes.
    Input is a dict from name to the source path.
    Output is a dict from name to a Pandas DataFrame"""

    return {name: pd.read_csv(path, sep="\t", dtype={"doid": str}) for name, path in table_dict.items()}


def urlretrieve(url, filename):
    with closing(open(filename, "wb")) as outfile:
        with closing(urllib.request.urlopen(url)) as instream:
            outfile.write(instream.read())


def download_datadir(
    basedir: str,
    baseurl: str,
    dateslug: str,
    tablelist: list[str],
    overwrite: bool = True,
) -> dict[str, str]:
    dlpaths = {}

    # make directory
    dldir = os.path.join(basedir, dateslug)
    os.makedirs(dldir, exist_ok=True)

    # download files
    for tbl in tablelist:
        basefn = f"{dateslug}-{tbl}.tsv"
        dlpaths[tbl] = os.path.join(dldir, basefn)
        if overwrite or not os.path.exists(dlpaths[tbl]):
            url = f"{baseurl}/{dateslug}/{basefn}"
            logging.info(f"Downloading from {url} to {dlpaths[tbl]}")
            urlretrieve(url, dlpaths[tbl])

    return dlpaths


def first_of_month(day: datetime.date) -> datetime.date:
    return datetime.date(year=day.year, month=day.month, day=1)


def get_prior_date_slug(day: datetime.date) -> str:
    """For a given day, get the prior date slug one month before"""
    priorday = datetime.timedelta(days=-14) + first_of_month(day)
    return get_date_slug(priorday)


def get_date_slug(day: datetime.date) -> str:
    """CIViC archive data slug, the first of the month

    >>> get_date_slug(datetime.date.fromisoformat("2024-03-22"))
    '01-Mar-2024'
    """
    return first_of_month(day).strftime("%d-%b-%Y")


def diff_dfs(
    dfs: dict[str, pd.DataFrame], prior_dfs: dict[str, pd.DataFrame]
) -> dict[str, dict]:
    diffs = {}

    for k in dfs:
        diffs[k] = {
            "row_delta": dfs[k].shape[0] - prior_dfs[k].shape[0],
            "row_delta_percent": 100.0
            * (dfs[k].shape[0] - prior_dfs[k].shape[0])
            / dfs[k].shape[0],
        }

    return diffs


def download_and_load_dataframes(
    dldir: str,
    baseurl: str,
    day: datetime.date,
    overwrite: bool,
    skip_check_prior: bool,
) -> dict[str, pd.DataFrame]:
    dl_files = download_datadir(
        dldir, baseurl, get_date_slug(day), DATA_TABLES, overwrite=overwrite
    )
    dfs = load_dataframes(dl_files)
    if not skip_check_prior:
        prior_dl_files = download_datadir(
            dldir, baseurl, get_prior_date_slug(day), DATA_TABLES, overwrite=False
        )
        prior_dfs = load_dataframes(prior_dl_files)
        diffs = diff_dfs(dfs, prior_dfs)
        for name, diff in diffs.items():
            d = diff["row_delta_percent"]
            logging.info(f"table {name} changed by {d:.2f}% rows versus prior")
            expect(d > MAX_ROW_DECREASE, f"Rows in {name} decreased too much {d:.2f}%")
            expect(d < MAX_ROW_INCREASE, f"Rows in {name} increased too much {d:.2f}%")

    return dfs


def shell(cmd: str) -> bytes:
    logging.info(f"Running shell code: {cmd}")
    return subprocess.check_output(cmd, shell=True)


def cli1(argv: list[str]) -> bytes:
    logging.info(f"Running subprocess command: {argv}")
    return subprocess.check_output(argv)


def liftover_gene_variant_beds(
    vldf: pd.DataFrame,
    out_fn_template: str = "civic.gene.{db}.bed",
    dir: str = "gene_features",
) -> dict[str, str]:
    """Take a data farme of gene variant loci in bed12+ format, and
    split out by reference genome into separate bed files on
    disk. Then use `liftOver` to transfer annotations between each
    reference, and finally merge into a single bed per reference.

    Returns a dictionary keyed on reference build (hg19/hg38) to
    bed12+ file paths that include variants mapped from both genomes.

    Steps:
    1. Split variants into bed12 files based on the annotated genome
    2. sort raw variants & liftover variants to other genomes
    3. combine and sort variants for each genome

    """
    os.makedirs(dir, exist_ok=True)

    step1_tmpl = os.path.join(dir, "temp01.unsorted." + out_fn_template)
    splitfiles = {
        "GRCh37": open(step1_tmpl.format(db="hg19"), "w"),
        "GRCh38": open(step1_tmpl.format(db="hg38"), "w"),
    }
    tsv_trans = str.maketrans("\n\t", "  ")
    for index, v in vldf.iterrows():
        o = splitfiles[str(v["reference_build"])]
        o.write("\t".join([str(x).translate(tsv_trans) for x in v]) + "\n")

    for f in splitfiles.values():
        f.close()

    liftover_tmpl = os.path.join(dir, "temp02.from{from_db}." + out_fn_template)
    for from_db, to_db, chain in LIFTOVER_CHAINS:
        infile = step1_tmpl.format(db=from_db)
        outfile = liftover_tmpl.format(from_db=from_db, db=to_db)
        shell(
            f"{BED_SORT_CMD} {infile} /dev/stdout| {LIFT_OVER_CMD} -bedPlus=12 -tab /dev/stdin {chain} {outfile} {outfile}.unmapped"
        )

    for from_db, to_db, _ in LIFTOVER_CHAINS:
        outfile = out_fn_template.format(db=to_db)
        if1 = step1_tmpl.format(db=to_db)
        if2 = liftover_tmpl.format(from_db=from_db, db=to_db)
        shell(f"cat {if1} {if2} | bedSort /dev/stdin {outfile}")

    return {db: out_fn_template.format(db=db) for db in ["hg19", "hg38"]}


def filter_augment_variant_summaries(orig_vs_df: pd.DataFrame) -> pd.DataFrame:
    logging.info(f"Initial Variants dataframe shape: {orig_vs_df.shape}")
    orig_vs_df = orig_vs_df.loc[(orig_vs_df.is_flagged == False), :].reset_index(
        drop=True
    )
    logging.info(
        f"Dataframe shape of variants after filtering flagged: {orig_vs_df.shape}"
    )

    return orig_vs_df.assign(
        short_variant_string=extract_variant_name(orig_vs_df),
        molecular_profile_desc="",
        assertion_desc="",
        evidence_desc="",
    )


def join_set(
    sep: str,
    values: set[str],
    max_length: int = MAX_LONG_BED_FIELD_LENGTH,
    ellipses: str = "...",
) -> str:
    """Join strings together, but only up to a maximum length. If the
    maximum length is exceeded, add the ellipses string at the end.

    """
    ellipses_len = len(sep) + len(ellipses)
    max_length -= ellipses_len

    r = ""
    loop_sep = ""  # after first iteration switch to `sep`
    for v in values:
        if len(r) + len(v) > max_length:
            return r + loop_sep + ellipses
        r += loop_sep + v
        loop_sep = sep
    return r


def add_variant_diseases_therapies(
    assertion_df: pd.DataFrame,
    evidence_df: pd.DataFrame,
    variant_df: pd.DataFrame,
    mid2vids: dict[int, list[int]],
) -> pd.DataFrame:
    vid2diseases: dict[int, set[str]] = defaultdict(set)
    vid2dis_html: dict[int, set[str]] = defaultdict(set)
    vid2therapies: dict[int, set[str]] = defaultdict(set)
    missing_mps = []

    columns = ["molecular_profile_id", "disease_link", "disease_html", "therapies"]

    # Gather all the associations from the Assertion table
    for _, mpid, disease, disease_html, therapies in assertion_df[columns].itertuples():
        # tests for self-equality will remove NaNs
        tset = set(therapies.split(",")) if therapies == therapies else set()
        dset = set([disease]) if disease == disease else set()
        dhset = set([disease_html]) if disease_html == disease_html else set()

        if mpid in mid2vids:
            for vid in mid2vids[mpid]:
                vid2diseases[vid] |= dset
                vid2dis_html[vid] |= dhset
                vid2therapies[vid] |= tset
        else:
            missing_mps.append(mpid)

    # Gather all the associations from the Evidence table
    for _, mpid, disease, disease_html, therapies in evidence_df[columns].itertuples():
        tset = set(therapies.split(",")) if therapies == therapies else set()
        dset = set([disease]) if disease == disease else set()
        dhset = set([disease_html]) if disease_html == disease_html else set()

        if mpid in mid2vids:
            for vid in mid2vids[mpid]:
                vid2diseases[vid] |= dset
                vid2dis_html[vid] |= dhset
                vid2therapies[vid] |= tset
        else:
            missing_mps.append(mpid)

    variant_id = []
    dhtml_col = []
    dlink_col = []
    therapy_col = []

    for vid in set(vid2diseases.keys()) | set(vid2therapies.keys()):
        variant_id.append(vid)
        dhtml_col.append(join_set(", ", vid2dis_html.get(vid, set())))
        dlink_col.append(join_set(",", vid2diseases.get(vid, set()), ellipses="|..."))
        therapy_col.append(join_set(", ", vid2therapies.get(vid, set())))

    dt_df = pd.DataFrame(
        {
            "variant_id": variant_id,
            "disease_html": dhtml_col,
            "disease_links": dlink_col,
            "therapies": therapy_col,
        }
    ).set_index("variant_id")

    # The best (alphabetically lowest) evidence level of all evidence items
    vid2level: dict[int, str] = {}
    for mpid, level in zip(
        evidence_df["molecular_profile_id"], evidence_df["evidence_level"]
    ):
        if level != level or mpid not in mid2vids:
            continue
        for vid in mid2vids[mpid]:
            vid2level[vid] = min(level, vid2level.get(vid, level))

    out_df = variant_df.set_index("variant_id").join(dt_df).reset_index()
    return out_df.assign(
        best_evidence_level=out_df["variant_id"].map(vid2level).fillna("")
    )


def transform_dfs(dfs: dict[str, pd.DataFrame]) -> dict[str, str]:
    """From a dictionary of data frames for VariantSummaries,
    MolecularProfileSummaries, AssertionSummaries, and ClinicalEvidenceSummaries,
    create a unifed bigBed file for each reference genome.

    Returns a dictionary from reference genome slug (hg19/hg38) to the
    file path of the bidBed files.
    """
    adf = transform_assertion_summaries(
        dfs["AssertionSummaries"], dfs["MolecularProfileSummaries"]
    )
    edf = transform_clinical_evidence(
        dfs["ClinicalEvidenceSummaries"], dfs["MolecularProfileSummaries"]
    )

    mps = dfs["MolecularProfileSummaries"]
    mid2vids = split_one_to_n_map(mps, "molecular_profile_id", "variant_ids")

    filtered_variant = filter_augment_variant_summaries(dfs["VariantSummaries"])
    filtered_variant = add_variant_diseases_therapies(
        adf, edf, filtered_variant, mid2vids
    )

    gene_variant_df = transform_gene_variant_summaries(filtered_variant)
    gene_beds = liftover_gene_variant_beds(gene_variant_df)

    fusion_variant_dfs = {}
    fusion_decoration_dfs = {}
    for ref, fn in GENCODE_UCSC_FN.items():
        fusion_variant_dfs[ref], fusion_decoration_dfs[ref] = transform_fusion_variants(
            filtered_variant, fn
        )

    fusion_beds = write_fusion_beds(fusion_variant_dfs)
    decoration_beds = write_fusion_beds(
        fusion_decoration_dfs, out_fn_template="civic.decorator.{db}.bed"
    )

    finalBed_tmpl = "civic.{ref}.bed"
    finalBigBed_tmpl = "civic.{ref}.bb"
    outbb = {}
    for ref in gene_beds:
        gbed = gene_beds[ref]
        fbed = fusion_beds[ref]
        outbed = finalBed_tmpl.format(ref=ref)
        outbb[ref] = finalBigBed_tmpl.format(ref=ref)
        shell(f"cat {gbed} {fbed} | bedSort /dev/stdin {outbed}")
        shell(
            f"{BED_TO_BIG_BED_CMD} -tab -type=bed12+9 -as=civic.as {outbed} {CHROM_SIZES[ref]} {outbb[ref]}"
        )
        decbed = decoration_beds[ref]
        decbb = f"civic.decorator.{ref}.bb"
        shell(f"bedSort {decbed} {decbed}")
        shell(
            f"{BED_TO_BIG_BED_CMD} -tab -type=bed12+4 -as={DECORATION_AS} {decbed} {CHROM_SIZES[ref]} {decbb}"
        )
    return outbb


def main(
    day: datetime.date, dldir: str, overwrite: bool, skip_check_prior: bool
) -> None:
    logging.info(f"Starting civicToBed for {get_date_slug(day)}")
    dfs = download_and_load_dataframes(
        dldir, DOWNLOAD_BASE_URL, day, overwrite, skip_check_prior
    )

    transform_dfs(dfs)


if __name__ == "__main__":
    import argparse

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--test",
        help="do not run normally, instead execute unit tests",
        action="store_true",
    )
    ap.add_argument(
        "--day",
        help="which archive to download, in ISO format like 2024-11-28. Default: today()",
        default=datetime.date.today(),
        type=datetime.date.fromisoformat,
    )
    ap.add_argument(
        "--skip-check-prior",
        action="store_true",
        help="Don't look for the prior month to compare data sizes",
    )
    ap.add_argument(
        "--download-dir",
        default=DOWNLOAD_DIR,
        help=f"base of downloads; each date gets a separate subdirectory. default: {DOWNLOAD_DIR}",
    )
    ap.add_argument(
        "--overwrite",
        action="store_true",
        help=f"overwrite download of latest archive, if present",
    )
    argv = ap.parse_args()

    import sys

    print(sys.path)

    if argv.test:
        import doctest

        doctest.testmod()
    else:
        main(argv.day, argv.download_dir, argv.overwrite, argv.skip_check_prior)
