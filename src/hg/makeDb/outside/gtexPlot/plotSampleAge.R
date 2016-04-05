# Stacked bar graph showing contribution by age to tissue samples

df <- read.table("sampleDf.txt", sep="\t", header=TRUE)

ylimit <- 500

source("../../../hgc/hgcData/gtexColorsV4.R")
# sets colorsHex, darkerColorsHex

year20 <- subset(df, age == 20)
year30 <- subset(df, age == 30)
year40 <- subset(df, age == 40)
year50 <- subset(df, age == 50)
year60 <- subset(df, age == 60)
year70 <- subset(df, age == 70)

tissueAge <- cbind(
		table(year70$tissue),
		table(year60$tissue),
		table(year50$tissue),
		table(year40$tissue),
		table(year30$tissue),
		table(year20$tissue)
		)
ageColors = c("red", "green", "purple", "yellow", "blue", "orange")
ageLabels = c("70-80 yrs", "60-70 yrs", "50-60 yrs", "40-50 yrs", "30-40 yrs", "20-30 yrs")

png(file="sampleAge.png", width=1070, height=450)
par(mar=c(12,4,3,1) + 0.1, mgp=c(2,1,0))
bp=barplot(t(tissueAge), main="GTEx Sample Donor Age by Tissue (V6, 8555 Samples)",
	ylab="Sample count", ylim=c(0,ylimit),
	xaxt="n", col=ageColors)
labels <- names(table(df$tissue))
bars = length(labels)
# need to query the barplot for X placements
xBars = bp[c(1:bars)]
axis(1, xBars, labels=rep("",bars))

# Add X axis labels (tissue) at 45 degree angle
# NOTE: should compute adjustment of y axis from something
#yLabel <- par("usr")[3]-.3
yLabel <- -22
cex <- .9
text(xBars, yLabel, cex=cex,labels=labels,srt=45,xpd=TRUE,adj=c(1,.5), col="black")
text(xBars, yLabel, cex=cex,labels=labels,srt=45,xpd=TRUE,adj=c(1,.5), col=darkerColorsHex)

# Mark threshold at 60 -- sample count needed for eQTL
abline(h=60,col="red",lty=6, xpd=FALSE)
text(65.5, 70, "eQTL cutoff",xpd=TRUE, col="red", cex=.75)

# Add legend for ages
legend('top', rev(ageLabels), cex=.75, col=rev(ageColors), lwd=5, bty='n')

graphics.off()
