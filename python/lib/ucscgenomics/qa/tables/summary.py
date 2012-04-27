class SumRow(object):
    """Keeps track of tableQa information: db, name, type, statistics, and error status."""
    def __init__(self, db, table, tableType):
        self.__row = [db, table, tableType, '', '', 'no']

    def __repr__(self):
        return str(self.__row)

    def setCount(self, count):
        self.__row[3] = count

    def setCoverage(self, coverage):
        self.__row[4] = coverage

    def setError(self):
        self.__row[5] = 'YES'

class SumTable(object):
    """A list of SumRows, plus a header line."""
    def __init__(self):
        self.header = ["Database", "Table", "Type", "Row Count", "Coverage", "Errors"]
        self.table = [self.header]

    def addRow(self, sumRowToAdd):
        self.table.append(sumRowToAdd._SumRow__row)

    def __repr__(self):
        return str(self.table)

    def tabSeparated(self):
        return '\n'.join('\t'.join(item for item in row) for row in self.table) + '\n'

