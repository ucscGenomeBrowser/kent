# Create sample characterization plots for GTEx track description page

# Create tab-separated file of sample metadata
csh makeSampleDf.csh > sampleDf.txt

# Plot RNA Integrity by tissue
/usr/bin/Rscript --vanilla plotSampleRin.R
cp sampleRin.png ~/public_html/gtex/gtexSampleRinV6.png
# visually inspect, then copy to source dir
cp sampleRin.png ~/kent/src/hg/htdocs/images/gtex/gtexSampleRin.V6.png

# Plot distribution of sample age by tissue
/usr/bin/Rscript --vanilla plotSampleAge.R
cp sampleAge.png ~/public_html/gtex/gtexSampleAgeV6.png
# visually inspect, then copy to source dir
cp sampleAge.png ~/kent/src/hg/htdocs/images/gtex/gtexSampleAge.V6.png
