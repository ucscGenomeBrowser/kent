
table cdwStepDef
"A step in an analysis pipeline - produces new files from old. See also cdwStepRun"
    (
    uint id primary auto; "Pipeline id"
    string name unique;  "Name of this analysis step"
    string description;  "Description of step, about a sentence."
    )
 

table cdwStepRun
"A particular run of a step."
    (
    uint id primary auto; "Analysis run ID"
    uint stepDef;	"Associated step definition"
    string stepVersion;	"Version number of step"
    )

table cdwStepIn
"Inputs to an cdwStepRun"
    (
    uint id primary auto; "Input table ID"
    uint stepRunId index; "Which cdwStepRun this is associated with"
    string name;  "Input name within step"
    uint ix;  "Inputs always potentially vectors.  Have single one with zero ix for scalar input"
    uint fileId;  "Associated file."
    )

table cdwStepOut
"Outputs to an cdwAnalysis"
    (
    uint id primary auto; "Output table ID"
    uint stepRunId index; "Which cdwStepRun this is associated with"
    string name;  "Output name within step"
    uint ix;  "Outputs always potentially vectors. Have single one with zero ix for scalar output"
    uint fileId;  "Associated file."
    )

