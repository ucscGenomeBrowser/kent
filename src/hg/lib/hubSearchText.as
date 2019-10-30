table hubSearchText
"Track hub descriptions"
    (
    lstring hubUrl; "Hub URL"
    string db; "Assembly name (UCSC format) for Assembly and Track descriptions, NULL for hub descriptions"
    string track; "Track name for track descriptions, NULL for others"
    string label; "Name to display in search results"
    enum ("Short", "Long", "Meta") textLength; "Length of text (short for labels, long for description pages, meta for metadata)"
    lstring text; "Description text"
    lstring parents; "Comma separated list of parent tracks (and their labels) of this track, NULL for others"
    lstring parentTypes; "Comma separated list of parent track types"
    )
