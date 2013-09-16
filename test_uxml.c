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

int test_navigate()
{
  uxml_node_t *root, *node;

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
  printf( "/attrR1=\"%s\"\n", uxml_get( root, "/attrR1" ) );
  printf( "attrR1=\"%s\"\n", uxml_get( root, "attrR1" ) );
  printf( "/nodeA/attrA1=\"%s\"\n", uxml_get( root, "/nodeA/attrA1" ) );
  node = uxml_node( root, "/nodeA" );
  printf( "attrA1=\"%s\"\n", uxml_get( node, "attrA1" ) );
  printf( "..=\"%s\"\n", uxml_get( node, ".." ) );
  printf( "../attrR1=\"%s\"\n", uxml_get( node, "../attrR1" ) );
  printf( "../nodeB=\"%s\"\n", uxml_get( node, "../nodeB" ) );
  printf( "../nodeB/attrB1=\"%s\"\n", uxml_get( node, "../nodeB/attrB1" ) );
  printf( "../nodeB/attrB2=\"%s\"\n", uxml_get( node, "../nodeB/attrB2" ) );
  uxml_free( root );
  return 1;
}

int test_add()
{
  uxml_node_t *ra, *rb, *c;
  unsigned char *s;

  const char xml_a[] = 
    "<?xml version='1.0' encoding='UTF-8'?>\n"
    "<nodeRA attrRA1='valueRA1'>\n"
    "contentRA &lt;&#9;&gt;&amp;&apos;&quot;\n"
    "<nodeA attrA1='valueA1'/>\n"
    "<nodeB attrB1='valueB1' attrB2='valueB2 &lt;&#9;&gt;&amp;&apos;&quot;'>contentB</nodeB>\n"
    "</nodeRA>";

  const char xml_b[] = 
    "<?xml version='1.0' encoding='UTF-8'?>\n"
    "<nodeRB attrR1='valueRB'>\n"
    "contentRB\n"
    "<nodeC attrC1='valueC1'>contentC</nodeC>\n"
    "</nodeRB>";

  if( (ra = uxml_parse( xml_a, sizeof( xml_a ), &e )) == NULL ) 
    return print_error( &e );

  if( (s = uxml_dump( ra )) != NULL )
    printf( "%s", s ); 

  if( (rb = uxml_parse( xml_b, sizeof( xml_b ), &e )) == NULL ) 
    return print_error( &e );

  printf( "= a =\n" );
  uxml_dump_list( ra );
  printf( "= b =\n" );
  uxml_dump_list( rb );
  printf( "= a + b =\n" );

  c = uxml_node( rb, "/nodeC" );
  uxml_add_child( ra, c );
  uxml_dump_list( ra );

  if( (s = uxml_dump( ra )) != NULL )
    printf( "%s", s ); 

  return 1;
}

int test_set()
{
  uxml_node_t *a;
  char t[16];

  const char xml_a[] = 
    "<?xml version='1.0' encoding='UTF-8'?>\n"
    "<nodeR attrR1='valueR1'>\n"
    "contentR\n"
    "<nodeA attrA1='valueA1'/>\n"
    "<nodeB attrB1='valueB1' attrB2='valueB2'>contentB</nodeB>\n"
    "</nodeR>";

  if( (a = uxml_parse( xml_a, sizeof( xml_a ), &e )) == NULL ) 
    return print_error( &e );

  printf( "=== test_set origin ===\n" );

  uxml_dump_list( a );

  uxml_set( a, "/nodeA", "12345", 0 );
  uxml_set( a, "/nodeA/attrA1", "valueA1+valueA2+valueA3+valueA4+valueA5+valueA6", 0 );
  uxml_set( a, "/nodeB", "contentB1+contentB2+contentB3+contentB4", 0 );
  uxml_set( a, "/nodeB/attrB1", "valueB1+valueB2+valueB3+valueB4+valueB5+valueB6", 0 );

  printf( "=== test_set result1 ===\n" );
  uxml_dump_list( a );

  uxml_set( a, "/nodeB/attrB2", "valueBB1+valueBB2+valueBB3+valueBB4+valueBB5+valueBB6", 0 );

  printf( "=== test_set result2 ===\n" );
  uxml_dump_list( a );

  uxml_copy( a, "/nodeA", t, sizeof( t ) );
  printf( "copy(/nodeA)=\"%s\"\n", t );

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
  if( !test_navigate() ) return 1;
  if( !test_add() ) return 1;
  if( !test_set() ) return 1;
  return 0;
}
