# Boxplot of data values by category (bar in barChart)
#       usage:   Rscript barChartBoxplot.R itemName units colorFile dataFile outFile altName
# where:
#       colorFile is a tab-sep file in the format: category hex-color
#       dataFile is a tab-sep file, with a header and row for each sample in the format:
#               sample  category  value
#       outFile is filename suitable for writing graphic (e.g. in PNG format)


# if more than this categories, switch to a display mode that is easier to read:
# - instead of plotting boxplot from left to right, plot from top to bottom
# - box plot labels are horizontal
# - labels and number are printed in black, not colored

DENSECUTOFF <- 54

drawBoxPlot <- function(df) {
    # it's not easy at all in R to add optional arguments via a list object, so duplicate code
    if (count < DENSECUTOFF) {
        # old-style vertical bars with labels at a 45 degree angle. Harder to read but saves screen space
        yLimit <- c(-(max*.02), max+ (max*.03))
        par(mar=c(marBottom,marLeft,3,1) + 0.1, mgp=c(2,1,0), font.main=1)

        exprPlot <- boxplot(value ~ df$category, data=df, ylab=yLabel, ylim=yLimit,
                        main=title, 
                        col=colorsHex, border=c(darkgray),
                        # medians
                        medcol="black", medlwd=2,
                        # outliers
                        outcex=.5, outcol=darkgray,
                        # whiskers
                        whisklty=1, whiskcol=gray, 
                        # 'staples'
                        staplecol=gray, staplecex=1.3, staplewex=.8,
                        # with R versions 3.6 and later must suppress x-labels manually
                        xlab="",
                        # erase X axis labels (add later rotated)
                        names=rep(c(""), count))

        y1 <- par("usr")[3]
        size <- .8
        # draw sample count twice (once in color, once in black) to make light colors readable
        adjust <- .4*abs(y1)
        text(.4, y1 + adjust, "N=", cex=size, xpd=TRUE)
        text(1:count, y1 + adjust, exprPlot$n, cex=size, col=colorsHex)
        text(1:count, y1 + adjust, exprPlot$n, cex=size, col="black")
        # add sample (X axis) labels at 45 degree angle
        rot <- 45
        size <- 1
        text(1:count, y1 - adjust, cex=size, labels=labels, srt=rot, xpd=TRUE, adj=c(1,.5), col="black")
        text(1:count, y1 - adjust, cex=size, labels=labels, srt=rot, xpd=TRUE, adj=c(1,.5), col=colorsHex)
    }
    else { 
        # new-style vertical bars, easier to read
        size <- .7
        longestLabelIdx = which.max(nchar(labels))
        longestLabel = labels[longestLabelIdx]
        marLeft = nchar(longestLabel)/2.5 # 2.5 was trial and error, strwidth(longestLabel, "inches") didn't work
        par(mar=c(3,marLeft,2,1) + 0.1, mgp=c(2,1,0), font.main=1)
        yLimit <- c(-(max*.02), max+ (max*.03))
        # by default, the order of horizontal barcharts is reversed compared to vertical barcharts
        # this is why we inverse the order with at=rev(...) and later for text() apply rev(...) on all y-coords
        exprPlot <- boxplot(value ~ df$category, data=df, ylim=yLimit, xlim=c(0, count),
                        at=rev(1:nlevels(df$category)),
                        main=title, 
                        horizontal=TRUE,
                        space = 0.3,
                        col=colorsHex, border=c(darkgray),
                        # medians
                        medcol="black", medlwd=2,
                        # outliers
                        outcex=.5, outcol=darkgray,
                        # whiskers
                        whisklty=1, whiskcol=gray, 
                        # 'staples'
                        staplecol=gray, staplecex=1.3, staplewex=.8,
                        # erase y ticks
                        yaxt="n",
                        # with R versions 3.6 and later must suppress x-labels manually
                        xlab="", ylab="",
                        # erase X axis labels (add later rotated)
                        names=rep(c(""), count))
        y1 <- par("usr")[1]
        # draw numbers only black, at 90 degrees and left-adjusted
        rot <- 0
        adjust <- .15*abs(y1)
        text(y1 + adjust, rev(1:count), exprPlot$n, cex=size, col="black", srt=rot, adj=0.0)
        # draw labels
        size <- .9
        adjust <- .4*abs(y1)
        text(y1-adjust, rev(1:count), cex=size, labels=labels, srt=rot, xpd=TRUE, adj=c(1,.5), col="black")
    }
}

drawBarPlot <- function(df) {
    yLimit <- c(-(max*.02), max+ (max*.03))
    par(mar=c(marBottom,marLeft,3,1) + 0.1, mgp=c(2.5,1,0), font.main=1)
    df <- df[(order(match(df$category, colorDf$category))),]
    exprPlot <- barplot(df$value, ylab=yLabel, ylim=yLimit,
                        main=title,
                        col=colorsHex, border=c(darkgray),
                        # erase X axis labels (add later rotated)
                        names.arg=rep(c(""), count))
    limits <- par("usr")
    xmin <- limits[1]
    xmax <- limits[2]
    ymin <- limits[3]
    ymax <- limits[4]
    adjust <- abs(ymin)

    # draw box around bargraph
    rect(xmin, ymin + adjust, xmax, ymax, border=c(darkgray), lwd=2, xpd=TRUE)

    # add sample (X axis) labels at 45 degree angle
    rot <- 45
    size <- 1
    text(exprPlot, ymin - adjust, cex=size, labels=labels, srt=rot, xpd=TRUE,
        adj=c(1,.5), col="black") # does this have any effect?
    text(exprPlot, ymin - adjust, cex=size, labels=labels, srt=rot, xpd=TRUE,
        adj=c(1,.5), col=colorsHex)
}

args <- commandArgs(TRUE)
locus <- args[1]
units <- args[2]
colorFile <- args[3]
dataFile <- args[4]
outFile <- args[5]
name2 <- args[6]
# this argument can be used by QA to check if the new fonts look very different from the old fonts
# see #22838
useOldFonts <- (!is.na(args[7]) && args[7]=="1")

if (!useOldFonts && require("showtext",character.only=TRUE, quietly=TRUE)) {
    library(showtext, quietly=TRUE)
    showtext_auto()
    showtext_opts(dpi = 72)
}

# read colors file
colorDf = read.table(colorFile, sep="\t", header=TRUE, comment.char="")
colorsHex <- paste("#",format(as.hexmode(colorDf$color), width=6), sep="")

# order categories as in colors file
df <- read.table(dataFile, sep="\t", header=TRUE, comment.char="")
# ensure colors df and data df have consistent spacing:
colorDf$category <- gsub("_", " ", colorDf$category)
df$category <- gsub("_", " ", df$category)
df$category <- ordered(df$category, levels=colorDf$category)
labels <- names(table(df$category))
count <- length(labels)

# draw graph
# adjust X label height based on length of labels
# TODO: scale by number of categories ?
if (count < DENSECUTOFF) {
    png(file=outFile, width=1070, height=600)
} else {
    png(file=outFile, width=1070, height=18*count)
}
# res=72 is default
gray <- "#A6A6A6"
darkgray <- "#737373"

# plot with customized margins, symbol and line styles and colors, in the style of
# GTEx gene expression plots (based on GTEx Portal at the Broad)
# adjust margins based on max label size
marBottom <- 10;
marLeft <- 4;
if (max(nchar(labels)) > 30) {
    # change margins to appropriately fit longest label name
    # left * .45 + 1 accounts for final rotation and length / 3 for length to margins ratio
    marBottom <- max(nchar(labels))/3;
    marLeft <- marLeft + (marLeft * .45 + 1);
}

yLabel <- units
max <- max(df$value)
title <- locus
labels = gsub("_", " ", labels)

if (name2 != "n/a")
    title <- paste(locus, " (", name2, ")", sep="")

if (length(unique(df$category)) == length(df$category)) {
    drawBarPlot(df)
} else {
    drawBoxPlot(df)
}

graphics.off()
