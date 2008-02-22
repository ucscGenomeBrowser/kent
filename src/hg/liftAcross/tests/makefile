
all::
	liftAcross -warn testAcross.lft input.gp testOutput.gp 2> testErr.out
	diff testOutput.gp expectedOutput.gp
	diff testErr.out expectedErr.out
