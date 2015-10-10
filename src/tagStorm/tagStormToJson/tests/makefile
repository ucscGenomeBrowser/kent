test:
	tagStormToJson test.tags temp.json
	diff temp.json expected.out
	rm temp.json

clean:
	rm -f temp.json
