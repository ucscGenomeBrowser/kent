table scopDes
"Structural Classification of Proteins Description. See Lo Conte, Brenner et al. NAR 2002"
    (
    int sunid;	"Unique integer"
    char[2] type;	"Type.  sf=superfamily, fa = family, etc."
    string sccs;	"Dense hierarchy info."
    string sid;		"Older ID."
    lstring description; "Descriptive text."
    )
