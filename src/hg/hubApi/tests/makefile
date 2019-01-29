
test::
	@./jsonConsumer.pl | gzip -c > testOutput.json.gz
	@zdiff testOutput.json.gz expected.json.gz

clean::
	rm -f testOutput.json.gz
