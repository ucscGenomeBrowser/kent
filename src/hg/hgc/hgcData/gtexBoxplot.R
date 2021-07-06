# Boxplot of gene expression by tissue
#       usage:   Rscript geneBoxplot.R gene dataFile outFile log=<TRUE|FALSE>
# where dataFile is a tab-sep file, with a header and row for each sample in the format:
#       sample  tissue  rpkm
# and outFile is filename suitable for writing graphic (e.g. in PNG format)

args <- commandArgs(TRUE)
gene <- args[1]
dataFile <- args[2]
outFile <- args[3]
isLog <- args[4] == "log=TRUE"
isScoreOrder <- args[5] == "order=score"
version <- args[6]

# TODO: replace with V6 colors
# Currently, V6 and V8 tracks both use V4 colors.  V8 has one additional tissue (Kidney - Medulla)

if (version == "V8") {
    colorVersion <- "V8"
    units <- "TPM"
} else {
    colorVersion <- "V4"
    units <- "RPKM"
}

colorVersion <- if (version == "V8") "V8" else "V4"
colorFile <- paste0("hgcData/gtexColors", colorVersion, ".R")
source(colorFile)
# sets colorsHex, darkerColorsHex, tissueOrder

df <- read.table(dataFile, sep="\t", header=TRUE)
labels <- names(table(df$tissue))
count <- length(labels)
tissueColors <- data.frame(labels, colorsHex)

if (isScoreOrder) {
    # order boxes, Y labels and colors by median descending
    tissueMedian <- with(df, reorder(tissue, -rpkm, median))
    orderedLevels <- levels(tissueMedian)
    tissueMedianFrame <- data.frame(orderedLevels, 1:count)
    dfColors <- merge(tissueMedianFrame, tissueColors, by.x="orderedLevels", by.y="labels")
    dfOrderedColors <- dfColors[with(dfColors, order(X1.count)),]
    colorsHex <- as.vector(dfOrderedColors$colorsHex)
    labels <- as.vector(dfOrderedColors$orderedLevels)
    tissueFactor <- tissueMedian
} else {
    # the gtexColorVX.R file has the tissue colors in a particular order, so match
    # that up with the labels and data frame if we aren't sorting on median
    labels <- labels[order(match(labels, tissueOrder))]
    tissueFactor <- ordered(df$tissue, levels=labels)
}

# draw graph
png(file=outFile, width=1070, height=600)
# res=72 is default
gray <- "#A6A6A6"
darkgray <- "#737373"

# plot with customized margins, symbol and line styles and colors, to mimic GTEx figure in
# UCSC GTEx grant proposal
par(mar=c(12,4,3,1) + 0.1, mgp=c(2,1,0), font.main=1)
#yLabel <- if (isLog) "Log10 (RPKM+1)" else "RPKM"
yLabel <- if (isLog) paste0("Log10 (",units,"+1)") else units 
max <- max(df$rpkm)
yLimit <- if (isLog) c(-.05, max+.1) else c(-(max*.02), max+ (max*.03))
exprPlot <- boxplot(rpkm ~ tissueFactor, data=df, ylab=yLabel, ylim=yLimit,
                        main=paste0(gene, " Gene Expression from GTEx (Release ", version, ")"),
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
                        # erase Y axis labels (add later rotated)
                        names=rep(c(""), count))

# display sample count
y1 <- par("usr")[3]
size <- .8
adjust <- if (isLog) max/30 else .4*abs(y1)
text(-.5, y1 + adjust, "N=", cex=size)
text(1:count, y1 + adjust, exprPlot$n, cex=size, col="black")
# draw text twice (once in color, once in black) to make light colors readable
text(1:count, y1 + adjust, exprPlot$n, cex=size, col=darkerColorsHex)

# add X axis labels at 45 degree angle
rot <- 45
size <- 1
adjust <- if (isLog) max/30 else .7*abs(y1)
text(1:count, y1 - adjust, cex=size, labels=labels, srt=rot, xpd=TRUE, adj=c(1,.5), col="black")
text(1:count, y1 - adjust, cex=size, labels=labels, srt=rot, xpd=TRUE, adj=c(1,.5), 
        col=darkerColorsHex)

graphics.off()
