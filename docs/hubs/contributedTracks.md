---
title: "Contributed Tracks"
---

Researchers are creating annotations for hundreds of assemblies
in one process, or creating a number of annotations for one assembly.
It would be useful to the wider research community to have these
annotations included in the default browser view on those assemblies.

This discussion outlines procedures for submitting a set of tracks to the
browser for inclusion in the default view of a genome assembly.
This procedure is for developers of tracks that have a number of tracks
to submit to one assembly, or a set of tracks to deliver to a multiple
set of genome assemblies.

Development process:

- Develop your annotations in a track hub under your control.
- Provide a reference to your development track hub to the genome browser
   for inclusion consideration.
- By following the suggested outline, the browser will be able to
   efficiently add your annotations to the genome browser system.

## Your track hub file/directory structure

```
+-- hub.txt
+-- genomes.txt
+-- documentation.html
+-- GCA_000260495.2/
|   +-- trackDb.txt
|   +-- veupathGenes.bb
|   +-- veupathGenes.gtf.gz
+-- GCF_014441545.1/
|   +-- trackDb.txt
|   +-- veupathGenes.gtf.gz
|   +-- veupathGenes.bb
+-- GCF_014108235.1/
|   +-- trackDb.txt
|   +-- veupathGenes.gtf.gz
|   +-- veupathGenes.bb
```

## Requirements

Assembly names need to be the NCBI GenBank accession names to
identify the corresponding assembly. **GCF\_...** or **GCA\_...**

All annotations must have appropriate documentation.

The track hub directory structure must be as described here.

## Example hub.txt file

```
hub PAG_2026_GenArk_Contrib_Example
shortLabel PAG 2026 GenArk tracks
longLabel PAG 2026 example of contributed tracks to UCSC GenArk assemblies
email genome@soe.ucsc.edu
genomesFile genomes.txt
descriptionUrl documentation.html

# comments:
# load this hub with a reference to this hub.txt file:
# https://genome-test.gi.ucsc.edu/~hiram/BRC/contrib/hub.txt

# for example, to show this track hub in the genome browser::

# https://genome.ucsc.edu/cgi-bin/hgTracks?genome=GCA_000260495.2&
#    hubUrl=https://genome-test.gi.ucsc.edu/~hiram/BRC/contrib/hub.txt
```

The **genomes.txt** file specifies the target assemblies.
The **descriptionUrl** link refers to **documentation.html** for
this track hub. Since the data is uniform for all the tracks,
a single **documentation.html** can be sufficient for all documentation.

## Example genomes.txt file

```
genome GCA_000260495.2
trackDb GCA_000260495.2/trackDb.txt

genome GCF_014441545.1
trackDb GCF_014441545.1/trackDb.txt

genome GCF_014108235.1
trackDb GCF_014108235.1/trackDb.txt
```

List each assembly for which you have calculated annotations.
The **trackDb.txt** file defines the tracks. To emphasize again,
the assembly names must be the GenBank accession identifier,
e.g.: **'GCF\_016077325.2'**. Refer to the [full
list](https://hgdownload.gi.ucsc.edu/hubs/UCSC_GI.assemblyHubList.txt) of all
GenArk repository assemblies to find the correct identifier for your genome.

## Example trackDb.txt file

```
track VEuPathDBGeneModels
shortLabel PAG 2026 contrib
longLabel PAG 2026 - demonstrating contributed tracks to UCSC GenArk assemblies
group genes
type bigBed 12 .
visibility pack
labelFields name
searchIndex name
bigDataUrl veupathGenes.bb
colorByStrand 0,0,122 157,60,32
dataVersion VEuPathDB release 68
html ../documentation

# can be multiple track definitions here for other annotations
```

Since all the annotations are the same type, a single **trackDb.txt**
file is sufficient for all assemblies. The **html** link refers
to **../documentation.html** as used in the **hub.txt** file.
For documentation structure, you can follow the typical UCSC track description
format with sections for: **Description**, **Methods**, **Credits**,
**References**. We provide an [example
template](/goldenPath/help/examples/hubExamples/templatePage.html), or see, for example, the
description page of a
[typical UCSC track](/cgi-bin/hgTrackUi?db=hg38&c=chrX&g=recombRate2).
