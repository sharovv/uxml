#include <uxml.h>
#include <stdio.h>
#include <string.h>

uxml_error_t e;

int print_error( uxml_error_t *e )
{
  fprintf( stderr, "Line %d column %d: %s\n", e->line, e->column, e->text );
  return 0;
}

void uxml_dump_list( uxml_node_t *root );

int test( const char *x )
{
  uxml_node_t *root;
  int n = strlen( x );

  if( (root = uxml_parse( x, n, &e )) == NULL ) 
    return print_error( &e );
  printf( "===\n%s---\n", x );
  uxml_dump_list( root );
  printf( "---\n" );
  fflush( stdout );
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
    "<nodeC/>\n"
    "<nodeD>contentD0</nodeD>\n"
    "<nodeD>contentD1</nodeD>\n"
    "<nodeD>\n"
      "contentD2\n"
      "<nodeDD>contentDD0</nodeDD>\n"
      "<nodeDDX>contentDDX</nodeDDX>\n"
      "<nodeDD>contentDD1</nodeDD>\n"
      "<nodeDD>contentDD2</nodeDD>\n"
    "</nodeD>\n"
    "<nodeD>contentD3</nodeD>\n"
    "<nodeD>contentD4</nodeD>\n"
    "</nodeR>";

  if( (root = uxml_parse( xml, sizeof( xml ), &e )) == NULL ) 
    return print_error( &e );

  printf( "===\n%s---\n", xml );
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
  printf( "prev of nodeB is %s\n", uxml_name( uxml_prev( uxml_node( root, "nodeB" ) ) ) );
  printf( "next of nodeB is %s\n", uxml_name( uxml_next( uxml_node( root, "nodeB" ) ) ) );
  printf( "/nodeD[4]=\"%s\"\n", uxml_get( root, "/nodeD[4]" ) );
  printf( "/nodeD[2]/*=\"%s\"\n", uxml_get( root, "/nodeD[2]/*" ) );
  printf( "/nodeD[2]/*[1]=\"%s\"\n", uxml_get( root, "/nodeD[2]/*[1]" ) );
  printf( "/nodeD[2]/nodeDD[1]=\"%s\"\n", uxml_get( root, "/nodeD[2]/nodeDD[1]" ) );
  uxml_free( root );
  return 1;
}

int test_base64()
{
  char b[64], d[64];
  static const char c[] = "9876543210", f[] = "OTg3 Nj U0Mz IxMA==";
  int i, k;

  if( (i = uxml_encode64( b, sizeof( b ), c, sizeof( c ) - 1 )) == 0 )
  {
    printf( "uxml_encode64 failed\n" );
    return 0;
  }
  if( strcmp( b, "OTg3NjU0MzIxMA==" ) != 0 )
  {
    printf( "uxml_encode64 failed\n" );
    return 0;
  }
  printf( "encode64 \"%s\" -> \"%s\"\n", c, b );
  if( (k = uxml_decode64( d, sizeof( d ), f, sizeof( f ) - 1 )) == 0 )
  {
    printf( "uxml_decode64 failed\n" );
    return 0;
  }
  d[k] = 0;
  if( memcmp( d, c, sizeof( c ) - 1 ) != 0 )
  {
    printf( "uxml_decode64 failed\n" );
    return 0;
  }
  printf( "decode64 \"%s\" -> \"%s\"\n", f, d );
  return 1;
}

int main()
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
    "<_1 attr_1=\"value_1\"> content_1 </_1>\n"
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
  if( !test_base64() ) return 1;
  return 0;
}
