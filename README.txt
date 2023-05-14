Added input validation to ensure resouces were added to the system, 
as well as that no task has a required resource that does not exist
before starting simulation

seq.c is an adapted version of the simualtion to exectute sequentially with 
only one thread of control, very slow!

(note seq.c based off of an older version with less error checking)

test.c was used to test time structs and timing things

I've also included my test input files, commands to run them 
are all included in the make file

Josh Lucas
