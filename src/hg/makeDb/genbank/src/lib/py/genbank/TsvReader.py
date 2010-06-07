"Simple TSV file reader"

class TsvRow(object):
    "row object that has a named field per column"
    

class TsvReader(object):
    "Simple TSV file reader.  Column header may start with #, per UCSC conventions"

    def __init__(self, fname):
        """open file and parse header"""
        self.fh = file(fname)
        slef.lineNum = 0

        # parse header
        line = self.fh.readline()
        self.lineNum += 1
        line = line[0:-1]  # do first to catch blank line
        if (line == ""):
            raise Exception("empty TSV file or header: " + fname)
        if line[0] == '#':
            line = line[1:]
        self.columns = map(intern, line.split("\t"))
        # build name to column index map
        self.colMap = {}
        i = 0
        for col in self.columns:
            self.colMap[col] = i
            i += 1

    def close(self):
        """close file, called automatically by iter on EOF."""
        if self.fh != None:
            self.fh.close()
        self.fh = None

    def __iter__(self):
        "get iter over rows of file, returned as TsvRow objects"
        return self

    def next(self):
        line = self.fh.readline()
        if (line == ""):
            self.close();
            raise StopIteration
        self.lineNum += 1
        line = line[0:-1]  # drop newline
        row = line.split("\t")
        if len(row) != len(self.columns):
            self.close()
            raise Exception("row has %d columns, expected %d" %
                           (len(row), len(self.columns)))
        return row

