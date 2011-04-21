function old_cellToColor(cell)
{
if (match(cell, "Gm12878"))
    return "255,128,128";
else if (match(cell, "H1hesc"))
    return "255,212,128";
else if (match(cell, "Hsmm"))
    return "120,235,204";
else if (match(cell, "Huvec"))
    return "128,212,255";
else if (match(cell, "K562"))
    return "158,158,255";
else if (match(cell, "Nhek"))
    return "212,128,255";
else if (match(cell, "Nhlf"))
    return "255,128,212";
else
    return "0,0,0";
}

function cellToColor(cell)
{
if (match(cell, "Gm12878"))
    return "250,125,125";
else if (match(cell, "H1hesc"))
    return "200,150,100";
else if (match(cell, "K562"))
    return "150,150,250";
else
    return "0,0,0";
}

function assayToVals(assay)
{
if (match(assay, "DNAse") || match(assay, "Dnase"))
     return "0:175";
else if (match(assay, "RNA-seq"))
     return "0:80";
else if (match(assay, "H3K"))
     return "0:110";
else if (match(assay, "Phylo"))
     return "-0.1:0.6";
else
     return "0:100";
}

{
printf("track type=wiggle_0 name=%s visibility=full color=%s", $1, cellToColor($1));
printf(" maxHeightPixels=150:75:11 autoScale=off viewLimits=%s\n", assayToVals($1));
printf("fixedStep chrom=chr1 start=1 step=1 span=1\n");
for (i=2; i<= NF; ++i)
    print $i;
}
