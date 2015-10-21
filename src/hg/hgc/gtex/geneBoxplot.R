# Boxplot of gene expression by tissue
#       usage:   Rscript geneBoxplot.R gene dataFile outFile
# where dataFile is a tab-sep file, with a header and row for each sample in the format:
#       sample  tissue  rpkm
# and outFile is filename suitable for writing graphic (e.g. in PNG format)

args <- commandArgs(TRUE)
gene <- args[1]
dataFile <- args[2]
outFile <- args[3]

#gene <- "BRCA1"
#df <- read.table("BRCA1.df.txt", sep="\t", header=TRUE)
df <- read.table(dataFile, sep="\t", header=TRUE)

# alternative is to directly pull from mySQL.
# library(RMySQL); dbConnect(MySQL(), user=, password=, etc.))
# Would want to use hg.conf for login.

source("gtex/tissueColors.R")
# sets colorsHex

labels <- names(table(df$tissue))
count <- length(labels)

# draw graph
png(file=outFile, width=900, height=500)
gray <- "#A6A6A6"
darkgray <- "#737373"

# plot with customized symbol and line styles and colors
exprPlot <- boxplot(rpkm ~ tissue, data=df, ylab="RPKM", 
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

# add X axis labels at 45 degree angle, drawing twice (once in color, once in black) to
# make light colors readable
rot <- 45
y1 <- par("usr")[3]
text(1:count, y1 -.3, cex=.75, labels=labels, srt=rot, xpd=TRUE, adj=c(1,.5), col="black")
text(1:count, y1 - .3, cex=.75, labels=labels, srt=rot, xpd=TRUE, adj=c(1,.5), col=colorsHex)

# display sample count
adjust <- .2
text(-.5, y1 + adjust, "N=", cex=.6)
text(1:count, y1 + adjust, exprPlot$n, cex=.6, col="black")
text(1:count, y1 + adjust, exprPlot$n, cex=.6, col=colorsHex)

graphics.off()
