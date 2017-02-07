
strainList = GCA_001624185.1_129S1_SvImJ_v1 GCA_001624215.1_A_J_v1 \
	GCA_001624295.1_AKR_J_v1 GCA_001624445.1_CAST_EiJ_v1 \
	GCA_001624475.1_CBA_J_v1 GCA_001624505.1_DBA_2J_v1 \
	GCA_001624535.1_FVB_NJ_v1 GCA_001624675.1_NOD_ShiLtJ_v1 \
	GCA_001624745.1_NZO_HlLtJ_v1 GCA_001624775.1_PWK_PhJ_v1 \
	GCA_001624835.1_WSB_EiJ_v1 GCA_001624865.1_SPRET_EiJ_v1 \
	GCA_001632525.1_BALB_cJ_v1 GCA_001632555.1_C57BL_6NJ_v1 \
	GCA_001632575.1_C3H_HeJ_v1 GCA_001632615.1_LP_J_v1

install::
	@for D in ${strainList}; do \
	    echo $$D ; \
	    rm -f /hive/data/genomes/asmHubs/hubLinks/$$D/rnaSeqData/$$D.trackDb.txt ; \
	    cp -p $$D/rnaSeqData/$$D.trackDb.txt /hive/data/genomes/asmHubs/hubLinks/$$D/rnaSeqData/$$D.trackDb.txt ; \
	    rm -f /hive/data/genomes/asmHubs/hubLinks/$$D/$$D.trackDb.txt ; \
	    rm -f /hive/data/genomes/asmHubs/hubLinks/$$D/$$D.bigMaf.trackDb.txt ; \
	    cp -p $$D/$$D.trackDb.txt /hive/data/genomes/asmHubs/hubLinks/$$D/$$D.trackDb.txt ;\
	    cp -p $$D/$$D.bigMaf.trackDb.txt /hive/data/genomes/asmHubs/hubLinks/$$D/$$D.bigMaf.trackDb.txt ;\
	done
	rm -f /hive/data/genomes/asmHubs/hubLinks/rnaSeqStar.html
	cp -p rnaSeqStar.html /hive/data/genomes/asmHubs/hubLinks/rnaSeqStar.html
	rm -f /hive/data/genomes/asmHubs/hubLinks/mouseStrains.html
	cp -p mouseStrains.html /hive/data/genomes/asmHubs/hubLinks/mouseStrains.html
