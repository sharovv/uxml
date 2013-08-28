#ifndef _uxml_h
#define _uxml_h

typedef struct _uxml_node_t uxml_node_t;
typedef struct _uxml_error_t
{
  const char *text;
  int line;
  int column;
} uxml_error_t;

uxml_node_t *uxml_parse( const char *xml_data, const int xml_length, uxml_error_t *error );
uxml_node_t *uxml_load( const char *xml_file, uxml_error_t *error );
const char *uxml_content( uxml_node_t *node );
uxml_node_t *uxml_first_attr( uxml_node_t *node );
uxml_node_t *uxml_child( uxml_node_t *node );
uxml_node_t *uxml_next( uxml_node_t *node );
const char *uxml_attr( uxml_node_t *node, const char *name );
void uxml_free( uxml_node_t *root );
void uxml_dump_list( uxml_node_t *root );

#endif
