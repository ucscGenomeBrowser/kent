# Boxplot of data values by category (bar in barChart)
#       usage:   Rscript barChartBoxplot.R itemName units colorFile dataFile outFile altName
# where:
#       colorFile is a tab-sep file in the format: category hex-color
#       dataFile is a tab-sep file, with a header and row for each sample in the format:
#               sample  category  value
#       outFile is filename suitable for writing graphic (e.g. in PNG format)

args <- commandArgs(TRUE)
locus <- args[1]
units <- args[2]
colorFile <- args[3]
dataFile <- args[4]
outFile <- args[5]

name2 <- args[6]

# read colors file
colorDf = read.table(colorFile, sep="\t", header=TRUE)
colorsHex <- paste("#",as.character(as.hexmode(colorDf$color)), sep="")

# order categories as in colors file
df <- read.table(dataFile, sep="\t", header=TRUE)
df$category <- ordered(df$category, levels=colorDf$category)
labels <- names(table(df$category))
count <- length(labels)

# draw graph
# adjust X label height based on length of labels
# TODO: scale by number of categories ?
png(file=outFile, width=1070, height=600)
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
par(mar=c(marBottom,marLeft,3,1) + 0.1, mgp=c(2,1,0), font.main=1)
yLabel <- units
max <- max(df$value)
yLimit <- c(-(max*.02), max+ (max*.03))
title <- locus
if (name2 != "n/a")
    title <- paste(locus, " (", name2, ")", sep="")

if (length(unique(df$category)) == length(df$category)) {
    boxPlot <- FALSE
} else {
    boxPlot <- TRUE
}
if (boxPlot) {
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
                        # erase X axis labels (add later rotated)
                        names=rep(c(""), count))
    # draw sample counts
    y1 <- par("usr")[3]
    size <- .8
    adjust <- .4*abs(y1)
    text(.4, y1 + adjust, "N=", cex=size, xpd=TRUE)
    text(1:count, y1 + adjust, exprPlot$n, cex=size, col="black")
    text(1:count, y1 + adjust, exprPlot$n, cex=size, col=colorsHex)

    # add sample (X axis) labels at 45 degree angle
    rot <- 45
    size <- 1
    adjust <- .7 * abs(y1)
    text(1:count, y1 - adjust, cex=size, labels=labels, srt=rot, xpd=TRUE, adj=c(1,.5), col="black")
    text(1:count, y1 - adjust, cex=size, labels=labels, srt=rot, xpd=TRUE, adj=c(1,.5),
            col=colorsHex)
} else {
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
    rect(xmin, ymin + adjust, xmax, ymax, border=c(darkgray), lwd=2,xpd=TRUE)

    # add sample (X axis) labels at 45 degree angle
    rot <- 45
    size <- 1
    text(exprPlot, ymin - adjust, cex=size, labels=labels, srt=rot, xpd=TRUE,
        adj=c(1,.5), col="black")
    text(exprPlot, ymin - adjust, cex=size, labels=labels, srt=rot, xpd=TRUE,
        adj=c(1,.5) ,col=colorsHex)
}

graphics.off()
