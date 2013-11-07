#include <stdio.h>
#include <uxml.h>

int print_error( uxml_error_t *e )
{
  fprintf( stderr, "Line %d column %d: %s\n", e->line, e->column, e->text );
  return 1;
}

int main()
{
  uxml_node_t *root;
  uxml_error_t e;

  const char xml[] = 
    "<?xml version='1.0' encoding='UTF-8'?>\n"
    "<nodeR attrR1='valueR1'>\n"
    "contentR1\n"
    "<nodeA attrA1='valueA1'/>\n"
    "<nodeB attrB1='valueB1'>contentB<attrB2>valueB2</attrB2></nodeB>\n"
    "</nodeR>";

  if( (root = uxml_parse( xml, sizeof( xml ), &e )) == NULL ) 
    return print_error( &e );

  printf( "root content=\"%s\"\n", uxml_get( root, NULL ) );
  uxml_free( root );
  return 0;
}
