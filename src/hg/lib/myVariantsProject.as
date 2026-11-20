table myVariantsProjects
"One My Annotations project. Each project is one SQL table and one track."
    (
    uint id;          "Auto-increment primary key"
    string userName;  "Login of the user who owns the project"
    string dbTable;   "Fully qualified SQL table holding the annotations"
    string label;     "Project name as the user typed it; the track's short label"
    string createdAt; "Timestamp of creation"
    )
