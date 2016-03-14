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
source("hgcData/gtexColorsV4.R")
# sets colorsHex

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
    tissueFactor <- df$tissue
}

# draw graph
png(file=outFile, width=1000, height=500)
# res=72 is default
gray <- "#A6A6A6"
darkgray <- "#737373"

# plot with customized margins, symbol and line styles and colors, to mimic GTEx figure in
# UCSC GTEx grant proposal
par(mar=c(7,4,3,2) + 0.1, mgp=c(2,1,0), font.main=1)
yLabel <- if (isLog) "Log10 (RPKM+1)" else "RPKM"
max <- max(df$rpkm)
yLimit <- if (isLog) c(-.05, max+.1) else c(-(max*.02), max+ (max*.03))
exprPlot <- boxplot(rpkm ~ tissueFactor, data=df, ylab=yLabel, ylim=yLimit,
                        main=paste(gene, "Gene Expression from GTEx (Release ", version, ")"),
                        col=colorsHex, border=c(darkgray),
                        # medians
                        medcol="black", medlwd=2,
                        # outliers
                        outcex=.5, outcol=darkgray,
                        # whiskers
                        whisklty=1, whiskcol=gray, 
                        # 'staples'
                        staplecol=gray, staplecex=1.3, staplewex=.8,
                        # erase Y axis labels (add later rotated)
                        names=rep(c(""), count))

# display sample count
y1 <- par("usr")[3]
size <- .8
adjust <- if (isLog) max/30 else .4*abs(y1)
text(-.5, y1 + adjust, "N=", cex=size)
# draw text twice (once in color, once in black) to make light colors readable
text(1:count, y1 + adjust, exprPlot$n, cex=size, col="black")
text(1:count, y1 + adjust, exprPlot$n, cex=size, col=colorsHex)

# add X axis labels at 45 degree angle
rot <- 45
size <- 1
adjust <- if (isLog) max/30 else .7*abs(y1)
text(1:count, y1 - adjust, cex=size, labels=labels, srt=rot, xpd=TRUE, adj=c(1,.5), col="black")
text(1:count, y1 - adjust, cex=size, labels=labels, srt=rot, xpd=TRUE, adj=c(1,.5), col=colorsHex)

graphics.off()
