table expRecord
"minimal descriptive data for an experiment in the browser"
(
	uint id; "internal id of experiment"
	string name; "name of experiment"
	lstring description; "description of experiment"
	lstring url; "url relevant to experiment"
	lstring ref; "reference for experiment"
	lstring credit; "who to credit with experiment"
	uint numExtras; "number of extra things"
	lstring[numExtras] extras; "extra things of interest, i.e. classifications"
)
