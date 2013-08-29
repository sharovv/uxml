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
 * \param node - node's pointer.
 * \return pointer to node's content.
 * Content is a zero-terminated string.
 * This pointer are valid until \c uxml_free called.
 */
const char *uxml_content( uxml_node_t *node );

/*! Get node's name
 *
 * \param node - node's pointer.
 * \return pointer to node's name.
 * The name is a zero-terminated string.
 * This pointer are valid until \c uxml_free called.
 */
const char *uxml_name( uxml_node_t *node );

/*! Get node's first attribute
 *
 * \param node - node's pointer.
 * \return attribute's node.
 * The content of this node is the value of attribute,
 * that can aquire through \c uxml_content call.
 * The name of this is equal to the name attribute,
 * and can be aquired by \c uxml_name call.
 * Others attributes can be accessed by \c uxml_next calls.
 * Also, this node cannot have childred node.
 */
uxml_node_t *uxml_first_attr( uxml_node_t *node );

/*! Get first node's child
 *
 * \param node - node's pointer.
 * \return First child of a node, or NULL if there is no children of this node. 
 * Other children nodes can be obtained by sequence of \c uxml_next calls.
 */
uxml_node_t *uxml_child( uxml_node_t *node );

/*! Get next node
 *
 * \param node - node's pointer.
 * \return Next node, or NULL if this is last child of parent node.
 */
uxml_node_t *uxml_next( uxml_node_t *node );

/*! Get attribute value
 *
 * \param node - node's pointer;
 * \name - name of attribute.
 * \return pointer to zero-teminated string with attribute's value,
 * or pointer to empty string if no there is no attribute with specified name.
 */
const char *uxml_attr( uxml_node_t *node, const char *name );

/*! Free XML tree
 *
 * \param node - root node's pointer;
 * Frees whole XML tree, and no nodes will be accessed after this call.
 */
void uxml_free( uxml_node_t *root );

void uxml_dump_list( uxml_node_t *root );

#endif
