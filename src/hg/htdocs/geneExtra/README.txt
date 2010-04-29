
Files in this directory provide added annotation to the details
pages of some genes.  This is based on gene name and so is not
organism-specific.  This is intended as a complement to some of
the creative names given to genes.  See this page for some
examples:
        
      http://home.earthlink.net/~misaak/taxonomy/taxGene.html

This directory (htdocs/geneExtra/) is examined for files named
after the gene name.  Image files named in the form $gene.png,
$gene.gif, or $gene.jpg will be displayed inline on the details
page.  Due to inconsistent capitalization in databases, gene
names should be all lower case.  If a file named in the form
$gene.txt exists, it contains an HTML fragment describing
something about the naming of the gene.

The image should have a height of around 150-200 pixels and
the HTML fragment should use <BR> tags to get the width at
around 30 characters.



