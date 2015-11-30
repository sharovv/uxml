#include <uxml.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>

typedef unsigned long long ticks_t;

#if defined(_MSC_VER)
#pragma warning(disable:4996)
#include <intrin.h>

ticks_t ticks()
{
  return __rdtsc();
}
#elif defined(__GNUC__)
ticks_t ticks()
{
  ticks_t result;
  __asm__ __volatile__("rdtsc" : "=A" (result));
  return result;
}
#else
#include <time.h>

ticks_t ticks()
{
  return clock();
}
#endif

int uxml_get_initial_allocated( uxml_node_t *root );

int main( int argc, char *argv[] )
{
  FILE *fp;
  void *b;
  int size, n;
  int i, j, k = 0;
  uxml_node_t *r;
  uxml_error_t e;
  time_t t, t0;
  ticks_t tck, freq;

  for( i = 1; i < argc; i++ )
  {
    if( argv[i][0] != '-' )
    {
      break;
    }
  }

  if( i == argc )
  {
    return fprintf( stderr, "No file specified\n" );
  }

  if( (fp = fopen( argv[i], "rb" )) == NULL )
  {
    return fprintf( stderr, "fopen(%s) failed\n", argv[i] );
  }
  fseek( fp, 0, SEEK_END );
  n = ftell( fp );
  fseek( fp, 0, SEEK_SET );
  if( (b = malloc( n )) == NULL )
  {
    fclose( fp );
    return fprintf( stderr, "malloc(%d) failed\n", n );
  }
  size = fread( b, 1, n, fp );
  fclose( fp );
  if( size != n )
  {
    free( b );
    return fprintf( stderr, "fread failed\n" );
  }

  for( t0 = time( &t0 ); time( &t ) == t0; );
  tck = ticks();
  for( t0 = t; time( &t ) == t0; );
  freq = ticks() - tck;
  
  tck = ticks();
  do
  {
    if( (r = uxml_parse( b, n, &e )) == NULL )
    {
      return fprintf( stderr, "Line %d column %d: %s\n", e.line, e.column, e.text );
    }
    j = uxml_get_initial_allocated( r );
    uxml_free( r );
    t = time( &t ) - t0;
    k++;
  }
  while( (ticks() - tck) < 5 * freq );
  tck = ticks() - tck;

  printf( "CPU %.3f GHz: %d ticks/character, %d bytes/s, %d bytes allocated (%d%% overhead)\n", 
  (double)( 0.000000001 * freq ),
  (int)(tck / (n * k)),
  (int)(freq * n * k / tck),
  j,
  (j - n) * 100 / n );

  free( b );
  return 0;
}
