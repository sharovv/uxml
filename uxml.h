#ifndef _uxml_h
#define _uxml_h

typedef struct _uxml_node_t uxml_node_t;
typedef struct _uxml_error_t
{
  const char *text;
  int line;
  int column;
} uxml_error_t;

/*! Parse XML data from memory
 *
 * Buffer must contain valid XML data include header.
 * \param xml_data - pointer buffer with XML data, may be zero-terminated;
 * \param xml_length - length of XML data in buffer \c xml_data;
 * \param error - pointer to structure, which will be fill with error 
 * description and it's position in XML data (row and column).
 * \return Root node, or NULL in case of error.
 */
uxml_node_t *uxml_parse( const char *xml_data, const int xml_length, uxml_error_t *error );

/*! Parse XML from file
 *
 * \param xml_file - name of file with XML data;
 * \param error - pointer to structure, which will be fill with error 
 * description and it's position in XML data (row and column).
 * \return Root node, or NULL in case of error.
 */
uxml_node_t *uxml_load( const char *xml_file, uxml_error_t *error );

/*! Get node's content
 *
 * Return pointer to content of the specified node - element or attribute.
 * There is no differences between two forms of following XML definitions,
 * with attributes:
 * \verbatim
<node attribute="value"/>
\endverbatim
 * and with child node:
 * \verbatim
<node><attribute>value</attribute></node>
\endverbatim
 * Getting attribute value and content of node are the same.
 * Node is specified by the path. 
 * The path is the equivalent to UNIX filesystem path.
 * This path consist of name's set, separated with '/' characters.
 * If first character of path is '/', 
 * then path is considered as absolute from root of whole XML,
 * regardrless of current node.
 * And else, if path's first character is not '/', 
 * then path is relative from current node.
 * Also, special characters ".." instead name means the parent node's access.
 * When current node's contents is needed, the path must point to
 * empty string or to NULL.
 * Returned pointer is pointed to node content.
 * This content are consider as constant, and valid until
 * parsed XML will free by \c uxml_free call.
 * \param node - node's pointer;
 * \param path - node's path. 
 * \return pointer to node's content.
 */
const char *uxml_content( uxml_node_t *node, const char *path );

/*! Get the size of node's content
 *
 * \param node - node's pointer.
 * \return content size in bytes 
 * (length of zero-terminated string).
 */
int uxml_content_size( uxml_node_t *node, const char *path );

/*! Get node by path
 *
 */
uxml_node_t *uxml_node( uxml_node_t *node, const char *path );

/*! Get node's name
 *
 * \param node - node's pointer.
 * \return pointer to node's name.
 * The name is a zero-terminated string.
 * This pointer are valid until \c uxml_free called.
 */
const char *uxml_name( uxml_node_t *node );

/*! Get first node's child
 *
 * \param node - node's pointer.
 * \return First child of a node (element or attribute),
 * or NULL if there is no children of this node. 
 * Other children nodes can be obtained by sequence of \c uxml_next calls.
 */
uxml_node_t *uxml_child( uxml_node_t *node );

/*! Get next node
 *
 * \param node - node's pointer.
 * \return Next node, or NULL if this is last child of parent node.
 */
uxml_node_t *uxml_next( uxml_node_t *node );

/*! Add child node
 */
int uxml_add_child( uxml_node_t *root, uxml_node_t *child );

/*! Free XML tree
 *
 * \param node - root node's pointer;
 * Frees whole XML tree, and no nodes will be accessed after this call.
 */
void uxml_free( uxml_node_t *root );

/*! Return dump of whole XML
 */
char *uxml_dump( uxml_node_t *root );

void uxml_dump_list( uxml_node_t *root );

#endif
