---
title: "Genome Browser Documentation"
---

Browse all UCSC Genome Browser documentation by topic.

New to the Genome Browser? Start with our [Genome Browser Basics
tutorial](/docs/tutorials/gb101.html), a 15-minute walkthrough of the
interface, navigation, and track display. You can also browse our
[glossary](/docs/genomeBrowserGlossary.html) to learn common terms used throughout
this documentation.

## Using the Genome Browser Interface and Tools

The Genome Browser offers a dynamic interface for visualizing genomic data on
a variety of genome assemblies. In addition to visualization, there are several tools
for extracting data from the current visualization.

### Tutorials
We offer detailed step-by-step tutorials for several of our tools:

| Title | Description | Link |
| - | - | - |
| Genome Browser Basics | Covers basics of the UCSC Genome Browser track interface | [View Tutorial](/docs/tutorials/gb101.html) |
| Gateway Page | Learn to find genome assemblies | [View Tutorial](/docs/tutorials/gatewayTutorial.html) |
| Table Browser | Introduces Table Browser queries | [View Tutorial](/docs/tutorials/tableBrowserTutorial.html) |
| Custom Tracks | Upload your custom annotations | [View Tutorial](/docs/tutorials/customTrackTutorial.html) |

Navigate to our [Tutorial index](/docs/tutorials/index.html) to see them all.

### Video tutorials
Our [training page](/training/index.html) has short how-to videos covering common
tasks like saving sessions, using BLAT and isPCR, finding SNPs in a gene, and
using Multi-Region View. We also offer in-person and virtual workshops,
[contact us](/docs/contacts.html) to host one at your institution.

## Understanding Genome Browser Assemblies and Annotations

The Genome Browser allows for the visualization of various annotations on
a wide variety of genome assemblies. A genome assembly is a reconstruction of
an organism's complete DNA sequence from shorter sequenced fragments. Different
assemblies of the same species may exist as sequencing technology improves
(e.g., hg19 vs. hg38 for human).

- Use our [gateway page](/cgi-bin/hgGateway) to access popular assemblies or
  search for an assembly of interest. Learn more from our
  [tutorial](/docs/tutorials/gatewayTutorial.html).
- If your assembly is not available, [request it](/assemblySearch.html).

The Genome Browser displays annotations as horizontal "tracks" layered beneath
the genome coordinate ruler. These tracks fall into several major categories:

- Genes and Gene Predictions: curated and predicted gene models (GENCODE,
  RefSeq, Augustus)
- Regulation: transcription factor binding, histone marks, DNase
  hypersensitivity (ENCODE, Roadmap Epigenomics)
- Comparative Genomics: cross-species conservation scores, alignments (PhyloP,
  PhastCons, chain/net)
- Variation: SNPs, structural variants, clinical variants (dbSNP, ClinVar,
  gnomAD)
- Repeats: repetitive elements (RepeatMasker, Simple Repeats)
- Mapping and Sequencing: assembly structure, gaps, chromosome bands

Each track has its own description page (accessible by clicking the track name)
with details on the data source, methods, and display conventions. Human and mouse
assemblies (e.g. hg38, hg19, mm10, or mm39) will include tracks across nearly
all of these categories. However, most assemblies may only have a basic set of
annotations (genes, repeats, assembly mapping).

Relevant FAQs:

- [Gene Tracks](/FAQ/FAQgenes.html)
- [Genome Browser Tracks](/FAQ/FAQtracks.html)


## Visualizing Your Own Data

The Genome Browser allows you to visualize your own data alongside the data we host:

- [Custom tracks](/cgi-bin/hgCustom)
    - See our [documentation](/goldenPath/help/customTrack.html) to learn how to load your own
- [Track hubs](/cgi-bin/hgHubConnect?#unlistedHubs)
    - See our [Hubs Basics](/docs/hubs/hubBasics.html) page to create your hub
    - Checked using our [hub development tools](/cgi-bin/hgHubConnect?#hubDeveloper)

For both hubs and custom tracks, we provide storage through [hub space](/cgi-bin/hgHubConnect?#hubUpload).

## Sharing Your Data

Once you've loaded and configured your data, you can share it with others
through several means:

- [Links](/FAQ/FAQlink.html)
- [Sessions](/cgi-bin/hgSession)
- [Public hubs](/cgi-bin/hgHubConnect?#publicHubs) (to contribute your own, see
  our [guidelines](/goldenPath/help/publicHubGuidelines.html))
- [Contributed tracks](/docs/hubs/contributedTracks.html) for GenArk assemblies

Relevant FAQs:

- [Data File Formats](/FAQ/FAQformat.html) - see the formats supported by the Genome Browser


## Downloading Data from the Genome Browser

Beyond data visualization, the Genome Browser offers a wide variety of ways to access
and work with the underlying data:

- The [download server](https://hgdownload.gi.ucsc.edu/downloads.html) provides
  access to the data underlying the Genome Browser,
  in addition to pairwise alignment and LiftOver chain files.
- Our [REST API](/goldenPath/help/api.html) can be used to retrieve data in JSON format.
- Our [public MySQL server](/goldenPath/help/mysql.html) allows command-line access to underlying data tables.
- We provide [command-line tools](https://hgdownload.gi.ucsc.edu/downloads.html#utilities_downloads)
  for working with bigBed, bigWig, or other formats,
  and command-line versions of our tools like `liftOver` or `blat`.

We also provide scripts for creating your own
[mirror](/goldenPath/help/mirror.html) of the Genome Browser software and
databases, although it is often recommended to explore other methods of
visualizing your custom data as mirrors can require significant effort and
storage space to set up and maintain.

Relevant FAQs:

- [Data and Downloads](/FAQ/FAQdownloads.html)
- [Mirroring or Licensing the Genome Browser](/FAQ/FAQlicense.html)
