This directory contains the text and image files used for 
the Proteome Browser user's guide and help files. The user's 
guide and track/stamp help files use server side includes (SSI's). 
This allows the same text to be included in multiple files while
reducing maintenance overhead.

Here is the basic layout:

- .gif files: Image files

- pbTracksHelp.shtml: The "main" user's guide file. It 
pulls in most of the smaller pb*.txt files using SSI's.

- other .shtml files: The help files linked to from PB 
stamps & tracks. They pull in the appropriate  description 
& discussion .txt files that correspond to the stamp or track.

- .txt files: The actual description & discussion text files. 
Discussion files have *Disc* in their names. Some tracks share 
a *Disc* file with a related histogram.

The .txt files share the same root name as the .shtml files 
that include them, except in cases where both a track & a stamp 
share the same discussion file. In that case, the stamp name is used.

So, for example:
- pbhydro.shtml - linked to by Hydrophobicity stamp
- pbhydro.txt - text for stamp description section
- pbhydroTr.shtml - linked to by Hydrophobicity track
- pbhydroTr.txt - text for track description section
- pbhydroDisc.txt - text for stamp & track dicussion sections


