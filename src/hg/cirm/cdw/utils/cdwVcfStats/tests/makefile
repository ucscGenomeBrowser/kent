test:
	cdwVcfStats indelTrim.vcf temp.out
	diff temp.out expected.out
	rm temp.out

clean::
	@[ -f ./temp.out ] && rm temp.out || true
