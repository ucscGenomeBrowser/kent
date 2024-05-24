table hubSpace
"file storage table for users to store uploaded tracks"
    (
    string userName; "userName of user uploading file"
    lstring fileName; "name of uploaded files. The actual path to this file is different"
    bigint fileSize; "size of the uploaded file"
    string fileType; "track type of file"
    string creationTime; "first upload time"
    string lastModified; "last change time"
    lstring hubNameList; "comma separated list of hubs this file is a part of"
    string db; "genome assembly associated with this file"
    lstring location; "file system path or URL to file"
    )
