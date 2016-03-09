#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h> 
#include <unistd.h>
#include "util.h"

struct target *targetArray[ 10 ];
int tarArrIndex = 0;

//This function will parse makefile input from user or default makeFile. 
int parse(char * lpszFileName)
{
	int nLine=0;
	char szLine[1024];
	char * lpszLine;
	FILE * fp = file_open( lpszFileName );	   	
 	
	if( fp == NULL ){
		return -1;
	}
	targetArray[ tarArrIndex ] = malloc( sizeof( struct target ) );
	while ( file_getline( szLine, fp ) != NULL ) {	
		// Allocate memory for a target struct 
		nLine++;

		//Remove newline character at end if there is one
		lpszLine = strtok( szLine, "\n"); 
		
		// Skip blank lines		
		if ( lpszLine == NULL )
			continue;

		//Skip if blank or comment && Skip if whitespace-only.
		int i = 0;
		int check = 1;

		while ( lpszLine[ i ] != '\0' ) {
			if ( lpszLine[ i ] == '#' || lpszLine[ i ] == ' ' ) {
				i++;
				break;		
			}
			else {
				check = 0;	// Not a comment line or a blank line 
				break;
			}			
		}
		if ( check )
			continue;
		
		// Target Line 
		int index = 0;		
		int start_index, end_index;
		if ( ( lpszLine[ index ] != ' ' ) && ( lpszLine[ index ] != '\t' ) ) {
			
			// Parse target	
			index = 1;
			while ( lpszLine[ index ] != ':'  ) {
				index++; 		
			}

			// Store target
			strncpy( targetArray[ tarArrIndex ] -> szTarget, lpszLine, index );

			// Parse dependencies
			targetArray[ tarArrIndex ] -> nDependencyCount = 0;  
			
			int *dep_count = &( targetArray[ tarArrIndex ] -> nDependencyCount );
			index++; 		
			
			// Advance preceding spaces		
			while ( lpszLine[ index ] == ' ' )
				index++;				

			while ( lpszLine[ index ] != '\0' ) {			

				// Move pointer to next dependency							
				lpszLine += index;
				index = 0;

				// Replace blank with '\0'
				while( ( lpszLine[ index ] != ' ' ) && ( lpszLine[ index ] != '\0' ) )
					index++;
				if( lpszLine[ index ] != '\0' )				
					lpszLine[ index ] = '\0';
				index++;

				// Store dependencies 	
				strcpy( targetArray[ tarArrIndex ] -> szDependencies[ *dep_count ], lpszLine );				
				( *dep_count )++;								
			}   			
		}
		// Command line		
		else if( lpszLine[ 0 ] == '\t' ){
			lpszLine += 1;
			char **myargv;
			int numtokens; 
			numtokens = makeargv( lpszLine, " ", &myargv );
			strcpy( targetArray[ tarArrIndex ] -> szCommand, myargv[ 0 ] );
			targetArray[ tarArrIndex ] -> szParameter = myargv;
			targetArray[ tarArrIndex ] -> paraCount = numtokens;
			tarArrIndex++;
			targetArray[ tarArrIndex ] = malloc( sizeof(struct target) );				
		}		
		else{
			fprintf( stderr, "Makefile syntax error.\n" );
			exit( 0 );
		}		
	}
	int ii, jj, kk;
	
	// Handle duplicated target name
	for( ii = 0; ii < tarArrIndex; ii++ ){
		for( jj = 0; jj < tarArrIndex; jj++ ){
			if( ( ii != jj ) && ( !strcmp( targetArray[ ii ] -> szTarget, targetArray[ jj ] -> szTarget ) ) ){
				fprintf( stderr, "Duplicated target name is not allowed.\n");
				exit( 0 );
			}
		}
	}	

	int checkFile = 0;
	// Build tree		
	for ( ii = 0; ii < tarArrIndex; ii++ ) {
		for ( jj = 0; jj < targetArray[ ii ] -> nDependencyCount; jj++ ) {
			for ( kk = 0; kk < tarArrIndex; kk++ ) {
				if ( !strcmp( targetArray[ ii ] -> szDependencies[ jj ], targetArray[ kk ] -> szTarget ) ) {				
					targetArray[ ii ] -> parents[ jj ] = targetArray[ kk ];
					targetArray[ kk ] -> pStatus = 1; 			
					break;			
				}
				checkFile++;
			}
			if ( ( checkFile == tarArrIndex ) && ( is_file_exist( targetArray[ ii ] -> szDependencies[ jj ] ) == -1 ) ) {
				perror( "Dependency file does not exist" );
				exit( 0 );
			}
			checkFile = 0;	
		}
	}	

	//Close the makefile. 
	fclose( fp );

	return 0;
}

void show_error_message( char * lpszFileName ) 
{
	fprintf( stderr, "Usage: %s [options] [target] : only single target is allowed.\n", lpszFileName );
	fprintf( stderr, "-f FILE\t\tRead FILE as a maumfile.\n" );
	fprintf( stderr, "-h\t\tPrint this message and exit.\n" );
	fprintf( stderr, "-n\t\tDon't actually execute commands, just print them.\n" );
	fprintf( stderr, "-B\t\tDon't check files timestamps.\n" );
	fprintf( stderr, "-m FILE\t\tRedirect the output to the file specified .\n" );
	exit( 0 );
}

/*
	execute function will recursively calls itself until it hits a base case, then it will return 
	to its caller and its caller is responsible to fork and execute the command stored in the 
	base node. For example in the following example, 1 will call execute on 2 and 3, then 2 will
	call on 4, 3 will call on 5 and 6. 4,5 and 6 are base cases. They will return a pointer, which
	points to itself back to its parent. Then 2 and 3 will do fork and execute 4 and 5&6
	respectively, and so on. The root of the tree will be handled in the same way in the main
	function. 
    	 	1
		2 	 	3
	4		  5	  6
*/
struct target *execute( struct target *tar, int BFlag ){
	int i,j;
	int tarCount = 0, checkFlag = 0;
	int base_flag = 0;
	struct target *child;
	
	// Base case: No older dependencies or no dependency at all or none of its dependencies is target
	// Check for number of dependencies. If no dependencies, base_flag = 1
	if( !( tar -> nDependencyCount ) )
		return tar;

	// Check if all of its dependencies already exist.
	int checkexist = 0;
	for ( i = 0; i < tar -> nDependencyCount; i++ ) { 
		if ( is_file_exist( tar -> szDependencies[ i ] ) != -1 )	
			checkexist++;
	}
	if( checkexist == tar -> nDependencyCount )
		return tar;

	// Check if all of its dependencies are not targets in the makefile
	for( i = 0; i < tar -> nDependencyCount; i++ ){
		for( j = 0; j < tarArrIndex; j++ ){
			if( !strcmp( ( tar -> szDependencies[ i ] ), targetArray[ j ] -> szTarget ) ) {

				checkFlag = 1;
				break;
			}	
		}
	}
	if( ! checkFlag ){
		base_flag = 1;
	}

	// Check if any dependencies has newer timestamp. If so, base_flag = 0
	tarCount = 0;
	pid_t cpid;
	int x, exStatus; 	
	if( !base_flag ){
		for( i = 0; i < tar -> nDependencyCount; i++ ){
			if( !BFlag ){
				if( ( compare_modification_time( tar -> parents[ i ] -> szTarget, tar -> szTarget ) == 1 ) ){
					tarCount++;
				}
			}	
			if( !( is_file_exist( tar -> parents[ i ] -> szTarget ) == -1 ) ){
				tarCount++;	
			}
		}
		if( ( BFlag == 1 ) && ( tarCount == ( 2 * ( tar -> nDependencyCount ) ) ) ) 
			base_flag = 1;
		else if( ( BFlag == 0 ) && ( tarCount == ( tar -> nDependencyCount ) ) )
			base_flag = 1;
	}


	int sFlag = 0;

	// Return a pointer to the base node
	if( base_flag && ( tar -> pStatus == 1 ) )
		return tar;	  	
	else if ( base_flag ){
		tar -> nDependencyCount++;
		sFlag = 1; 
	}

	for( i = 0; i < ( tar -> nDependencyCount ); i++ ){		
		if( !sFlag )
			child = execute( tar -> parents[ i ], BFlag );
		else{
			tar -> nDependencyCount--;
			child = tar;
		}			
		cpid = fork();
		if( cpid == -1 ){
			perror( "Failed to fork" );
			exit( 0 );
		}
		if( cpid == 0 ){	// Child process
			exStatus = execvp( child -> szCommand, child -> szParameter );
			if( exStatus == -1 ){
				perror( "Fail to execute" );
				exit( 0 );
			}
			if( !is_file_exist( tar -> szTarget ) ){
				fprintf( stderr, "Command failed. File do not exist.\n" );
				exit( 0 );
			}
			exit( 0 );
		}
	}
	// Wait for all children
	int indexw = 0, loc;
	for(;;){
		cpid = wait( &loc );
		if( cpid != -1 ){				
			indexw++;
		}
		
		// Handle synta error.
		if( loc == 256 ){
			fprintf( stderr, "Syntax error occurred.\n" );
			exit( 0 );		
		}
		if( ( cpid == -1) && ( errno != EINTR ) )
			break;	
	}
	return tar;

}

int main( int argc, char **argv ) {
	// Declarations for getopt
	extern int optind;
	extern char * optarg;
	int ch;
	char * format = "f:hnBm:";
	int nFlag = 0, BFlag = 0, mFlag, optFlag = 1;		// optFlag = 1 when no options used in command line.
	
	// Default makefile name will be Makefile
	char szMakefile[ 64 ] = "Makefile";
	char szTarget[ 64 ];
	char szLog[ 64 ];

	while( ( ch = getopt( argc, argv, format ) ) != -1 ) 	// No options, getopt returns -1 
	{
		switch( ch ) 
		{
			case 'f':
				strcpy( szMakefile, strdup( optarg ) );		// Argument after -f stores in optarg
				optFlag = 0;
				break;
			case 'n':
				optFlag = 0;
				nFlag = 1; 
				break;
			case 'B':
				BFlag = 1;
				break;
			case 'm':
				strcpy( szLog, strdup( optarg ) );
				mFlag = 1;
				break;
			case 'h':
			default:
				show_error_message( argv[ 0 ] );
				exit( 1 );
		}
	}
	
	argc -= optind;		
	argv += optind;		// Move to next processing element. 
	
	// At this point, what is left in argv is the targets that were 
	// specified on the command line. argc has the number of them.
	
	if( argc > 1 ){
		show_error_message( argv[ 0 ] );
		return EXIT_FAILURE;
	}

	
	// Parse graph file or die 
	if( ( parse( szMakefile ) ) == -1 ){
		return EXIT_FAILURE;
	}

	// You may start your program by setting the target that make4061 should build.
	// if target is not set, set it to default (first target from makefile)
	int i, j, tarFlag = 0, targetIndex = 0;		// targetIndex holds the index of the target user try to execute.

	if( argc == 1 ){		//
		strcpy( szTarget, *argv );

		// Find matching target
		for( i = 0; i < tarArrIndex; i++ ){
			if( !strcmp( szTarget, targetArray[ i ] -> szTarget ) ){
				tarFlag = 1;
				targetIndex = i;
				break;	
			}
		}

		// Check if target is valid. That is, in the makefile 
		if( !tarFlag ){
			fprintf( stderr, "Required target is not in the makefile.\n" );
			exit( 0 );
		}
	}
	
	// Handle -m option
	if( mFlag ){
		mode_t fdmode = ( S_IRWXU | S_IRGRP |S_IROTH );
		int fd = open( szLog, O_CREAT|O_RDWR|O_APPEND, fdmode );
		if( fd == -1 ){
			perror( "Failed to open file" );
			exit( 0 );
		}
		dup2( fd, 1 );
		close( fd );
	}	

	// Handle -n option
	if( nFlag ){
		if( !tarFlag ){	
			for( i = 0; i < tarArrIndex; i++ ){
				printf( "Command: " );
				for( j = 0; j < targetArray[ i ] -> paraCount; j++ ){
					printf( "%s ", ( targetArray[ i ] -> szParameter )[ j ] );
				}
				printf( "\n" );	
			}
		}
		else{
			printf( "Command: " );
			for( j = 0; j < targetArray[ targetIndex ] -> paraCount; j++ ){
				printf( "%s ", ( targetArray[ targetIndex ] -> szParameter )[ j ] );
			}
			printf( "\n" );	
		}
		return 0;	
	}

	// Start executing
	struct target *tarTar;
	pid_t cpid;
	int exStatus;
	tarTar = execute( targetArray[ targetIndex ], BFlag );
	cpid = fork();
	if( cpid == -1 ){
		perror( "Failed to fork" );
		exit( 0 );
	}
	if( cpid == 0 ){	// Child process
		exStatus = execvp( tarTar -> szCommand, tarTar -> szParameter );
		if( exStatus == -1 ){
			perror( "Fail to execute" );
			exit( 0 );
		}
		if( !is_file_exist( tarTar -> szTarget ) ){
			fprintf( stderr, "Command failed. File do not exist.\n" );
			exit( 0 );
		}
		exit( 0 );
	}
	else{		// Parent process
		int loc;
		cpid = wait( &loc );
		if( cpid == -1 ){
			perror( "Wait failed" );
			exit( 0 );
		}

		// Handle synta error.
		if( loc == 256 ){
			fprintf( stderr, "Syntax error occurred.\n" );
			exit( 0 );		
		}
	}
	
	return EXIT_SUCCESS;
}







