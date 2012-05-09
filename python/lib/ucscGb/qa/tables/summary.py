class SumRow(object):
    """A list that keeps track of tableQa info: db, name, type, statistics, and error status."""
    def __init__(self, db, table, tableType):
        if tableType == None:
            tableType = 'none'
        self.__row = [db, table, tableType, '', 'n/a', 'n/a', 'no']

    def __repr__(self):
        return str(self.__row)

    def setCount(self, count):
        self.__row[3] = count

    def setFeatureBits(self, fbOut):
        self.__row[4] = fbOut

    def setFeatureBitsGaps(self, fbGapOut):
        self.__row[5] = fbGapOut

    def setError(self):
        self.__row[6] = 'YES'

class SumTable(object):
    """A list of SumRows, plus a header line."""
    def __init__(self):
        self.header = ["Database", "Table", "Type", "Row count", "featureBits", "Overlap with gap",
                       "Errors"]
        self.table = [self.header]

    def __repr__(self):
        return str(self.table)

    def addRow(self, sumRowToAdd):
        self.table.append(sumRowToAdd._SumRow__row)

    def tabSeparated(self):
        """Returns a tab-separated version of the SumTable."""
        return '\n'.join('\t'.join(item for item in row) for row in self.table) + '\n'

