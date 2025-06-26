% UCSC Genome Browser Table Browser Tutorial

The [UCSC Table Browser](../cgi-bin/hgTables) is a flexible tool for accessing and exporting data from genome browser tracks. This tutorial introduces the Table Browser interface and demonstrates how to:

- Select a genome and assembly
- Choose a track and table
- Define a region of interest or use identifiers
- Customize output formats
- Download or view extracted results

## Learning materials

<div class="row" style="padding-top: 15px">
<div class="col-md-4">
<div class="panel panel-default" style="padding-bottom: 10px">
<h3 class="panel-title" style="width: 100%;">Table Browser Overview</h3>

A screenshot highlighting the layout and key elements of the Table Browser interface.

<p style="text-align: end">
<button>[View](#table-browser-overview)</button>
</p>
</div>
</div>

<div class="col-md-4">
<div class="panel panel-default" style="padding-bottom: 10px">
<h3 class="panel-title" style="width: -webkit-fill-available;">Guided Walkthrough</h3>

Step-by-step guidance for using the Table Browser to extract data for your analysis.

<p style="text-align: end">
<button>[View](#guided-walkthrough)</button>
</p>
</div>
</div>

<div class="col-md-4">
<div class="panel panel-default" style="padding-bottom: 10px">
<h3 class="panel-title" style="width: -webkit-fill-available;">Interactive Tutorial</h3>

An in-browser walkthrough that introduces the Table Browser interface and workflow.

<p style="text-align: end">
<button>[View](../cgi-bin/hgTables?db=hg38&startTutorial=true)</button>
</p>
</div>
</div>
</div>

## Table Browser Overview

``` image
src=../images/tableBrowserAnnotated.png
width=65%
```

---

## Guided Walkthrough


<div class="row">
  <div class="col-md-6">
### Step 1: Select Your Assembly
  
  Use the **Clade**, **Genome**, and **Assembly** menus to choose your reference genome.
  
  - *Clade*: Major organism group (e.g., Mammal, Vertebrate)
  - *Genome*: Species (e.g., Human, Mouse)
  - *Assembly*: Specific genome version (e.g., hg38)
  </div>

  <div class="col-md-6">
  ```image
  src=../images/assemblySelection.gif
  width=80%
  ```
</div>
</div>

---


<div class="row">
  <div class="col-md-6">
  ``` image
  src=../images/trackSelection.gif
  width=80%
  ```
  </div>

  <div class="col-md-6">
### Step 2: Select a Track
  Choose the data track you want to work with. The Table Browser will pre-select your most recent track, but you can change it.
  
  - Tracks are grouped similarly to those on the Genome Browser main page.
  - Use "All Tracks" for comprehensive options.
  </div>
</div>

---


<div class="row">
  <div class="col-md-6">
### Step 3: Select the Table

  Each track may have one or more associated tables that store the data. Use the **Table** menu to select the relevant one.
  
  Click the <button>Data format description</button> to explore:

  - Table layout
  - Related tables
  - Joinable fields

  Use "All Tables" to list all tables for the assembly.

  </div>

  <div class="col-md-6">
  ``` image
  src=../images/tableSelection.gif
  width=80%
  ```
  </div>
</div>

---


<div class="row">
  <div class="col-md-6">
  ``` image
  src=../images/defineRegions.gif
  width=80%
  ```
  </div>

  <div class="col-md-6">
### Step 4: Define a Genomic Region
  You can limit the output to a specific region or get data genome-wide.
  Whole-genome output may be unavailable for some tracks due to the large amount of data. 
  
  Options include:
  
  - Entering a position (e.g., `chr7:117199645-117356025`)
  - Typing a gene name and clicking <button>Lookup</button>
  - Using <button>Define regions</button> to upload/paste multiple coordinates
  
  </div>
</div>

---

<div class="row">
  <div class="col-md-6">
### Step 5: Select Output Format
  Use the **Output format** dropdown to choose what type of file or fields you want returned.
  
  Options include:
  
  - All fields from the table
  - Selected fields only
  - File formats like BED, GTF, or custom tracks
  </div>
  <div class="col-md-6">
  ``` image
  src=../images/tutorialImages/tableBrowserOutputDropDown.png
  width=80%
  ```
  </div>
</div>


---

<div class="row">
  <div class="col-md-6">
  ``` image
  src=../images/downloadData.gif
  width=80%
  ```
  </div>
  <div class="col-md-6">
### Step 6: Submit Your Query
  Click <button>Get output</button> to execute your query and view/download results.
  You can download results by entering a filename in the **output filename** field before clicking <button>Get output</button>.
  
  You can also click <button>Summary/statistics</button> to preview:
  
  - Record count
  - Base coverage
  - Item size ranges
  - Time to compute

  </div>
</div>

## Additional Help

- [Table Browser User Guide](https://genome.ucsc.edu/goldenPath/help/hgTablesHelp.html)
- [Contact UCSC](https://genome.ucsc.edu/contacts.html)
