test::
	 @printf "\ntest1: basic test\n"
	 clusterMatrixToBarChartBed -median test.meta test.matrix test.coords test.output
	 diff median.expected test.output
	 clusterMatrixToBarChartBed -simple test.meta test.matrix test.coords test.output
	 diff mean.expected test.output
	 clusterMatrixToBarChartBed -simple -name2=test.name2 test.meta test.matrix test.coords test.output
	 diff name2.expected test.output

clean::
	rm -f test.output
