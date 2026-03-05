---
title: "UCSC Genome Browser Glossary"
---

A comprehensive reference guide to terminology used on the UCSC Genome Browser.

This page covers the following topics:

- [Genome Assemblies and Nomenclature](#genome-assemblies-and-nomenclature)
    - [Popular Genome Assemblies](#popular-genome-assemblies)
- [Core Tools](#core-tools)
- [Browser Interface and Interaction](#browser-interface-and-interaction)
    - [Main Display Elements](#main-display-elements)
    - [Navigation Controls](#navigation-controls)
    - [Mouse Interactions](#mouse-interactions)
    - [Position and Search](#position-and-search)
    - [Configuration and Settings](#configuration-and-search)
    - [Views, Output, and Export](#views-output-and-export)
- [Tracks and Display](#tracks-and-display)
- [User Data Features](#user-data-features)
- [Data Formats](#data-formats)
- [Genome Browser Data and Annotations](#genome-browser-data-and-annotations)
    - [Gene Annotations](#gene-annotations)
    - [Conservation and Comparative Genomics](#conservation-and-comparative-genomics)
    - [Variants and Clinical Data](#variants-and-clinical-data)
    - [Regulatory and Functional Data](#regulatory-and-functional-data)
- [Technical Terms](#technical-terms)

## Genome Assemblies and Nomenclature

**Assembly**: A genome assembly is the complete genome sequence produced after
chromosomes have been fragmented, sequenced, and computationally reassembled.
Assemblies are updated when new sequence data fills gaps or improved algorithms
produce better results. Find supported assemblies from the 
[gateway page](/cgi-bin/hgGateway) or request new ones from
our [assembly search page](/assemblySearch.html).

**[GenArk](https://hgdownload.gi.ucsc.edu/hubs/)**: UCSC's Genome Archive
containing thousands of additional genome assemblies beyond the main featured
assemblies.

**Chromosome Coordinates**: Genomic positions specified as chromosome name and
base position (e.g., `chr7:155,799,529-155,812,871`). UCSC uses zero-based,
half-open coordinates in its databases.


### Popular Genome Assemblies

**[hg19 (GRCh37)](/cgi-bin/hgTracks?db=hg19)**: The February 2009 human
reference genome assembly from the Genome Reference Consortium. Still widely
used for legacy datasets and clinical annotations.

**[hg38 (GRCh38)](/cgi-bin/hgTracks?db=hg38)**: The December 2013 human
reference genome assembly, the current standard for most new human genomics
work. Contains improved sequence accuracy and gap filling compared to hg19.

**[hs1 (T2T-CHM13)](/cgi-bin/hgTracks?db=hub_3671779_hs1)**: The
telomere-to-telomere human genome assembly released in 2022, representing the
first complete, gapless sequence of a human genome including centromeres and
other previously unresolved regions.

**[mm10 (GRCm38)](/cgi-bin/hgTracks?db=mm10)**: The December 2011 mouse
reference genome assembly from the Genome Reference Consortium.

**[mm39 (GRCm39)](/cgi-bin/hgTracks?db=mm39)**: The June 2020 mouse reference
genome assembly, the current standard for mouse genomics.


## Core Tools

**[Genome Browser](/cgi-bin/hgTracks)**: The main visualization tool that
displays any portion of a genome at any scale with aligned annotation tracks
showing genes, regulatory elements, conservation, variants, and other genomic
features.

**[BLAT (BLAST-Like Alignment Tool)](/cgi-bin/hgBlat)**: A rapid sequence
alignment tool developed by Jim Kent for finding sequence matches in genomes.
Faster than BLAST for closely related sequences and useful for locating
mRNA/EST alignments.

**[Table Browser](/cgi-bin/hgTables)**: A web interface for querying,
filtering, and downloading data from the underlying MySQL databases. Allows
intersection of data tables and export in multiple formats.

**[LiftOver](/cgi-bin/hgLiftOver)**: A tool for converting genomic coordinates
between different genome assemblies (e.g., hg19 to hg38). Requires chain files
that map regions between assemblies.

**[In-Silico PCR](/cgi-bin/hgPcr)**: A tool for virtually testing PCR primer
pairs against a genome to verify specificity and predict amplicon locations.

**[Variant Annotation Integrator](/cgi-bin/hgVai)**: A tool for annotating
genomic variants using multiple data sources to predict functional effects.

**[Data Integrator](/cgi-bin/hgIntegrator)**: A tool for intersecting and
combining data from multiple annotation tracks simultaneously.


## Browser Interface and Interaction

### Main Display Elements

**Browser Graphic / Tracks Image**: The main visualization area displaying the
genome and all visible annotation tracks. The image is interactive and supports
mouse-based navigation.

**Base Position Track / Ruler**: The coordinate ruler at the top of the browser
graphic showing the genomic position scale. Clicking and dragging on the ruler
activates the drag-and-select zoom feature.

**Chromosome Ideogram**: A graphical representation of the entire chromosome
shown above the browser graphic (for assemblies with cytological banding data).
A red box indicates the currently viewed region's location on the chromosome.

**Scale Bar**: A reference bar in the center of the browser graphic showing the
current viewing scale in bases, kilobases, or megabases.

**Track Label (Long Label)**: The descriptive text displayed at the left edge
of each track in the browser graphic (e.g., "GENCODE V41 Comprehensive
Transcript Annotation").

**Short Label**: The abbreviated track name shown in the track controls section
below the browser graphic.

**Track Control / Visibility Menu**: The drop-down menus below the browser
graphic that control each track's display mode (`hide`, `dense`, `squish`,
`pack`, `full`).

**Minibutton**: The small gray button to the left of each displayed track.
Clicking it opens the track's configuration/settings page.

**Track Groups**: Categories that organize related tracks together below the
browser graphic (e.g., "Genes and Gene Predictions," "Mapping and Sequencing,"
"Regulation").

### Navigation Controls

**Position/Search Box**: The text field at the top of the page where you enter
coordinates, gene names, accession numbers, rsIDs, HGVS terms, or DNA sequences
to navigate to specific locations.

**Zoom Buttons**: Controls above and below the browser graphic for zooming in
(`1.5x`, `3x`, `10x`, `base`) or out (`1.5x`, `3x`, `10x`, `100x`) on the
current view.

**Move/Pan Buttons**: Arrow buttons for shifting the view left or right along
the chromosome while maintaining the current zoom level.

**Reverse Button**: Flips the browser display to show the negative strand (3'
to 5') instead of the default forward strand (5' to 3').

**Next/Prev Item Navigation**: Gray double-headed arrows that appear at the ends
of track items (when enabled in configuration) allowing you to jump to the next or
previous feature in that track.

**Keyboard Shortcuts**: Many Genome Browser interactions can be activated using
keyboard shortcuts (e.g. "vd" to view DNA sequence of current window). See all
keyboard shortcuts by typing "?".

### Mouse Interactions

**Right-Click Context Menu**: A context-sensitive menu that appears when you
right-click on any item in the browser graphic. Options include zooming to the
full item, highlighting, getting DNA sequence, viewing the details page, and
accessing track configuration.

**Click on Item**: Clicking on a feature (gene, SNP, etc.) in the browser
graphic opens its details page with comprehensive information and external
links.

**Drag-and-Reorder**: Click and drag tracks vertically to rearrange their
display order in the browser graphic.

**Drag-and-Scroll (Pan)**: Click and drag anywhere on the browser graphic
(except the ruler) to scroll the view horizontally left or right.

**Drag-and-Select (Drag-and-Zoom)**: Click and drag on the ruler/base position
track to select a region, then choose to zoom into that region. Hold Shift
while dragging elsewhere on the image to activate this feature outside the
ruler.

**Highlight**: A colored vertical band that can be added to mark regions of
interest. Created via the drag-and-select popup menu or right-click menu.
Multiple highlights can be added with different colors.

### Position and Search

**Position/Search box**: Text entry box at the top of the main genome genome browser image. Accepts
positions or one of a variety of search terms, including gene names, rsIDs, short sequences and
[various other terms](/goldenPath/help/query.html).

**Autocomplete**: For assemblies with gene annotations, the position search box
offers autocomplete suggestions as you type gene symbols.

**[Track Search](/cgi-bin/hgTracks?hgt_tSearch=track+search)**: A feature for
finding tracks by searching their names, descriptions, and metadata. Accessed
via the Genome Browser menu or a button below the graphic.

### Configuration and Settings

**[Configure Button](/cgi-bin/hgTracks?hgTracksConfigPage=configure)**: Opens
the Track Configuration page where you can adjust global display settings
including image width, text size, font, and gridlines.

**Track Settings Page**: The detailed configuration page for an individual
track, accessed by clicking the track's minibutton or name. Allows filtering,
coloring, and display customization.

**Default Tracks Button**: Resets all track visibility settings to their
default states for the current assembly.

**Hide All Button**: Sets all tracks to hidden, clearing the display.

**Image Width**: A configurable setting (in pixels) controlling the horizontal
size of the browser graphic. Larger widths show more genomic territory without
scrolling.

**Gridlines**: Optional light blue vertical lines in the browser graphic that
help align features across tracks. Can be toggled on/off in configuration.

**Reset All User Settings**: Under top navigation menu "Genome Browser", clears all customizations
including track visibility, custom tracks, and hubs, returning the browser
to its original default state.

### Views, Output, and Export

**Recommended Track Sets**: Under top navigation menu "Genome Browser". Allows
users to enable a set of recommended tracks for tasks such as clinical variant
evaluation.

**View Menu**: A top navigation menu providing options like viewing DNA
sequence, converting coordinates to other assemblies, and accessing
PDF/PostScript output.

**Get DNA**: A feature to retrieve the genomic DNA sequence for the current
viewing region or for a specific track item. Accessible via the View menu or
right-click context menu.

**PDF/PS Output**: Options under the View menu to generate publication-quality
vector graphics of the browser display.

## Tracks and Display

**Track**: A horizontal row in the Genome Browser display showing a specific
type of annotation data (e.g., genes, SNPs, conservation scores). Each track
can be configured for different display modes.

**Track Group**: A set of related tracks grouped together under the main track
image, e.g. "Mapping and Sequencing" or "Comparative Genomics".

### Display Modes

| Mode | Description |
|------|-------------|
| `hide` | Track is not displayed |
| `dense` | All features collapsed into a single line |
| `squish` | Features shown at reduced height |
| `pack` | Features shown at full height, labeled when space permits |
| `full` | Features shown at full height with all labels |


**Composite Track**: A container that groups related tracks together (e.g.,
RNA-seq replicates), allowing them to be managed collectively. Indicated in the
track groups by a folder icon.

**MultiWig**: A special composite display mode that overlays multiple
wiggle-format data tracks in a single graphical area. See, for example,
the "Layered H3K4Me1" track under the "ENCODE Regulation" supertrack.

**Supertrack**: A higher-level grouping of composite tracks or individual
tracks into a collapsible folder structure. Indicated in the
track groups by a folder icon.

## User Data Features

**[Custom Tracks](/cgi-bin/hgCustom)**: User-uploaded annotation data displayed
temporarily in the browser (expires after 48 hours of inactivity unless saved
in a session). See [custom track
documentation](/goldenPath/help/customTrack.html) to learn how to load your
custom tracks and accepted formats.

**[Track Hub](/cgi-bin/hgHubConnect#unlistedHubs)**: A collection of remotely hosted
annotation files that can be connected to the browser via a `hub.txt`
configuration file. Provides more stable and configurable data display than
custom tracks. Will show up as its own group under the main genome browser image.
See our [hub basics page](/docs/hubs/hubBasics.html) for
help creating your own or our [track hub
documentation](/goldenPath/help/hgTrackHubHelp.html) for a full description
of the format.

**[Assembly Hub](/goldenPath/help/assemblyHubHelp.html)**: A track hub that
includes a custom genome assembly (in twoBit format) along with annotation
tracks.

**[Public Hub](/cgi-bin/hgHubConnect#publicHubs)**: A track or assembly hub
provided by an external group. Will show up as its own group under the main
genome browser image. Questions about track data should be directed to the hub
maintainers, whoe email address can be found on the description page for 
any track in the hub. Public hubs are required to me a set of 
[guidelines](/goldenPath/help/publicHubGuidelines.html) and are reviewed by 
Genome Browser staff before being added to the list.

**[Hub Space/Hub Upload](/cgi-bin/hgHubConnect#hubUpload)**: The UCSC Genome
Browser provides up to 10 GB of space for those with Genome Browser accounts
to store custom track and hub data.

**[Hub Development](/cgi-bin/hgHubConnect#hubDeveloper)**: Configuration settings
useful when developing a new hub. Provides an interface for checking a hub for
configuration issues.

**[Sessions](/cgi-bin/hgSession)**: A saved snapshot of browser configuration
including track visibility settings, position, custom tracks, and hubs. Can be shared
via URL. See [sessions documentation](/goldenPath/help/hgSessionHelp.html).

**[Public Sessions](/cgi-bin/hgPublicSessions)**: User-created sessions made
publicly available for others to view. 

## Data Formats

**[BED (Browser Extensible Data)](/FAQ/FAQformat.html#format1)**: A
tab-delimited format for defining genomic regions. Minimum 3 columns
(chromosome, start, end); can extend to 12+ columns including name, score,
strand, and exon structure.

**[bigBed](/goldenPath/help/bigBed.html)**: A compressed, indexed binary
version of BED format enabling efficient random access for large datasets.
Custom AutoSQL (.as) files allow it to be extended to any number of columns
containing item details, sequence, tables, and more.

**[WIG (Wiggle)](/goldenPath/help/wiggle.html)**: A format for
continuous-valued data displayed as graphs (e.g., conservation scores, read
coverage).

**[bigWig](/goldenPath/help/bigWig.html)**: A compressed, indexed binary
version of WIG format for large continuous data tracks.

**[bedGraph](/goldenPath/help/bedgraph.html)**: A format for displaying
continuous data where each line specifies a chromosome region and associated
value. Similar to WIG but preserves original data on export.

**[BAM (Binary Alignment/Map)](/goldenPath/help/bam.html)**: A compressed
binary format for storing sequence alignment data. Requires a separate `.bai`
index file.

**[CRAM](/goldenPath/help/cram.html)**: A more compressed alternative to BAM
that references an external genome sequence file.

**[VCF (Variant Call Format)](/goldenPath/help/vcf.html)**: A standard format
for storing genetic variant data including SNPs, insertions, and deletions.

**[PSL](/FAQ/FAQformat.html#format2)**: A format for storing sequence
alignments, commonly used for BLAT output and mRNA/EST alignments.

**[MAF (Multiple Alignment Format)](/FAQ/FAQformat.html#format5)**: A format
for storing multiple sequence alignments across species.

**[twoBit](/FAQ/FAQformat.html#format7)**: An efficient binary format for
storing genomic sequence data.

See our [format page](/FAQ/FAQformat.html) for a full listing of track and data types. 

## Genome Browser Data and Annotations
### Gene Annotations

**[GENCODE](https://www.gencodegenes.org/)**: The reference gene annotation for
human and mouse genomes, combining manual curation with computational
predictions. Includes protein-coding genes, non-coding RNAs, and pseudogenes.

**[RefSeq](https://www.ncbi.nlm.nih.gov/refseq/)**: NCBI's curated collection
of reference sequences for genes, transcripts, and proteins.

**[Ensembl Genes](https://www.ensembl.org/)**: Gene predictions from the
Ensembl project, available for many species.

**UCSC Genes**: UCSC's gene track built by integrating data from RefSeq and GenBank
among other sources with extensive metadata and external database links. Now retired
and replaced by GENCODE genes. 

**Exon**: A coding or untranslated region of a gene that is retained in the
mature mRNA after splicing. Displayed as thick boxes in gene tracks.

**Intron**: A region within a gene that is removed during RNA splicing.
Displayed as thin lines connecting exons. Chevrons indicate direction of
transcription.

**UTR (Untranslated Region)**: Portions of mRNA at the 5' and 3' ends that do
not code for protein. Displayed as half-height boxes in gene tracks.

**CDS (Coding Sequence)**: The portion of a gene or mRNA that codes for
protein, from start codon to stop codon.


### Conservation and Comparative Genomics

**[phastCons](/goldenPath/help/phastCons.html)**: A conservation scoring method
that calculates the probability that each base is in an evolutionarily
conserved element, using a phylogenetic hidden Markov model. Scores range from
0 to 1. Typically found alongside phyloP scores and a Multiz multiple alignment.

**phyloP**: A conservation scoring method that measures evolutionary rates at
individual bases compared to a neutral model. Positive scores indicate
conservation; negative scores indicate faster-than-expected evolution.
Typically found alongside phastCons scores and a Multiz multiple alignment.

**Multiz**: An algorithm for creating multiple genome alignments from pairwise
alignments. Subsequent multiple alignments are displayed in the Genome Browser
in MAF format. Typically found alongside phastCons and phyloP scores.

**[Chain](/goldenPath/help/chain.html)**: A series of gapless aligned blocks between two genomes, representing
alignable regions.

**[Net](/goldenPath/help/net.html)**: A hierarchical arrangement of chains representing syntenic (same
genomic context) alignments between genomes, with the highest-scoring chains
filling each region. More details about net construction can be found in
[this FAQ](/FAQ/FAQtracks#tracks24).

**Conservation Track**: A composite track displaying multiple species alignments and conservation scores (phastCons and phyloP) computed from those alignments.


### Variants and Clinical Data

**SNP (Single Nucleotide Polymorphism)**: A single base position where
different alleles exist in a population.

**[dbSNP](https://www.ncbi.nlm.nih.gov/snp/)**: NCBI's database of genetic
variation, displayed as SNP tracks in the browser.

**rsID**: A reference SNP identifier from dbSNP (e.g., `rs12345`).

**[ClinVar](https://www.ncbi.nlm.nih.gov/clinvar/)**: NCBI's database of
clinically significant genetic variants and their relationship to disease.

**[gnomAD (Genome Aggregation Database)](https://gnomad.broadinstitute.org/)**:
A resource of exome and genome sequencing data from large populations,
providing allele frequencies.

**HGVS Nomenclature**: A standardized system for describing sequence variants
(e.g., `NM_004006.2:c.4375C>T`). Accepted in the position/search box.

### Regulatory and Functional Data

**[ENCODE (Encyclopedia of DNA Elements)](/ENCODE/)**: A consortium project
identifying all functional elements in the human genome, including regulatory
regions.

**cCRE (Candidate Cis-Regulatory Element)**: Regions identified by ENCODE as
potential regulatory elements based on epigenomic data.

**DNase Hypersensitivity**: Regions of open chromatin accessible to DNase I
enzyme, indicating potential regulatory activity.

**ChIP-seq**: Chromatin immunoprecipitation followed by sequencing, used to
identify protein-DNA interactions.

**CpG Islands**: Genomic regions with high frequency of CpG dinucleotides, often
found near gene promoters.

**[GTEx (Genotype-Tissue Expression)](/gtex.html)**: A project providing gene
expression data across multiple human tissues.

**[FANTOM5](https://fantom.gsc.riken.jp/5)**: A project mapping transcription
start sites and promoter activity across cell types and tissues.

## Technical Terms

**Byte-Range Requests**: HTTP feature required for hosting bigBed, bigWig, and
BAM files, allowing the browser to fetch only the portion of a file needed for
the current view.

**MariaDb/MySQL**: The relational database system underlying the Genome
Browser's data storage.

**[REST API](/goldenPath/help/api.html)**: A programming interface for
retrieving Genome Browser data in JSON format.

**[trackDb](/goldenPath/help/trackDb/trackDbHub.html)**: A configuration file
(`trackDb.txt`) that defines track properties in a track hub, including display
settings, colors, and metadata.

**AutoSql**: A schema definition format used to describe custom fields in
Genome Browser tables and bigBed files.

**hubCheck**: A command-line utility for validating track hub configuration files.
Available from our
[download server](https://hgdownload.gi.ucsc.edu/downloads.html#utilities_downloads.)

