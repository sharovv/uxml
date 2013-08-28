#include <uxml.h>
#include <time.h>
#include <stdio.h>

int main( int argc, char *argv[] )
{
  int i, j, k, n = 100;
  uxml_node_t *r;
  uxml_error_t e;
  time_t t, t0;

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
  if( (r = uxml_load( argv[i], &e )) == NULL )
  {
    return fprintf( stderr, "Line %d column %d: %s\n", e.line, e.column, e.text );
  }
  uxml_dump_list( r );
  uxml_free( r );
  
  for( t0 = time( &t0 ), k = 1; (time( &t ) - t0) < 5; k++ )
  {
    for( j = 0; j < n; j++ )
    {
      r = uxml_load( argv[i], &e );
      uxml_free( r );
    }
    k++;
  }
  t = time( &t ) - t0;
  k = k * n / (int)t;
  printf( "%d parse/second\n", k );
  return 0;
}