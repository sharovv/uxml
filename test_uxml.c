#include <uxml.h>
#include <stdio.h>
#include <string.h>

uxml_error_t e;

int print_error( uxml_error_t *e )
{
  fprintf( stderr, "Line %d column %d: %s\n", e->line, e->column, e->text );
  return 0;
}

int test( const char *x )
{
  uxml_node_t *root;
  int n = strlen( x );

  if( (root = uxml_parse( x, n, &e )) == NULL ) 
    return print_error( &e );
  uxml_dump_list( root );
  printf( "---\n" );
  uxml_free( root );
  return 1;
}

main()
{
  const char test_header_and_empty_root[] = 
    "<?xml version='1.0' encoding='UTF-8'?>\n"
    "<nodeR/>\n";
  const char test_root_comment[] = 
    "<?xml version='1.0' encoding='UTF-8'?>\n"
    "<!-- comment -->\n"
    "<nodeR/>\n";
  const char test_node_empty_content[] = 
    "<?xml version='1.0' encoding='UTF-8'?>\n"
    "<nodeR>\n"
    "</nodeR>\n";
  const char test_node_content[] = 
    "<?xml version='1.0' encoding='UTF-8'?>\n"
    "<nodeR>\n"
    "  contentR1 contentR2    contentR3    \n"
    "</nodeR>\n";
  const char test_node_comment[] = 
    "<?xml version='1.0' encoding='UTF-8'?>\n"
    "<!-- comment -->\n"
    "<nodeR> contentR1   <!-- commentR <node> & everything else --> contetR2    \n\n</nodeR>\n";
  const char test_node_attr[] = 
    "<?xml version='1.0' encoding='UTF-8'?>\n"
    "<nodeR>\n"
    "<nodeA attrA=\"valueA\"/>\n"
    "</nodeR>\n";
  const char test_node_attr2[] = 
    "<?xml version='1.0' encoding='UTF-8'?>\n"
    "<nodeR>\n"
    "<nodeA attrA1=\"valueA1\" attrA2=\"valueA2\" attrA3 = \"  value A3  \" />\n"
    "</nodeR>\n";
  const char test_nodes[] = 
    "<?xml version='1.0' encoding='UTF-8'?>\n"
    "<!-- comment -->\n"
    "<nodeR>\n"
    "<nodeA attrA1=\"valueA1\" attrA2=\"valueA2\"/>\n"
    "<nodeB attrB1=\"valueB1\" attrB2=\"valueB2\">  contentB_a <!-- commentB --> contentB_b <nodeC/> contentB_c <nodeD> contentD </nodeD></nodeB>\n"
    "</nodeR>\n";
  const char test_escape[] = 
    "<?xml version='1.0' encoding='UTF-8'?>\n"
    "<!-- comment -->\n"
    "<nodeR attrR=\"&lt;&gt;&amp;&apos;&quot;&#64;&#x4A;\"> &lt; &gt; &amp; &apos; &quot; &#64; &#x4A; </nodeR>\n";
  
  if( !test( test_header_and_empty_root ) ) return 1;
  if( !test( test_root_comment ) ) return 1;
  if( !test( test_node_empty_content ) ) return 1;
  if( !test( test_node_content ) ) return 1;
  if( !test( test_node_comment ) ) return 1;
  if( !test( test_node_attr ) ) return 1;
  if( !test( test_node_attr2 ) ) return 1;
  if( !test( test_nodes ) ) return 1;
  if( !test( test_escape ) ) return 1;
  return 0;
}
