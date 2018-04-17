
all: devChainNet.list rrChainNet.list rrActive.list chainNetPlacental.ra chainNetRBestPrimate.ra \
	chainNetSynPrimate.ra chainNetPrimate.ra chainNetVertebrate.ra

devChainNet.list:
	@for db in hg38 hg19; do \
	    hgsql -e 'show tables;' "$$db" \
	| grep -i chain | grep Link | egrep -i -v "self|patch" \
	| sed -e 's/chain//; s/Link//;' | sed -e 's/^./\L&\E/';\
	done | sort -u > $@

rrChainNet.list:
	@for db in hg38 hg19; do \
	    HGDB_CONF=$$HOME/.hg.mysqlrr.conf hgsql -e 'show tables;' "$$db" \
	| grep -i chain | grep Link | egrep -i -v "self|patch" \
	| sed -e 's/chain//; s/Link//;' | sed -e 's/^./\L&\E/';\
	done | sort -u > $@

rrActive.list:
	hgsql -N -hgenome-centdb -e 'select name from dbDb where active=1' \
		hgcentral | sort > $@

chainNetPlacental.ra: euarchontoglires.list carnivora.list \
	cetartiodactyla.list chiroptera.list laurasiatheria.list \
	perissodactyla.list xenarthra.list afrotheria.list
	../chainNetCompositeTrackDb.pl net placental $^ \
		| sed -e 's/  Chain/ Chain/g; s/  Net/ Net/g;' > $@

chainNetPrimate.ra: hominidae.list cercopithecinae.list haplorrhini.list \
	strepsirrhini.list
	../chainNetCompositeTrackDb.pl net primate $^ \
		| sed -e 's/  Chain/ Chain/g; s/  Net/ Net/g;' > $@

chainNetRBestPrimate.ra: hominidae.list cercopithecinae.list haplorrhini.list \
	strepsirrhini.list
	../chainNetCompositeTrackDb.pl rbest primate $^ \
		| sed -e 's/  Chain/ Chain/g; s/  Net/ Net/g;' > $@

chainNetSynPrimate.ra: hominidae.list cercopithecinae.list haplorrhini.list \
	strepsirrhini.list
	../chainNetCompositeTrackDb.pl syn primate $^ \
		| sed -e 's/  Chain/ Chain/g; s/  Net/ Net/g;' > $@

chainNetVertebrate.ra: mammalia.list dinosauria.list lepidosauria.list \
	amphibia.list cryptodira.list coelancanthimorpha.list neopterygii.list hyperoartia.list
	../chainNetCompositeTrackDb.pl net vertebrate $^ \
		| sed -e 's/  Chain/ Chain/g; s/  Net/ Net/g;' > $@

clean:
	rm -f devChainNet.list rrChainNet.list rrActive.list \
	chainNetPlacental.ra chainNetRBestPrimate.ra \
	chainNetSynPrimate.ra chainNetPrimate.ra chainNetVertebrate.ra
