/* Pipetest.c - some relics of pipe testing.  Haven't
 * gotten it to work... */

enum PIPES { READ, WRITE }; /* Constants 0 and 1 for READ and WRITE */
#define NUMPROBLEM 4

int test()
{
int pipeEnds[2];
int old[2];
int res;
int pid1, pid2;
int term1, term2;

printf("Testing a pipe\n");
if (_pipe(pipeEnds, 64*1024, _O_TEXT) < 0)
    {
    warn("Couldn't open pipe\n");
    return slcError;
    }

/* Reassign standard output to write side of pipe
 * and spawn first process asyncronously. */
res = old[1] = _dup(1);
res = _dup2(pipeEnds[1], 1);
pid1 = _spawnlp(_P_NOWAIT, "cat", "cat foo.awk", NULL);
res = _dup2(old[1], 1);

/* Restore standard output.  Reassign standard
 * input to read side of pipe.  Spawn process
 * asyncronously. */
res = old[0] = _dup(0);
res = _dup2(pipeEnds[0], 0);
pid2 = _spawnlp(_P_NOWAIT, "awk", "awk", NULL);
res = _dup2(old[0], 0);

_cwait( &term1, pid1, WAIT_CHILD );
printf("Done with first child\n");
_cwait( &term2, pid2, WAIT_CHILD );
printf("Done with second child\n");
close(pipeEnds[0]);
close(pipeEnds[1]);
errAbort("All for now");
}
  
void main( int argc, char *argv[] )
{

   int hpipe[2];
   char hstr[20];
   int pid, problem, c;
   int termstat;

   /* If no arguments, this is the spawning process */
   if( argc == 1 )
   {

      setvbuf( stdout, NULL, _IONBF, 0 );

      /* Open a set of pipes */
      if( _pipe( hpipe, 256, O_BINARY ) == -1 )
          exit( 1 );


      /* Convert pipe read handle to string and pass as argument 
       * to spawned program. Program spawns itself (argv[0]).
       */
      itoa( hpipe[WRITE], hstr, 10 );
      if( ( pid = spawnl( P_NOWAIT, argv[0], argv[0], 
            hstr, NULL ) ) == -1 )
          printf( "Spawn failed" );


      /* Read problem from pipe and calculate solution. */
      for( c = 0; c < NUMPROBLEM+1; c++ )
      {
      int readSize;

        readSize = read( hpipe[READ], (char *)&problem, sizeof( int ) );
        if (readSize == 0)
            {
            printf("Ok Son, I guess we're done\n");
            break;
            }
        printf( "Son, the square root of %d is %3.2f.\n",
                 problem, sqrt( ( double )problem ) );

      }

      /* Wait until spawned program is done processing. */
      _cwait( &termstat, pid, WAIT_CHILD );
      if( termstat & 0x0 )
         printf( "Child failed\n" );


      close( hpipe[READ] );

   }

   /* If there is an argument, this must be the spawned process. */
   else
   {

      /* Convert passed string handle to integer handle. */
      hpipe[WRITE] = atoi( argv[1] );

      for( problem = 1000; problem <= NUMPROBLEM * 1000; problem += 1000)
      {

         printf( "DAD, what is the square root of %d?\n", problem );
         write( hpipe[WRITE], (char *)&problem, sizeof( int ) );

      }
   close( hpipe[WRITE] );
   }
}

  
