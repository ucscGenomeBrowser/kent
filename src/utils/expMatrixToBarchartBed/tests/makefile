test::
	 @printf "\ntest1: basic test\n"
	 expMatrixToBarchartBed test.meta test.matrix test.coords foo
	 diff foo test.output
	 @printf "\ntest2: test of --groupOrder\n"
	 expMatrixToBarchartBed test.meta test.matrix test.coords foo --groupOrder test.groupOrder --useMean
	 diff foo test.output2
	 rm foo
	 @printf "\ntest3: test of extraFields in bed file and corresponding autoSql\n"
	 expMatrixToBarchartBed test.meta test.matrix extraFields.coords foo --autoSql test.as
	 diff foo extraFields.output
	 rm foo
	 @printf "\ntest4: test of all options\n"
	 expMatrixToBarchartBed test.meta test.matrix extraFields.coords foo --verbose --groupOrder test.groupOrder --useMean --autoSql test.as
	 diff foo allOptions.output
	 rm foo
	 @printf "\ntest5: autoSql supplied but no actual extraFields\n"
	 expMatrixToBarchartBed test.meta test.matrix noExtraFields.coords foo --verbose --groupOrder test.groupOrder --useMean --autoSql ../../../hg/lib/barChartBed.as
	 diff foo allOptions2.output
	 rm foo
	 @printf "\ntest6: mixed scientific notation and regular floats in matrix\n"
	 expMatrixToBarchartBed floats.meta floats.matrix floats.coords foo
	 diff foo floats.output
	 rm foo
