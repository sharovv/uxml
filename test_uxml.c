#include <uxml.h>
#include <stdio.h>

uxml_error_t e;

int print_error( uxml_error_t *e )
{
  fprintf( stderr, "Line %d column %d: %s\n", e->line, e->column, e->text );
  return 0;
}

int test1()
{
  uxml_node_t *root;
  /* test header and comment */
  const char x[] = 
    "<?xml version='1.0' encoding=\"WINDOWS-1251\" ?>\n"
    "<!-- comment -->\n"
    "<nodeR>\n"
    "<nodeC/>\n"
    "<nodeA attr1A=\"value1A\"/>\n"
    //"<nodeB attr1B=\"value1B\" attr2B=\"value2B\">  contentB_a <!-- commentB --> contentB_b <nodeC/> commentB_c </nodeB>\n"
    "</nodeR>\n";
  
  if( (root = uxml_parse( x, sizeof( x ), &e )) == NULL ) return print_error( &e );
  uxml_dump_list( root );
  return 1;
}

main()
{
  if( !test1() ) return 1;
  return 0;
}
