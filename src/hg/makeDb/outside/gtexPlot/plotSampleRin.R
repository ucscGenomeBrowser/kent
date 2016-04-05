# Boxplot of GTEx RNA integrity by Tissue

df <- read.table("sampleDf.txt", sep="\t", header=TRUE)

# today's location
source("../../../hgc/hgcData/gtexColorsV4.R")
# sets colorsHex, darkerColorsHex

# generate plot to compute coordinates, etc., but don't display yet, as we need special
#    handling of X axis (tissue) labels (too wide for generic display)
rinPlot = boxplot(rin ~ tissue, data=df, plot=0)

# Now draw the plot
png(file="sampleRin.png", width=1070, height=500)
par(mar=c(12,4,3,1) + 0.1, mgp=c(2,1,0))
boxplot(rin ~ tissue, data=df, col=colorsHex, main="GTEx RNA Integrity by Tissue", ylab="RIN", ylim=c(4.5,11), outline=FALSE, names=rep(c(""),length(rinPlot$n)))

# Add X axis labels (tissue) at 45 degree angle
# NOTE: should compute adjustment of y axis from something
labels <- names(table(df$tissue))
cex <- .9
text(1:length(rinPlot$n),par("usr")[3]-.3, cex=cex,labels=labels,srt=45,xpd=TRUE,adj=c(1,.5),
	col="black")
text(1:length(rinPlot$n),par("usr")[3]-.3, cex=cex,labels=labels,srt=45,xpd=TRUE,adj=c(1,.5),
	col=darkerColorsHex)

# Display sample count
adjust <- .2
cex <- .7
text(1:length(rinPlot$n), par("usr")[3]+adjust, rinPlot$n, cex=cex, col="black")
text(1:length(rinPlot$n), par("usr")[3]+adjust, rinPlot$n, cex=cex, col=darkerColorsHex)
text(-.5, par("usr")[3]+adjust, "N=", cex=.6)

# Add threshold line.  Should compute label X from bounds
text(par("usr")[2]-1.7, 5.85, "GTEX cutoff",xpd=TRUE, col="red",cex=.75)
# From literature: RIN=8 is commonly cited cut-off.  9.7 for extreme qual
abline(h=6,col="red",lty=6)

graphics.off()

# UNCOMMENT TO SEE POINTS
#points(df$tissue,df$rin, cex=.5, col="gray33")

