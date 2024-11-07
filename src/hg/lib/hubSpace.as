table hubSpace
"file storage table for users to store uploaded tracks"
    (
    string userName; "userName of user uploading file"
    string fileName; "name of uploaded files. The actual path to this file is different"
    bigint fileSize; "size of the uploaded file"
    string fileType; "track type of file"
    string creationTime; "first upload time"
    string lastModified; "last change time"
    string db; "genome assembly associated with this file"
    string location; "file system path or URL to file"
    string md5sum; "md5sum of file"
    string parentDir; "parent directory of file"
    )
