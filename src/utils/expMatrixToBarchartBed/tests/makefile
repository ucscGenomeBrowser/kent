test::
	 expMatrixToBarchartBed test.meta test.matrix test.coords foo
	 diff foo test.output
	 expMatrixToBarchartBed test.meta test.matrix test.coords foo --groupOrder test.groupOrder --useMean
	 diff foo test.output2
	 rm foo
	 expMatrixToBarchartBed test.meta test.matrix extraFields.coords foo --autoSql test.as
	 diff foo extraFields.output
	 rm foo
	 expMatrixToBarchartBed test.meta test.matrix extraFields.coords foo --groupOrder test.groupOrder --useMean --autoSql test.as
	 diff foo allOptions.output
	 rm foo
