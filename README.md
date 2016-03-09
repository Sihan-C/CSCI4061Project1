The purpose of the porgram: 
Making a simplified version of Makefile, with options -f -n -B -m,
which their syntax are described below.

How to compile the program:
Type in make in the shell

How to use the program from the shell
./make4061		( Execute the first target )
./make4061 targetname	( Execute the targetname from the makefile )
./make4061 -f filename	( Execute the specified makefile, filename )
./make4061 -n 	( Display all the commands without execute them )
./make4061 -B	( Commpile the program without check the timestamp )
./make4061 -m log.txt	( Redirect output to log.txt file )

Additional Instructions 
Options can be used together. For example, 
./make4061 targetname -m log.txt -n 
will output the command of targetname to the log.txt. 


