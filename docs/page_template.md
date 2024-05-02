% Genome Browser Tutorials

# Page Title Here! e.g. Introduction to the Genome Browser

Very short explanation of what the page will show users. Try to keep it to a
couple of sentences and maybe a bulleted list.

# Sections

Use the links in the panels below to go to different instructional materials.

The panels grow/shrink depending on the amount of text in them. This means that
if one box has more text that the rest, it is going to be larger. To prevent this
keep the text in each panel the same length. For reference, the panels in this
example are the same size in a 1000px window.

<div class="row" style="padding-top: 15px">
<div class="col-md-4">
<div class="panel panel-default" style="padding-bottom: 10px">
<h3 class="panel-title" style="width: 100%;"
>Panel 1</h3>

**Be sure to change the panel title.** 

This panel should be for the the annotated screenshot. The link at the bottom
takes people to that screenshot. Have sentence telling people what they're
going to see.

<p style="text-align: end">
<button>[View](#annotated-screenshot)</button>
</p>
</div>
</div>
<div class="col-md-4">
<div class="panel panel-default" style="padding-bottom: 10px">
<h3 class="panel-title" style="width: 100%;"
>Panel 2</h3>

**Be sure to change the panel title.** 
 
This panel should be for the guided workthrough. The link at the bottom takes
people to that workthrough. Have a sentence telling people what's coming up.

<p style="text-align: end">
<button>[View](#guided-workthorugh)</button>
</p>
</div>
</div>
<div class="col-md-4">
<div class="panel panel-default" style="padding-bottom: 10px">
<h3 class="panel-title" style="width: 100%;"
>Panel 3</h3>

**Be sure to change the panel title.** 

This is intended to take people to an interactive tutorial. This is currently
only available on hgTracks, but adding it to the other CGIs has been discussed.
If there is no interactive tutorial, then... TBD

<p style="text-align: end">
<button>[View](../cgi-bin/hgTracks?#showTutorial)</button>
</p>
</div>
</div>
</div>

# Annotated Screenshot

<!-- 
You will need to resize your images to fit on the page. There is not a way to
do so using Markdown right now. This can be done with a command like (change
the number after "resize" to change the image size):
convert annot-screenshot-sarCov2.png -sampling-factor 4:2:0 -strip -quality 85 -interlace JPEG -colorspace sRGB -resize 650^ annot-screenshot-sarCov2.jpg
-->

<!-- in markdown the [] are necessary for the image to display, but you can
leave them empty to skip having an image caption -->
<!--
![](../images/annot-screenshot-sarCov2.jpg)
-->
<!--
You can put something between the square brackets, it just shows up under
the image as a caption.
-->

<!--
max-width:100% makes the image take up the entire box
-->
<img src="../images/annot-screenshot-sarCov2.jpg" alt="Genome selection" style="max-width:100%;">


| Item | Explanation |
|------|-------------|
| Menu | Find BLAH   |

(We need something below the table otherwise later markdown is not properly interpreted)

# Guided Workthorugh

Introduce what this workthrough is going to cover again. 

Next provide a list of learning objectives in the form of a bulleted list. You
can use these to help structure the rest of the workthrough.

For example, it might something like:

By the end of this tutorial, you should be able to:

1. Naviagte the Genome Browser Interface
2. Perform a basic Table Browser query
3. Perform a simple BLAT query

## Goal 1 (replace this with a couple words, e.g. Genome Browser Interface)

Introduce overall flow of this section.

### Step 1: (few description of the step, e.g. Selecting a Genome)

<!--
We are going to use bootstrap columns to put the image/text side by side
Alternate the images left/right between different sections, mostly for aesthetics
--->

<div class="row">
<div class="col-md-6">
<img src="../images/GenomeSelect.jpg" alt="Genome selection" style="max-width:100%;">

<!--
![markdown image syntax](../images/GenomeSelect.jpg)
-->

</div>
<div class="col-md-6">

Go to BLAH, select your genome from the diagram or search for in the search box at the top left of the screen.

</div>
</div>

<!-- note to self, something seems off with the image handling here? like too wide and not auto-fitting to the col -->

### Step 2: Navigate to your gene of interest

<div class="row">
<div class="col-md-6">

After selecting a genome, enter a gene name into the search box. In our case,
we will enter "HUNK", a gene for BLAH, into the search box. 

After typing in the name, select the gene name "HUNK" from the list that
appears below the search box. You will be taken directly to that gene location. 
</div>
<div class="col-md-6">

<img src="../images/GeneSearch.jpg" alt="Searching for a specific gene name" style="max-width:100%;">

</div>



</div>

### Step 3: Configure the Genome Browser tracks

The horizontal lines of genomic data in the UCSC Genome Browser are known as **tracks**. they may BLAH, BLAH, and BLAH


### Goal 2

Adjust the number of sections based on the number of learning objectives for that page.

#### Step 1:

BLAH

#### Step 2:

BLAH

#### Step 3:

BLAH

### Goal 3

In this section of the template, we're going to cover some of the other
bootstrap functionality that could be useful for the Genome Browser tutorial
page.

#### Step 1:

<div class="well panel-danger">
This is a bootstrap well. It could be useful if you wanted to highlight something specific.
</div>

#### Step 2:

BLAH

#### Step 3:

BLAH
