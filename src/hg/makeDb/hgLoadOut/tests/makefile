
test:
	@hgLoadOut -tabFile=chrCp.rmsk.tab -table=rmsk test chrCp.fa.out.gz
	@diff chrCp.expected.tab chrCp.rmsk.tab
	@rm -f chrCp.rmsk.tab
	@zcat chrCp.fa.out.gz | hgLoadOut -tabFile=chrCp.rmsk.tab -table=rmsk test stdin
	@diff chrCp.expected.tab chrCp.rmsk.tab

clean::
	rm -f chrCp.rmsk.tab
