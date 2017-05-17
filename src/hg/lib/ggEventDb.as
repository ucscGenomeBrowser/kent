table ggLinkEventDb
" hgcGeneGraph details of database curated interactions; a cause gene influences a theme gene "
    (
    string eventId;       "ID of interaction description"
    string causeType;       "type of cause of interaction, values: complex, gene or family"
    string causeName;       "name of cause, as used in database"
    lstring causeGenes;       "gene symbols of cause, comma-separated list"
    string themeType;       "type of theme of interaction, values: complex, gene or family. If empty: the interaction is a purified complex, with the members in the causeGenes field."
    string themeName;       "name of theme, as used in database"
    lstring themeGenes;       "gene symbols of theme, |-separated list"
    string relType;       "type of relation between cause and theme"
    string relSubtype;       "subtype of relation, if provided"
    string sourceDb;       "source database"
    lstring sourceId;       "identifier in source database for HTML links"
    string sourceDesc;       "description of interaction in source database (e.g. pathway name)"
    lstring docIds;       "list of supporting documents, numbers are PMIDs"
    )
