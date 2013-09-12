#include <uxml.h>
#include <time.h>
#include <stdio.h>

int main( int argc, char *argv[] )
{
  int i;
  uxml_node_t *r;
  uxml_error_t e;

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
  return 0;
}