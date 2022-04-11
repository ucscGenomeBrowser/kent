
# default set of hub track html description files

htmlFiles = html/GCA_009914755.4_T2T-CHM13v2.0.assembly.html \
html/GCA_009914755.4_T2T-CHM13v2.0.chainNet.html \
html/GCA_009914755.4_T2T-CHM13v2.0.cpgIslands.html \
html/GCA_009914755.4_T2T-CHM13v2.0.description.html \
html/GCA_009914755.4_T2T-CHM13v2.0.gc5Base.html \
html/GCA_009914755.4_T2T-CHM13v2.0.repeatMasker.html \
html/GCA_009914755.4_T2T-CHM13v2.0.simpleRepeat.html \
html/GCA_009914755.4_T2T-CHM13v2.0.windowMasker.html \
html/GCA_009914755.4_T2T-CHM13v2.0.xenoRefGene.html

# these html files come from the T2T porting project, not maintained here
outsideSource = GCA_009914755.4_T2T-CHM13v2.0.catLiftOffGenes.html \
GCA_009914755.4_T2T-CHM13v2.0.catLiftOffGenesV1.html \
GCA_009914755.4_T2T-CHM13v2.0.censat.html \
GCA_009914755.4_T2T-CHM13v2.0.clinVar20220313.html \
GCA_009914755.4_T2T-CHM13v2.0.dbSNP155.html \
GCA_009914755.4_T2T-CHM13v2.0.gwasSNPs2022-03-08.html \
GCA_009914755.4_T2T-CHM13v2.0.hgCactus.html \
GCA_009914755.4_T2T-CHM13v2.0.hgLiftOver.html \
GCA_009914755.4_T2T-CHM13v2.0.hgUnique.html \
GCA_009914755.4_T2T-CHM13v2.0.proseq.html \
GCA_009914755.4_T2T-CHM13v2.0.rdnaModel.html \
GCA_009914755.4_T2T-CHM13v2.0.rnaseq.html \
GCA_009914755.4_T2T-CHM13v2.0.sedefSegDups.html

# tracks not used, and userTrackDb comes from T2T porting project

notPushed = html/GCA_009914755.4_T2T-CHM13v2.0.augustus.html \
html/GCA_009914755.4_T2T-CHM13v2.0.tanDups.html \
trackDb/GCA_009914755.4_T2T-CHM13v2.0.userTrackDb.txt

# files used to construct a useOneFile hub.txt
hubTxt = hub/GCA_009914755.4_T2T-CHM13v2.0.hub.txt \
hub/GCA_009914755.4_T2T-CHM13v2.0.genomes.txt \
trackDb/GCA_009914755.4_T2T-CHM13v2.0.trackDb.txt

singleFileHub: ${hubTxt}
	@rm -f hub.txt
	@grep -v genomesFile hub/GCA_009914755.4_T2T-CHM13v2.0.hub.txt > hub.txt
	@printf "useOneFile on\n\n" >> hub.txt
	@grep -v trackDb hub/GCA_009914755.4_T2T-CHM13v2.0.genomes.txt >> hub.txt
	@echo >> hub.txt
	@cat trackDb/GCA_009914755.4_T2T-CHM13v2.0.trackDb.txt >> hub.txt

alpha: singleFileHub

clean::
	rm -f hub.txt
