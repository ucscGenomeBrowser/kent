table clonePos
"A clone's position and other info."
    (
    string name;	"Name of clone including version"
    uint seqSize;       "base count not including gaps"
    ubyte phase;        "htg phase"
    string chrom;       "Reference sequence chromosome or scaffold"
    uint chromStart;	"Start in chromosome"
    uint chromEnd;      "End in chromosome"
    char[1] stage;      "F/D/P for finished/draft/predraft"
    string faFile;      "File with sequence."
    )
