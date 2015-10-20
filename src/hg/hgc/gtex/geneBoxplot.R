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

source ("gtex/tissueColors.R")
# sets colorsHex

# set X11 display for graphing package (perhaps should be done in environment var)
#Sys.setenv("DISPLAY"=":0.0")
# required for Linux (replaced now with --gui=X11 on command line)
#options(bitmapType='cairo')
png(file=outFile, width=800, height=500)
boxplot(rpkm ~ tissue, data=df, col=colorsHex, main=paste(gene, "Gene Expression in 53 Tissues from GTEx (V4 release, 2014)"))
graphics.off()

# add X axis labels (code in sampleRinPlot.R)
