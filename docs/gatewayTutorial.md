% UCSC Genome Browser Gateway tutorial

The [UCSC Genome Browser Gateway](../cgi-bin/hgGateway) page is a tool for finding and accessing
genome assemblies.  It features two search boxes: one for selecting an assembly and another for
specifying a genomic position. This tutorial will guide you through various features of the
Gateway page, including:

- Finding a genome browser using the Popular Species option
- Exploring the UCSC Species Tree
- Searching for an assembly using different identifiers
- Viewing sequences
- Using search terms to jump to a specific genome location

## Learning materials

<div class="row" style="padding-top: 15px">
<div class="col-md-4">
<div class="panel panel-default" style="padding-bottom: 10px">
<h3 class="panel-title" style="width: 100%;"
>Gateway Page Screenshot</h3>

A Gateway page screenshot highlighting its main features and functionalities.

<p style="text-align: end">
<button>[View](#gateway-page-screenshot)</button>
</p>
</div>
</div>

<div class="col-md-4">
<div class="panel panel-default" style="padding-bottom: 10px">
<h3 class="panel-title" style="width: -webkit-fill-available;"
>Guided Walkthrough</h3>

A guided walkthrough that explains how to find a genome, explore the Species Tree, search with
assembly identifiers, view sequences, and use search terms to jump to genome locations.

<p style="text-align: end">
<button>[View](#guided-walkthrough)</button>
</p>
</div>
</div>

<div class="col-md-4">
<div class="panel panel-default" style="padding-bottom: 10px">
<h3 class="panel-title" style="width: -webkit-fill-available;"
>Interactive Tutorial</h3>

An interactive tutorial that covers the basic Browser introduction on the Gateway page.

<p style="text-align: end">
<button>[View](../cgi-bin/hgGateway?db=hg38&startGateway=true)</button>
</p>
</div>
</div>
</div>

## Gateway page screenshot

```image
src=../images/gatewayPageAnnotated.jpg
width=90%
```

## Guided Walkthrough

### Using the Popular Species Option

<div class="row">
<div class="col-md-6">

```image
src=../images/popular_species.png
width=70%
```

</div>
<div class="col-md-6">

The **Popular Species** section lists commonly used model organisms, allowing for quick selection of
their genome browsers. Clicking on a species will display the default assembly version for that
organism. 

</div>
</div>

---
<div class="row">
<div class="col-md-6">

<a target="_blank" href='https://www.ncbi.nlm.nih.gov/datasets/docs/v2/glossary/' title=''>NCBI</a>
defines an assembly or assembled genome as the set of chromosomes, unlocalized and
unplaced (sometimes called &quot;random&quot;) and alternate sequences used to represent an
organism&apos;s genome. The
<a target="_blank"
href='https://www.ncbi.nlm.nih.gov/datasets/docs/v2/policies-annotation/data-model/'
title=''>NCBI Assembly Data Model</a> defines assemblies as comprising one or more
assembly units. 

The different assemblies often differ in their sequence content, with newer versions using newer
technologies to fill in gaps, correct errors, and refine genome structure. Some regions of an older
assembly may shift or change, while sequencing errors from previous data may be corrected in an
updated version. For example, the updated reference genome hg38 contains many improvements over the
hg19 assembly, some of which may include contigs that were merged together or placed where there
were previously sequence gaps.

To change the assembly version, click the **Assembly** option under **Find Position**.

</div>
<div class="col-md-6">


```image
src=../images/assembly_version.png
width=90%
```

</div>
</div>

---
### Using the search box

<div class="row">
<div class="col-md-6">

The search box allows users to find genome assemblies by entering different types of queries:

Searching by **species name**: Ovis aries

Searching by  **common name**: dog

Searching by  **GC accession number**: GCF_016699485.2


</div>
<div class="col-md-6">

```image
src=../images/Search_GCF_016699485_2.gif
width=100%
```


</div>
</div>
---

### Exploring the UCSC Species Tree

<div class="row">
<div class="col-md-6">

```image
src=../images/species_tree.png
width=60%
```

</div>
<div class="col-md-6">

The **Species Tree** option displays a phylogenetic tree that can be navigated using scrolling or
by clicking different parts of the tree. Hovering over a branch will reveal the lineage branch name.

**Note**: The Species Tree does not include all available Genome Browser assemblies. The process of
adding Genome Browsers has been streamlined, enabling rapid releases but omitting certain previous
features, such as inclusion in the Species Tree. To find a specific assembly, use the assembly
search box.
</div>
</div>
---

### Requesting an Assembly

<div class="row">
<div class="col-md-6">

If a desired assembly is not listed, you can request it by clicking **&quot;Unable to find a
genome? Send us a request**&quot;. This will direct you to the **Genome Assembly Search and Request
page.**

Steps to request an assembly:

1. Enter the **species name, common name, or GC accession number** of the assembly.
2. Click the **request button**.
3. Fill out the required information on the submission page.

</div>
<div class="col-md-6">

```image
src=../images/assembly_request.png
width=130%
```

```image
src=../images/request_page.png
width=40%
```

</div>
</div>
---

### Using View sequences

<div class="row">
<div class="col-md-6">

```image
src=../images/view_sequence.png
width=30%
```

```image
src=../images/sequences_list.png
width=70%
```

</div>
<div class="col-md-6">

Clicking the **View sequences** link directs users to the **Assembly Browser Sequences** page. This
page displays information about chromosomes, sequences, and contigs for the selected assembly.

The third column displays alias sequence names, while subsequent columns show alternate naming
schemes. These include:

- **assembly** - Names from NCBI&apos;s assembly_report.txt file.
- **genbank** - INSDC names.
- **refseq** - Names from RefSeq annotations.

</div>
</div>
---

### Assembly details on the Gateway Page

<div class="row">
<div class="col-md-6">

The **Gateway Page** provides various details about an assembly, including:

- UCSC Genome Browser assembly ID
- Common name
- Taxonomic name
- Sequencing/Assembly provider ID
- Assembly date
- Assembly type
- Assembly level
- Biosample
- Assembly accession
- NCBI Genome ID
- NCBI Assembly ID
- BioProject ID

The Gateway page also offers  **download links** for data files related to the genome assembly.

</div>
<div class="col-md-6">

```image
src=../images/assembly_details.png
width=90%
```
</div>
</div>

----

<div class="row">
<div class="col-md-6">
</div>
</div>

### Jumping to a Specific Genome Location

<div class="row">
<div class="col-md-6">

```image
src=../images/Search_box.png
width=70%
```

</div>
<div class="col-md-6">
Once an assembly is selected, users can search for specific genome locations using:

- Genome positions (e.g., chr1:1000000-2000000)
- Gene names (e.g., BRCA1)

For more details on valid position queries, visit the [Querying the Genome Browser](/goldenPath/help/query.html) page.

</div>
</div>

