#!/usr/bin/env python3
""" Write the description pages of the bTaeGut7 hub into ../../trackDb/contrib/bTaeGut7.

Every page is Description / Display Conventions / Methods / Data Access / References /
Credits.  The per-track prose lives in the tracks table below; everything that is the same
on every page is assembled here, so the boilerplate cannot drift between 28 hand-kept files.

Subtrack pages open by naming their container and linking to it with
hgTrackUi?db=$db&g=$parentTrack&hgsid=$hgsid.  Those are trackDb variables, substituted by
the CGI when the page is served, because a hub track's real name carries a hub_<id>_ prefix
that is assigned per server and cannot be known when the hub is built.

References are the output of /cluster/bin/scripts/getTrackReferences, cached under refs/,
so no citation here is typed by hand.

Usage:  makeHtml.py
"""

import sys, os
from os.path import join, dirname, abspath, isfile

# this script and its inputs live in the kent tree; the pages it writes go to the place
# the tree keeps description pages of contributed GenArk tracks, next to hprc2annot and
# tiberius, so they are in git and makeHub.py can copy them into the hub from there
scriptDir = dirname(abspath(__file__))
refDir = join(scriptDir, "refs")
htmlDir = abspath(join(scriptDir, "..", "..", "trackDb", "contrib", "bTaeGut7"))

MAIN = "42561917"          # Formenti et al 2026, the assembly and annotation paper
ARK = "https://genomeark.s3.amazonaws.com/species/Taeniopygia_guttata/bTaeGut7"
ARKDIR = ("https://genomeark.s3.amazonaws.com/index.html?prefix=species/Taeniopygia_guttata/"
          "bTaeGut7/manuscript/annotations/")
GITHUB = "https://github.com/gf777/T2T-zebra-finch"
MAKEDOC = "src/hg/makeDb/doc/contrib/bTaeGut7.txt of the UCSC kent source tree"

def ref(pmid):
    " the getTrackReferences block for one PMID "
    path = join(refDir, pmid + ".html")
    if not isfile(path):
        sys.exit("missing refs/%s.html, run getTrackReferences %s > refs/%s.html"
                 % (pmid, pmid, pmid))
    return open(path).read().strip()

def link(url, text):
    return '<a href="%s" target="_blank">%s</a>' % (url, text)

def parentLink(label):
    " link from a subtrack page up to its container, resolved by the CGI at render time "
    return '<a href="hgTrackUi?db=$db&amp;g=$parentTrack&amp;hgsid=$hgsid">%s</a>' % label

# ---------------------------------------------------------------------------------------
# containers: name -> (label, description paragraphs, list of member labels)
# ---------------------------------------------------------------------------------------
containers = [
 ("readCoverage", "Read Coverage", """
<p>
Read coverage is the number of sequencing reads aligned over each base. It is the main
evidence that an assembled sequence is supported by data rather than being a mis-assembly,
and a drop or a spike in coverage marks a place worth looking at closely. These tracks show
coverage for the three long-read technologies used for this genome, and for the reads of the
earlier zebra finch reference mapped onto the new assembly.
</p>""", ["HiFi Coverage", "ONT Coverage", "CLR Coverage"]),

 ("genes", "Genes and Retrocopies", """
<p>
Gene annotation places transcripts and their coding sequence on the assembly, using
alignments of RNA sequencing reads and of proteins from related species. Alongside the
ordinary gene models, this collection carries retrocopies: DNA copies of a messenger RNA
that were written back into the genome by reverse transcription, which is why they lack the
introns of the gene they came from.
</p>""", ["EGAPx Genes", "Retrocopies"]),

 ("repeats", "Repeats", """
<p>
Roughly a third of a bird genome consists of repeated sequence. Transposable elements are
mobile sequences that copy themselves around the genome, while tandem repeats and satellites
are short motifs repeated head to tail, sometimes over megabases. These are the sequences
that short-read assemblies collapse, and resolving them is much of what a complete assembly
adds.
</p>""", ["Transposable Elements", "Tandem Repeats", "Satellite Repeats"]),

 ("centroTelo", "Centromeres and Telomeres", """
<p>
Centromeres are the chromosomal regions where the spindle attaches during cell division, and
telomeres are the repeated caps that protect the chromosome ends. Both are built from long
satellite arrays, so both were largely missing from earlier assemblies. This collection shows
the satellite arrays that make up the centromeres, the predicted centromere cores, mapped
genetic markers from earlier centromere studies, the terminal telomeres, and telomeric repeats
found away from the chromosome ends.
</p>""", ["Centromeric Satellites", "Centromere Cores", "Centromere Markers", "Telomeres",
          "Interstitial Telomeres"]),

 ("structVar", "Structural Variation", """
<p>
This collection shows two kinds of large-scale difference. One is the sequence that the new
assembly resolves but the previous zebra finch reference did not contain, which is where most
of the newly annotated genes are found. The other is structural variation between the two
haplotypes of this individual, the inversions and translocations that separate the maternal
and paternal copies of a chromosome.
</p>""", ["Previously Unassembled", "Large SVs"]),

 ("chromatin", "Methylation and 3D Chromatin", """
<p>
Beyond the base sequence, a chromosome carries chemical marks and a folded three-dimensional
shape, both of which relate to whether the DNA underneath is being transcribed. These tracks
show the fraction of DNA molecules carrying a methyl group on each CpG, and the split of the
genome into the two large compartments seen in Hi-C data: an open, gene rich A compartment and
a closed, heterochromatic B compartment.
</p>""", ["5mC Methylation", "Compartment E1", "A/B Compartments"]),
]

# ---------------------------------------------------------------------------------------
# tracks: name -> dict of the per-track prose
#   parent      label of the container, for the opening sentence
#   desc        Description section, background first then what is shown
#   conv        Display Conventions section, without the heading
#   methods     first Methods paragraph, how the annotation was produced
#   srcFile     file name on GenomeArk
#   srcSub      subdirectory of manuscript/annotations/ holding it, or "" for a full path
#   conv2       what our conversion did, appended to the second Methods paragraph
#   dataFile    the file in the hub, or None if the track points straight at GenomeArk
#   refs        extra PMIDs beyond the main paper
# ---------------------------------------------------------------------------------------
tracks = [

("gcPercent", dict(parent=None, dataFile=None,
 srcSub="general", srcFile="bTaeGut7v0.4_MT_rDNA.Teloscope.gc.bw",
 desc="""
<p>
GC content is the fraction of guanine and cytosine bases in a stretch of DNA. Along a bird
chromosome it is far from uniform: it rises with gene density and recombination rate, so the
small microchromosomes are considerably more GC rich than the large macrochromosomes, and
individual gene-dense regions stand out within a chromosome. This track shows the GC fraction
in fixed 200 bp windows.
</p>""",
 conv="""
<p>
A bar graph with one value per 200 bp window, autoscaled to the data in view.
</p>""",
 methods="""
<p>
GC content was computed with Teloscope, run over the assembly in non-overlapping 200 bp
windows (-w 200 -s 200) with the -g flag. Teloscope is a sequence scanner written for this
project; see %s. Full parameters are in the manuscript.
</p>""" % link("https://github.com/vgl-hub/teloscope", "vgl-hub/teloscope"),
 refs=[])),

("seqEntropy", dict(parent=None, dataFile=None,
 srcSub="general", srcFile="bTaeGut7v0.4_MT_rDNA.Teloscope.entropy.bw",
 desc="""
<p>
Shannon entropy measures how unpredictable the base sequence is inside a window. A window of
unique, random-looking sequence scores close to two bits per base, while a window filled with
a short motif repeated over and over scores much lower. Dips in this track therefore mark
simple repeats, satellite arrays and other low-complexity DNA, which is useful for reading the
repeat tracks alongside it.
</p>""",
 conv="""
<p>
A bar graph with one value per 200 bp window, autoscaled to the data in view. Values run from
0 to about 2 bits per base.
</p>""",
 methods="""
<p>
Shannon entropy was computed with Teloscope over the same non-overlapping 200 bp windows as
the GC track (-w 200 -s 200) with the -e flag. See %s for the tool and the manuscript for the
full parameters.
</p>""" % link("https://github.com/vgl-hub/teloscope", "vgl-hub/teloscope"),
 refs=[])),

("nonBdna", dict(parent=None, dataFile="nonBdna.bb",
 srcSub="nonB", srcFile="bTaeGut7v0.4_MT_rDNA.gfa%2BQuadron.sorted.v0.1.bed",
 desc="""
<p>
Most DNA in a cell is the familiar right-handed B-form double helix, but particular sequences
can fold into other shapes: four-stranded G-quadruplexes, left-handed Z-DNA, hairpins formed
by inverted repeats, and three-stranded triplexes. These structures can stall replication and
transcription, they are associated with elevated mutation rates, and they are enriched in
exactly the regions that were hardest to assemble. This track shows 11.3 million predicted
motifs in eight classes.
</p>""",
 conv="""
<p>
Each motif is drawn in the color of its class. The class, the motif length and the sequence
signature are in the item name and on the details page.
</p>
<table class="stdTbl">
  <tr><th style="background-color:#DB5829;width:2em">&nbsp;</th>
      <td>G4, G-quadruplex predicted by Quadron</td></tr>
  <tr><th style="background-color:#894B45;width:2em">&nbsp;</th>
      <td>Z, Z-DNA forming motif</td></tr>
  <tr><th style="background-color:#AE75A2;width:2em">&nbsp;</th>
      <td>APR, A-phased repeat</td></tr>
  <tr><th style="background-color:#7BB0DF;width:2em">&nbsp;</th>
      <td>DR, direct repeat</td></tr>
  <tr><th style="background-color:#E9E66D;width:2em">&nbsp;</th>
      <td>IR, inverted repeat, able to form a cruciform</td></tr>
  <tr><th style="background-color:#F4A637;width:2em">&nbsp;</th>
      <td>MR, mirror repeat</td></tr>
  <tr><th style="background-color:#ED952C;width:2em">&nbsp;</th>
      <td>TRI, triplex-forming motif, extracted from the mirror repeats</td></tr>
  <tr><th style="background-color:#008A69;width:2em">&nbsp;</th>
      <td>STR, short tandem repeat</td></tr>
</table>
<p>
Because there are more than eleven million motifs, the track is set to dense by default and
stops drawing above the window size inherited from the original session. Zoom in to see
individual motifs.
</p>""",
 methods="""
<p>
A-phased repeats, direct repeats, inverted repeats, mirror repeats, short tandem repeats and
Z-DNA motifs were annotated with gfa. Triplex motifs were marked by gfa and extracted from the
mirror repeat set. G-quadruplexes were predicted with Quadron; predictions with a score of NA
were removed, which Quadron produces for sequence within 50 bp of the end of an input
sequence, typically one or two motifs per chromosome.
</p>""",
 refs=["21097885", "29109402"])),

("covHifi", dict(parent="Read Coverage", dataFile="covHifi.bw",
 srcFile="v0.4_dip_hifi.pri.cov.wig", srcSub=None,
 srcUrl=ARK + "/assembly_verkko_0.1/manual_curation/bTaeGut7v0.4/mapping/v0.4_dip_hifi/"
              "v0.4_dip_hifi.pri.cov.wig",
 desc="""
<p>
PacBio HiFi reads are long reads with per-base accuracy comparable to short reads, and they
are the backbone of this assembly. This track shows how deeply HiFi reads cover each position
of the assembly. Flat coverage at the expected depth is the ordinary case; a collapse to zero
or a doubling of depth points at an assembly problem or at a repeat that has not been
separated correctly.
</p>""",
 conv="""
<p>
A bar graph of read depth, autoscaled to the data in view.
</p>""",
 methods="""
<p>
A total of 134.3 Gbp of PacBio HiFi data was generated from four sequencing runs of a single
library, 7.5 million reads with an N50 of 18,348 bp. Reads were mapped to the diploid assembly
and depth was summarized in fixed windows. The coverage tracks were used during manual
curation of the assembly graph, which is described in the manuscript.
</p>""",
 refs=[])),

("covOnt", dict(parent="Read Coverage", dataFile="covOnt.bw",
 srcFile="v0.4_dip_ont.pri.cov.wig", srcSub=None,
 srcUrl=ARK + "/assembly_verkko_0.1/manual_curation/bTaeGut7v0.4/mapping/v0.4_dip_ont/"
              "v0.4_dip_ont.pri.cov.wig",
 desc="""
<p>
Oxford Nanopore reads are longer than HiFi reads and less accurate, and their length is what
allows an assembly to cross long satellite arrays such as those in centromeres. This track
shows Nanopore read depth across the assembly, and is most informative in the repeat-rich
regions where HiFi coverage alone is not enough to resolve the structure.
</p>""",
 conv="""
<p>
A bar graph of read depth, autoscaled to the data in view.
</p>""",
 methods="""
<p>
Nanopore data came from nineteen libraries totalling 409.1 Gbp, 22.5 million reads with an N50
of 32,843 bp, combining standard ligation sequencing on R10.4.1 flow cells with ultra-long
sequencing from ultra-high molecular weight DNA. Reads were mapped to the diploid assembly and
depth was summarized in fixed windows.
</p>""",
 refs=[])),

("covClr", dict(parent="Read Coverage", dataFile=None,
 srcSub="bTaeGut1", srcFile="bTaeGut1.4_CLR.cov.bw",
 desc="""
<p>
This track shows coverage of the older PacBio Continuous Long Reads, generated for the
previous zebra finch reference bTaeGut1.4, after mapping them onto the new assembly. CLR reads
have a much higher error rate than HiFi reads, and the places where their coverage falls away
are a direct picture of what the earlier sequencing technology could not reach. Reading it
next to the Previously Unassembled track shows why those regions were missing.
</p>""",
 conv="""
<p>
A bar graph of read depth, autoscaled to the data in view. Note that these reads come from a
different individual than the one this genome was assembled from, so some of the variation is
genuine sequence difference rather than a property of the assembly.
</p>""",
 methods="""
<p>
PacBio CLR reads generated for the earlier bTaeGut1.4 reference, from a different individual,
were mapped onto the bTaeGut7 assembly and depth was summarized in fixed windows.
</p>""",
 refs=[])),

("egapx", dict(parent="Genes and Retrocopies", dataFile="egapx.bb",
 srcSub="genes", srcFile="bTaeGut7v0.4_MT_rDNA.EGAPx.v0.1.gtf.gz",
 desc="""
<p>
This track shows protein-coding and long non-coding RNA gene models built by NCBI's
eukaryotic genome annotation pipeline. Models are constructed from alignments of RNA
sequencing reads and of proteins from related species, combined with ab initio prediction, so
each transcript reflects both direct expression evidence and similarity to known genes. The
track contains about 142,000 transcripts.
</p>""",
 conv="""
<p>
Transcripts are drawn in the standard gene model style: boxes for exons, thin lines with
arrowheads for introns showing the direction of transcription, and thicker boxes for the
coding part of each exon. Clicking a transcript opens a page with its identifier and
structure.
</p>""",
 methods="""
<p>
Both haplotypes were annotated with the public version of NCBI's Eukaryotic Genome Annotation
Pipeline, EGAPx v0.4.1 (%s), which combines RNA-seq and protein alignments with ab initio
modelling. A set of 33 Illumina RNA-seq datasets covering a range of tissues and developmental
stages was used, drawn in part from NCBI BioProjects PRJNA1032078, PRJNA516733 and
PRJNA977995. Curated RefSeq and RNA-seq supported model RefSeq proteins from human, chicken,
song sparrow, barn swallow and other sauropsids provided additional support, and one-to-one
orthologs of human protein-coding genes were computed from protein homology and local synteny.
</p>""" % link("https://github.com/ncbi/egapx", "github.com/ncbi/egapx"),
 conv2="""
The GTF was converted to a genePred with gtfToGenePred and then to bigGenePred, so exon
structure and coding boundaries are preserved.""",
 refs=[])),

("retrocopies", dict(parent="Genes and Retrocopies", dataFile="retrocopies.bb",
 srcSub="retrogenes", srcFile="bTaeGut7v0.4_MT_rDNA.RCPedia.v0.1.gff",
 desc="""
<p>
A retrocopy arises when a messenger RNA is reverse transcribed and inserted back into the
genome. Because the copy is made from spliced RNA it has no introns, and it usually lands far
from the gene it came from. Most retrocopies decay into pseudogenes, but some acquire
regulatory sequence and become expressed genes in their own right. This track shows 151
retrocopies that survived filtering and had expression support.
</p>""",
 conv="""
<p>
Retrocopies are drawn as gene models named after the parent gene they derive from. Clicking an
item opens a page with its structure.
</p>""",
 methods="""
<p>
Retrocopies were identified with an improved version of the RCPedia pipeline. Messenger RNA
sequences of annotated protein-coding genes were extracted with gffread and aligned to the
genome with LAST (lastal -D1000). A candidate was kept if the alignment exceeded 120 bp, lay at
least 200 kb from the parental locus, and preserved at least one of the last three exon-exon
junctions of the parent transcript. Alignments with 40 percent or more repetitive sequence
were discarded, as were candidates overlapping three or more annotated exons or coming from
gene families with more than five redundant copies. Expression was then filtered against a
simulated background: paired-end RNA-seq excluding exonic retrocopy regions was simulated with
SANDY v1.0 and quantified with kallisto v0.48.0, and any retrocopy detectable in the simulation
was dropped as unreliable. Remaining retrocopies were quantified on seven real RNA-seq datasets
and kept only when supported by at least one uniquely mapped read from STAR v2.7.7a.
</p>""",
 conv2="""
The GFF was converted to a genePred with gff3ToGenePred and then to bigGenePred.""",
 refs=["39240653"])),

("transposons", dict(parent="Repeats", dataFile="transposons.bb",
 srcSub="repeats", srcFile="bTaeGut7v0.4_MT_rDNA.EDTA2.v0.2.gtf.gz",
 desc="""
<p>
Transposable elements are sequences that copy or move themselves around the genome, and over
evolutionary time they accumulate as fossilized copies that make up a large share of the DNA.
Bird genomes are dominated by one family in particular, the CR1 LINEs, and are unusually poor
in transposable elements compared with mammals. This track shows about 530,000 annotated
elements and interspersed repeats.
</p>""",
 conv="""
<p>
Each element is drawn in the color that the source annotation assigns to its repeat family,
so related families share a hue but the mapping is not a small fixed set of categories. The
family name and its classification are on the details page of each item, together with the
sequence ontology term and the identity to the library consensus. The track is set to dense by
default because of the number of elements.
</p>""",
 methods="""
<p>
Repeats were annotated with a combination of de novo and curated resources. EDTA2 and
RepeatModeler were used for de novo identification and their output was processed through
RepeatMasker. EDTA was run with several curated libraries: zebra finch specific high-frequency
repeats, a library built from bTaeGut1.4, RepBase, and a set of CR1 elements. The resulting
repeat library was then curated with TEtrimmer v1.5.4 and the consensus sequences were checked
by hand, with classifications corrected where coverage, self-alignment and conserved domain
evidence disagreed with the automatic call. The curated library was fed back to EDTA with the
--curatedlib option to reannotate the genome.
</p>""",
 conv2="""
The GTF file uses GFF3-style attributes, so it does not parse as a gene annotation; each line
was therefore converted to one BED feature, keeping the feature type and the full attribute
string as extra fields and the color attribute as itemRgb.""",
 refs=["31843001"])),

("tandemRepeats", dict(parent="Repeats", dataFile="tandemRepeats.bb",
 srcSub="repeats", srcFile="bTaeGut7v0.4_MT_rDNA.trf.sorted.v0.1.bed",
 desc="""
<p>
A tandem repeat is a short motif repeated directly head to tail, from a few bases up to arrays
spanning megabases. They mutate quickly by slippage during replication, they are the basis of
many genetic markers, and long arrays of them are what makes centromeric and subtelomeric DNA
so difficult to assemble. This track shows about 980,000 tandem repeat arrays across both
euchromatin and heterochromatin.
</p>""",
 conv="""
<p>
Each array is drawn as a single feature named after its repeat unit. The repeat unit and the
full array sequence are kept as extra fields on the details page. Item names longer than 255
characters are truncated for display; the untruncated repeat unit is in the extra fields. The
track is set to dense by default because of the number of arrays.
</p>""",
 methods="""
<p>
Tandem repeats were called de novo across the assembly with Tandem Repeats Finder. In the
manuscript, TRF is used to extract monomer units from satellite arrays for the comparison of
Tgut191A-like repeats across bird genomes; the genome-wide call set distributed with the
assembly is what is shown here.
</p>""",
 refs=["9862982"])),

("satellome", dict(parent="Repeats", dataFile="satellome.bb",
 srcSub="repeats", srcFile="bTaeGut7v0.4_MT_rDNA.satellome.v0.1.bed",
 desc="""
<p>
Satellite DNA is tandemly repeated sequence present in very long arrays, usually in
centromeric and subtelomeric heterochromatin. Individual satellite families are often specific
to one species or a small group of species, which makes them useful markers of chromosome
structure and evolution. This track shows 901 satellite arrays assigned to named families.
</p>""",
 conv="""
<p>
Each array is drawn as a single feature named after its satellite family, for example
Tgut716A, or TEL for terminal telomeric repeat.
</p>""",
 methods="""
<p>
Satellite sequences were identified de novo with SRF, run on the assembly, and with TAREAN,
run on unassembled Nanopore reads from zebra finch testis after several rounds of subsetting
with seqtk. Sequences of 20 bp or shorter were discarded and the rest were curated by hand.
Further satellites were found by inspecting the repeats next to gaps in earlier zebra finch
assemblies. RepeatMasker v4.1.5 was then used to place the resulting satellite set on the
assembly. The satellite catalog was built with Satellome (%s).
</p>""" % link("https://github.com/aglabx/satellome", "github.com/aglabx/satellome"),
 refs=[])),

("centroSat", dict(parent="Centromeres and Telomeres", dataFile="centroSat.bb",
 srcSub="centromeres", srcFile="bTaeGut7v0.4_MT_rDNA.RM.Takki2022.v0.1.colored.merged.gff",
 desc="""
<p>
Bird centromeres are built from long arrays of satellite repeat, and in the zebra finch two
families dominate: Tgut716A and Tgut191A. Because these arrays are hundreds of kilobases of
nearly identical sequence, they collapsed in every earlier assembly of this species, and
resolving them is one of the main results of this genome. This track shows about 7,000
satellite array segments placed with a curated centromere satellite library.
</p>""",
 conv="""
<p>
Each segment is drawn in the color that the source annotation gives its satellite family, so
one array appears as a run of features in a single color. The family is in the item name, for
example Motif:Tgut716A, and the full annotation attributes are on the details page.
</p>""",
 methods="""
<p>
Centromeric satellite repeats were localized with RepeatMasker using the curated satellite
library published for the zebra finch by Takki et al. Adjacent hits to the same family were
merged. The similarity of Tgut716A to crowSat1 was assessed with Censor.
</p>""",
 conv2="""
The GFF is a flat feature annotation rather than a gene annotation, so each line became one
BED feature, with the color attribute kept as itemRgb and the remaining attributes as an extra
field.""",
 refs=["35279659"])),

("centroCores", dict(parent="Centromeres and Telomeres", dataFile="centroCores.bb",
 srcSub="centromeres", srcFile="bTaeGut7v0.4_MT_rDNA.centromere_detector.v0.1.gff",
 desc="""
<p>
The centromere core is the part of the satellite array where the kinetochore actually
assembles, and it is usually narrower than the full array and marked by a local dip in DNA
methylation. This track shows 80 predicted centromere core regions, one or a few per
chromosome arm.
</p>""",
 conv="""
<p>
Cores are drawn as single blocks. The details page carries the satellite motif the core was
called on and the average methylation measured across it, which is the value that separates
an active core from the surrounding array.
</p>""",
 methods="""
<p>
Centromeric cores were located bioinformatically from the satellite annotation and the
methylation signal, and checked against the mapped centromere markers. The scripts used in the
centromere analyses are at %s.
</p>""" % link(GITHUB, "github.com/gf777/T2T-zebra-finch"),
 conv2="""
Each GFF line became one BED feature, keeping the color attribute as itemRgb and the
attributes, including the average methylation, as an extra field.""",
 refs=[])),

("centroMarkers", dict(parent="Centromeres and Telomeres", dataFile="centroMarkers.bb",
 srcSub="centromeres", srcFile="bTaeGut7v0.4_MT_rDNA.BLAST.Knief2016.v0.2.bed",
 desc="""
<p>
Before assemblies could span a centromere, centromere positions were located genetically, by
following which markers failed to recombine. This track places the primer pairs from one such
study onto the assembly, so the earlier genetic map of zebra finch centromeres can be compared
directly with the satellite arrays now visible in the sequence. It contains 225 mapped primer
positions.
</p>""",
 conv="""
<p>
Each item is one mapped primer, named after the marker it belongs to and whether it is the
forward or reverse primer of the pair, together with the position class assigned in the
original study.
</p>""",
 methods="""
<p>
64 previously described primer pairs were queried against both haplotype assemblies with
blastn -task blastn-short. Hits from the two haplotypes were combined and filtered to keep
only chromosome-specific hits whose forward and reverse partners lay less than 1 kb apart.
Because the assembly has diverged from the reference the primers were designed on, primers
without a surviving pair were rescued by taking the single highest scoring match per
chromosome. Final coordinates were checked by hand.
</p>""",
 refs=["26667931"])),

("telomeres", dict(parent="Centromeres and Telomeres", dataFile="telomeres.bb",
 srcSub="telomeres", srcFile="bTaeGut7v0.4_MT_rDNA.Teloscope.terminal.bed",
 desc="""
<p>
Telomeres are arrays of the motif TTAGGG that cap each chromosome end and protect it from
being treated as a broken chromosome. An assembly that reaches the telomere on both ends of a
chromosome is telomere-to-telomere for that chromosome, which is what this track lets you
check. It contains 160 terminal telomere arrays.
</p>""",
 conv="""
<p>
Each item is one terminal telomere array, labelled p or q for the chromosome end it caps. The
details page carries the array length and the counts of canonical and non-canonical repeat
units.
</p>""",
 methods="""
<p>
Telomeres were annotated with Teloscope (-w 200 -s 200 -c TTAGGG -p NNNGGG -d 200 -l 500, with
the flags -r -g -e -m -i) and with seqtk v1.4 run with -d 50000 to raise the maximum drop
cutoff and recover additional telomeres. Contigs were then classified into eight categories by
telomere completeness and gap presence.
</p>""",
 refs=[])),

("itsRepeats", dict(parent="Centromeres and Telomeres", dataFile="itsRepeats.bb",
 srcSub="telomeres", srcFile="bTaeGut7v0.4_MT_rDNA.teloscope.v0.3.its.merged200.bed",
 desc="""
<p>
Interstitial telomeric sequences are stretches of the telomere motif found away from the
chromosome ends. They are usually read as scars of ancient chromosome fusions or of repair at
double-strand breaks, and they are fragile sites where further rearrangement tends to happen.
This track shows 664 such clusters.
</p>""",
 conv="""
<p>
Each item is one cluster of telomeric repeat away from a chromosome end.
</p>""",
 methods="""
<p>
Telomeric blocks called by Teloscope were clustered with bedtools merge -d 200 and then
filtered on TTAGGG repeat density, keeping clusters above 0.5. The resulting coordinates were
classified by length and chromosomal position following the published scheme, and clusters
above 5 kb were compared with the terminal telomeres using StainedGlass v0.6 at a 400 bp
window.
</p>""",
 refs=[])),

("newRegions", dict(parent="Structural Variation", dataFile="newRegions.bb",
 srcSub="PUR", srcFile="bTaeGut7v0.4_MT_rDNA.PUR.fastga.v0.1.bed",
 desc="""
<p>
This track marks the sequence that the new assembly resolves and the previous zebra finch
reference, bTaeGut1.4, did not contain or had collapsed. These regions are where most of the
newly annotated genes are found, and they are strongly enriched for satellite DNA and other
repeats. The track contains about 27,000 regions longer than 1 kb.
</p>""",
 conv="""
<p>
Plain blocks, one per region. Regions shorter than 1 kb were filtered out by the authors, so
the track shows the substantial gains rather than every base of difference.
</p>""",
 methods="""
<p>
The new diploid assembly was aligned to the previous reference GCF_003957565.2 (bTaeGut1.4)
with FastGA, and the resulting unassembled-region intervals were filtered to keep only those
longer than 1 kb. Each gene was then classified by how it sits relative to these regions, and
genes lying entirely inside one, or overlapping one over at least 75 percent of their length,
were called putative novel or improved genes; novelty was checked by aligning the gene
sequence back to bTaeGut1.4. The manuscript reports 2,710 genes gained this way, and a
per-chromosome Fisher test of which feature types are enriched in these regions.
</p>""",
 refs=["41262969"])),

("largeSv", dict(parent="Structural Variation", dataFile="largeSv.bb",
 srcSub="large_SVs", srcFile="bTaeGut7v0.4_MT_rDNA.large_SVs.v0.1.bed",
 desc="""
<p>
Because this genome was assembled as two separate haplotypes, the maternal and paternal copy
of each chromosome can be compared directly. This track shows the large differences between
them: 38 inversions and 4 translocations. Differences of this size are invisible to short-read
resequencing and are one of the practical reasons for building a phased diploid assembly.
</p>""",
 conv="""
<p>
Each item is one variant, named by its type, either inversion or translocation.
</p>""",
 methods="""
<p>
Maternal and paternal haplotypes were aligned to each other with MashMap and the alignment was
inspected as a dotplot in JBrowse2. Every structural variant seen there was confirmed against
the Hi-C contact map in PretextView. Genome-wide sequence and structural variation was then
quantified with SyRI v1.6.3 using default filtering, and visualized with plotsr v1.1.1.
</p>""",
 refs=["31842948"])),

("methyl5mC", dict(parent="Methylation and 3D Chromatin", dataFile=None,
 srcSub="methylation", srcFile="bTaeGut7v0.4_MT_rDNA.PBmethylation.v0.1.bw",
 desc="""
<p>
Cytosines in a CpG dinucleotide can carry a methyl group, and this mark is copied to the
daughter strand after replication, so it is inherited through cell division. Dense methylation
is generally associated with silenced DNA, and PacBio reads report it directly, because the
polymerase kinetics differ over a modified base. This track shows the estimated fraction of
molecules methylated at each CpG. The dip in methylation inside an otherwise heavily methylated
satellite array is a well-known signature of the active centromere core.
</p>""",
 conv="""
<p>
A bar graph of methylation probability per CpG site, autoscaled to the data in view.
</p>""",
 methods="""
<p>
Per-read base modifications were called from the HiFi BAM with the PacBio Jasmine caller (%s),
producing a modBAM with MM and ML tags. HiFi reads were extracted from the CCS output with
extracthifi from pbtk, aligned to the reference with pbmm2, and sorted and indexed with
SAMtools. Reference-anchored CpG methylation probabilities were then computed with
aligned_bam_to_cpg_scores from %s.
</p>""" % (link("https://github.com/PacificBiosciences/jasmine", "PacBio Jasmine"),
            link("https://github.com/PacificBiosciences/pb-CpG-tools", "pb-CpG-tools")),
 refs=[])),

("compartE1", dict(parent="Methylation and 3D Chromatin", dataFile=None,
 srcSub="3D", srcFile="bTaeGut7v0.4_MT_rDNA.Cooltools.E1.200kbp.flipped.dip.collated.v0.1.bw",
 desc="""
<p>
Hi-C measures which parts of the genome are physically close together in the nucleus. When the
resulting contact matrix is decomposed, the first eigenvector splits the genome into two
alternating compartments: A, which is open, gene rich and transcribed, and B, which is closed
and heterochromatic. This track shows that eigenvector at 200 kb resolution, with the sign set
so that positive values mark the A compartment.
</p>""",
 conv="""
<p>
A bar graph of the first eigenvector at 200 kb, autoscaled to the data in view. Positive values
are the active A compartment and negative values the heterochromatic B compartment.
</p>""",
 methods="""
<p>
Adapters were trimmed from the Arima Hi-C reads with cutadapt v5.1 and about 414 million
trimmed reads were aligned with BWA-MEM v0.7.17 using -SP5M. Read pairs were extracted with
pairtools parse, then sorted and deduplicated, leaving 41.9 million unique pairs for the
diploid assembly. Contact matrices were built with cooler cload pairs at 10 kb bins, balanced,
and zoomified to 10, 20, 50, 100 and 200 kb. Eigenvectors were computed with cooltools eigs-cis
using a gene density track derived from the EGAPx annotation as the phasing track, so that a
positive first eigenvector corresponds to the gene rich A compartment.
</p>""",
 refs=["38709825"])),

("compartAB", dict(parent="Methylation and 3D Chromatin", dataFile="compartAB.bb",
 srcSub="3D", srcFile="bTaeGut7v0.4_MT_rDNA.Cooltools.E1.200kbp.flipped.dip.collated.v0.1.bed",
 desc="""
<p>
This track is the interval form of the Hi-C compartment call: the same 200 kb analysis as the
Compartment E1 track, reduced to labelled A and B blocks. It is the easier of the two to read
when zoomed out, and the easier one to intersect with other annotations. It contains about
3,100 intervals.
</p>""",
 conv="""
<p>
Intervals are colored by compartment.
</p>
<table class="stdTbl">
  <tr><th style="background-color:#E38AAA;width:2em">&nbsp;</th>
      <td>A compartment, open and gene rich</td></tr>
  <tr><th style="background-color:#6F9DD0;width:2em">&nbsp;</th>
      <td>B compartment, closed and heterochromatic</td></tr>
</table>""",
 methods="""
<p>
Produced by the same cooltools eigs-cis analysis as the Compartment E1 track, with the sign of
the first eigenvector at 200 kb converted into A and B interval calls. Topologically associating
domain boundaries were also computed at 200 kb with cooltools insulation, and are not part of
this hub.
</p>""",
 refs=["38709825"])),
]

# ---------------------------------------------------------------------------------------

dataAccess = """
<h2>Data Access</h2>
<p>
The data can be explored interactively in table format with the
<a href="../cgi-bin/hgTables">Table Browser</a> or the
<a href="../cgi-bin/hgIntegrator">Data Integrator</a> and exported from there to spreadsheet or
tab-separated tables. From scripts, the data can be accessed through our
%s, track=<i>%s</i>.
</p>
%s
<p>
The original annotation files are on GenomeArk, %s.
</p>"""

hubFileAccess = """
<p>
For automated download and analysis, this annotation is a %s file inside the hub. Individual
regions or the whole annotation can be obtained with <tt>%s</tt>, which can be compiled from
source or downloaded as a precompiled binary; instructions are
<a href="http://hgdownload.soe.ucsc.edu/downloads.html#utilities_downloads" target="_blank">here</a>.
The tool also fetches a range, for example
<tt>%s &lt;hubUrl&gt;/GCF_048771995.1/%s -chrom=NC_133024.1 -start=0 -end=100000 stdout</tt>,
where &lt;hubUrl&gt; is the directory this hub was loaded from.
</p>"""

remoteFileAccess = """
<p>
This track points straight at the file on GenomeArk rather than at a copy in the hub, so no
download step is needed to work with it: %s reads it over the network, as does the browser.
</p>"""

credits = """
<h2>Credits</h2>
<p>
The assembly and all of these annotations were produced by the Vertebrate Genome Laboratory at
The Rockefeller University and their collaborators, and released through GenomeArk. Thanks to
Giulio Formenti and Erich D. Jarvis and their co-authors for making the data available before
and after publication. The analysis code is at %s. This hub was assembled at UCSC from the IGV
session distributed with the annotations, using hubtools.
</p>""" % link(GITHUB, "github.com/gf777/T2T-zebra-finch")

def methodsSecond(t, name):
    " the second Methods paragraph: where the file came from and what we did to it "
    srcUrl = t.get("srcUrl") or "%s/manuscript/annotations/%s/%s" % (ARK, t["srcSub"],
                                                                     t["srcFile"])
    txt = '<p>\nThe file was taken from GenomeArk at %s.' % link(srcUrl, srcUrl)
    if t.get("dataFile"):
        txt += (" It is a text format that a track hub cannot use directly, so it was"
                " converted to a binary indexed file.")
        txt += t.get("conv2", "")
        txt += (" The conversion, and the whole hub, is reproduced by the commands in"
                " %s." % MAKEDOC)
    else:
        txt += (" It is already in a binary indexed format that the browser reads over the"
                " network, so the track points at it where it is and no copy is kept in the"
                " hub. The hub is reproduced by the commands in %s." % MAKEDOC)
    return txt + "\n</p>"

def writePage(name, title, body):
    path = join(htmlDir, name + ".html")
    open(path, "wt").write(body.strip() + "\n")
    return path

def writeHubDescription():
    " the hub overview page, shown on the hub connect page and above the track groups "
    rows = "\n".join('  <tr><td>%s</td><td>%s</td></tr>' % (label, ", ".join(members))
                     for _, label, _, members in containers)
    body = """<h2>Description</h2>
<p>
This hub carries the annotations released with the telomere-to-telomere genome of the zebra
finch, <em>Taeniopygia guttata</em>, individual bTaeGut7. The assembly resolves the somatic
genome completely, including the centromeres and eleven telomere-to-telomere dot chromosomes,
and adds 2,710 genes that were missing or incomplete in the previous zebra finch reference.
</p>
<p>
The tracks are shown on the maternal haplotype, which NCBI holds as %s. The paternal haplotype
is a separate assembly, %s, and is not part of this hub. Annotations that fall on paternal
sequence are present in the underlying files but are not displayed here, because this assembly
does not contain those chromosomes.
</p>

<h2>Tracks</h2>
<p>
Three tracks describe the bare sequence and stand on their own: GC Content, Sequence Entropy
and Non-B DNA. The rest are grouped into collections.
</p>
<table class="stdTbl">
  <tr><th>Collection</th><th>Tracks</th></tr>
%s
</table>

<h2>Data Access</h2>
<p>
Each track has its own description page with download and intersection instructions. The
original annotation files are on GenomeArk, %s, and the assembly and raw data are under the
%s.
</p>

<h2>References</h2>
%s
%s""" % (link("https://www.ncbi.nlm.nih.gov/datasets/genome/GCF_048771995.1/",
              "GCF_048771995.1"),
         link("https://www.ncbi.nlm.nih.gov/datasets/genome/GCA_048772025.1/",
              "GCA_048772025.1"),
         rows, link(ARKDIR, "in the bTaeGut7 annotations directory"),
         link("https://www.genomeark.org/genomeark-all/Taeniopygia_guttata.html",
              "GenomeArk page for this species"),
         ref(MAIN), credits)
    writePage("hubDescription", "Zebra finch T2T hub", body)

def main():
    if not os.path.isdir(htmlDir):
        os.makedirs(htmlDir)
    count = 0
    writeHubDescription()

    for name, label, desc, members in containers:
        memberTxt = ", ".join(members[:-1]) + " and " + members[-1]
        body = """<h2>Description</h2>
%s
<p>
This collection holds the %s tracks. Each has its own description page with the details of how
it was made, reachable from the track name in the browser or from the list below the
configuration controls on this page.
</p>

<h2>Data Access</h2>
<p>
The individual track description pages carry the download and intersection instructions for
each annotation. The original files are on GenomeArk, %s.
</p>

<h2>References</h2>
%s
%s""" % (desc.strip(), memberTxt, link(ARKDIR, "in the bTaeGut7 annotations directory"),
         ref(MAIN), credits)
        writePage(name, label, body)
        count += 1

    for name, t in tracks:
        # a track in a container opens by naming it and linking to it; a top-level track has
        # nothing to link to, so it goes straight into the description
        opening = ""
        if t["parent"]:
            opening = ('<p>\nThis track is part of the %s collection of the zebra finch '
                       'telomere-to-telomere hub.\n</p>' % parentLink(t["parent"]))
        if t.get("dataFile"):
            ext = t["dataFile"].rsplit(".", 1)[1]
            tool = "bigBedToBed" if ext == "bb" else "bigWigToBedGraph"
            fmt = "bigBed" if ext == "bb" else "bigWig"
            access = hubFileAccess % (fmt, tool, tool, t["dataFile"])
        else:
            tool = ("bigWigToBedGraph" if t["srcFile"].endswith(".bw") else "bigBedToBed")
            access = remoteFileAccess % ("<tt>%s</tt>" % tool)

        refs = ref(MAIN)
        for pmid in t.get("refs", []):
            refs += "\n" + ref(pmid)

        intro = (opening + "\n" + t["desc"].strip()) if opening else t["desc"].strip()
        body = """<h2>Description</h2>
%s

<h2>Display Conventions and Configuration</h2>
%s

<h2>Methods</h2>
%s
%s
%s

<h2>References</h2>
%s
%s""" % (intro, t["conv"].strip(), t["methods"].strip(),
         methodsSecond(t, name),
         dataAccess % (link("https://api.genome.ucsc.edu", "API"), name, access,
                       link(ARKDIR, "in the bTaeGut7 annotations directory")),
         refs, credits)
        writePage(name, name, body)
        count += 1

    print("Wrote %d description pages into %s" % (count, htmlDir))

main()
