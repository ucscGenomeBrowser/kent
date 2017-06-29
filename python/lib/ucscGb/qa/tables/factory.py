import pipes

from ucscGb.qa.tables.genePredQa import GenePredQa
from ucscGb.qa.tables.tableQa import TableQa
from ucscGb.qa.tables.pslQa import PslQa
from ucscGb.qa.tables.positionalQa import PositionalQa
from ucscGb.qa.tables.pointerQa import PointerQa
from ucscGb.qa import qaUtils
from ucscGb.qa.tables import trackUtils

def tableQaFactory(db, table, reporter, sumTable):
    """Returns tableQa object according to trackDb track type.""" 
    tableType = trackUtils.getTrackType(db, table)
    if tableType == None:
        return TableQa(db, table, tableType, reporter, sumTable)
    elif trackUtils.isPsl(tableType):
        return PslQa(db, table, tableType, reporter, sumTable)
    elif trackUtils.isGenePred(tableType):
        return GenePredQa(db, table, tableType, reporter, sumTable)
    elif trackUtils.isPositional(tableType):
        return PositionalQa(db, table, tableType, reporter, sumTable)
    elif trackUtils.isPointer(tableType):
        return PointerQa(db, table, tableType, reporter, sumTable)
    else:
        raise Exception(db + table + " has unknown track type " + tableType)

