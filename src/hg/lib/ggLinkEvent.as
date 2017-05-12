table ggLinkEvent
" hgcGeneGraph relationship between genes and events. The event itself is stored in ggEventDb if it is derived from a a database or ggEventText if derived from text mining "
    (
    string gene1 index;       "Gene pair, symbol1 (first in alphabet)"
    string gene2;       "Gene pair, symbol2 (second in alphabet)"
    string eventId;     "eventId to lookup more details for a gene pair "
    )
