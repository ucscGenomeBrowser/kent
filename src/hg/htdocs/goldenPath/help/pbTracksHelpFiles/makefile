global:
	rm -rf pbGlobal
	mkdir pbGlobal

	cp -p *.jpg pbGlobal
	cp -p *.gif pbGlobal

	@for F in *.txt; do \
	echo generating pbGlobal/$$F;\
	cat $$F|sed -e 's/genome-wide/proteome-wide/g'|\
	sed -e 's/Genome-wide/Proteome-wide/g' > ./pbGlobal/$$F;\
	done
	
	@for F in *.*html; do \
	echo generating pbGlobal/$$F;\
	cat $$F|sed -e 's/genome-wide/proteome-wide/g'|\
	sed -e 's/Genome-wide/Proteome-wide/g' > ./pbGlobal/$$F;\
	done

clean::
	rm -rf pbGlobal
