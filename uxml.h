#ifndef _uxml_h
#define _uxml_h

#ifdef __cplusplus
extern "C" {
#endif

/*! XML node structure
 *
 * Pointer to this structure is used 
 * in various functions of the uxml-library.
 * This pointer may define one XML-node or 
 * whole XML tree.
 */
typedef struct _uxml_node_t uxml_node_t;

/*! XML error description
 *
 * When error has occured, the fields in this structure
 * will be filled with error description text and its 
 * position, where error was found.
 */
typedef struct _uxml_error_t 
{
  const char *text;
  int line;
  int column;
} uxml_error_t;

/*! Parse XML data from memory
 *
 * Buffer must contain valid XML data.
 * If XML data contain no header with "version" and "encoding" attributes,
 * then values "1.0" and "UTF-8" will be used by default.
 * It is possible to create empty XML tree, specify "<root/>",
 * for example, as \c xml_data string.
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
 * Returns pointer to content of the specified node - element or attribute.
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
 * This path consists of name's set, separated with '/' characters.
 * If first character of path is '/', 
 * then path is considered as absolute from root of whole XML,
 * regardrless of specified node. 
 * Name of root element is not required in this case.
 * And else, if path's first character is not '/', 
 * then path is relative from specified node.
 * Also, special characters ".." instead the name means the parent node's access.
 * Node's path can choose one node from array. For example,
 * if multiple nodes with same name "abc" exists, the name "abc[N]" means
 * N-th node in the row of "abc" nodes, numbering starts from 0.
 * Moreover, name can substitute with wildcard "*", in this case
 * no differences in names.
 * When content of specified \c node is needed, the path must point to
 * empty string "" or to NULL.
 * Returned pointer is pointed to node content.
 * This content is concantenated from all parts, 
 * separated by child nodes or comment, and with all stripped spaces. 
 * For example, content of the \c node_a:
 * \verbatim
<node_a>  content1  <node_b/>  content2  <!-- comment -->  content3   </node_a>
\endverbatim
 * will result to "content1 content2 content3".
 * This content are consider as constant, and valid until
 * \c uxml_free call has been occured.
 * \param node - node's pointer;
 * \param path - node's path. 
 * \return pointer to node's content, or NULL, if specified node doesn't exists.
 */
const char *uxml_get( uxml_node_t *node, const char *path );

/*! Get integer value
 *
 * Like a \c uxml_get, but convert node's content to integer type.
 * \param node - node's pointer;
 * \param path - node's path.
 * \return node's content converted to the integer type.
 */
int uxml_int( uxml_node_t *node, const char *path );

#if !defined( UXML_DISABLE_DOUBLE )
/*! Get real type value
 *
 * Like a \c uxml_get, but convert node's content to double precision type.
 * \param node - node's pointer;
 * \param path - node's path.
 * \return Node's content converted to the real double-precision type.
 */
double uxml_double( uxml_node_t *node, const char *path );
#endif

/*! Return user's pointer
 */
void *uxml_user( uxml_node_t *node, const char *path );

/*! Set the user's pointer
 *
 */
void uxml_set_user( uxml_node_t *node, const char *path, void *user );

/*! Get the size of node's content
 *
 * \param node - node's pointer, root or branch.
 * \param path - node's path, see \c uxml_content description.
 * \return content size in bytes 
 * (length of zero-terminated string).
 */
int uxml_size( uxml_node_t *node, const char *path );

/*! Get node by path
 *
 * \param node - node's pointer, root or branch.
 * \param path - node's path, see \c uxml_content description.
 * \return pointer to specified node, or NULL, if path is invalid.
 */
uxml_node_t *uxml_node( uxml_node_t *node, const char *path );

/*! Get node's name
 *
 * Returns the pointer to the name of specified node.
 * \param node - node's pointer.
 * \return pointer to node's name.
 * The name is a zero-terminated string.
 * This pointer are valid until \c uxml_free called.
 */
const char *uxml_name( uxml_node_t *node );

/*! Get first children element or attribute
 *
 * \param node - node's pointer.
 * \return First child of specified node - element or attribute,
 * or NULL if there is no children of this node. 
 * Other children nodes can be obtained by sequence of \c uxml_next calls.
 */
uxml_node_t *uxml_child( uxml_node_t *node );

/*! Get first node's child node
 *
 * \param node - node's pointer.
 * \return First child of specified node - element only,
 * or NULL if there is no children of this node. 
 * Other children nodes can be obtained by sequence of \c uxml_next calls.
 */
uxml_node_t *uxml_child_node( uxml_node_t *node );

/*! Get first node's attribute
 *
 * \param node - node's pointer.
 * \return First attribute of specified node,
 * or NULL if there is no attributes children of this node.
 * Other attributes can be obtained by sequence of \c uxml_next calls.
 */
uxml_node_t *uxml_first_attr( uxml_node_t *node );

/*! Get next attribute
 *
 * \param node - node's pointer.
 * \return Next attribute, or NULL if this is last attribute of parent node.
 */
uxml_node_t *uxml_next_attr( uxml_node_t *node );

/*! Get next node
 *
 * \param node - node's pointer.
 * \return Next node, or NULL if this is last child of parent node.
 */
uxml_node_t *uxml_next( uxml_node_t *node );

/*! Get previous node
 *
 * \param node - node's pointer.
 * \return Previous node, or NULL if this is first child of parent node.
 */
uxml_node_t *uxml_prev( uxml_node_t *node );

/*! Free XML tree
 *
 * \param node - root node's pointer;
 * Frees whole XML tree, and no nodes can be accessed after this call.
 */
void uxml_free( uxml_node_t *root );

/*! Encode to base64 sequence
 *
 * Encode binary data into the Base64 text data.
 * Because the every four encoded characters is derived from three
 * origin bytes, the destination buffer must be as 4/3 times more size
 * than original, plus 4 padding characters and 1 zero byte.
 * \param dst - pointer to destination buffer with size = (n_src * 4 / 3) + 4 + 1 at least;
 * \param n_dst - maximum possible size of destination buffer;
 * \param src - pointer to original binary data;
 * \param n_src - size of original binary data in bytes.
 * \return size of encoded sequence in bytes, stored in \c dst buffer;
 */
int uxml_encode64( char *dst, const int n_dst, const void *src, const int n_src );

/*! Decode from base64 sequence
 *
 * Decode base64 sequence into binary data.
 * \param dst - pointer to destination binary data buffer;
 * \param n_dst - maximum possible size of destination buffer;
 * \param src - pointer to original base64 character sequence;
 * \param n_src - size of original base64 character sequence.
 * \return size of decoded binary data in bytes, stored in \c dst buffer;
 */
int uxml_decode64( void *dst, const int n_dst, const char *src, const int n_src );

#ifdef __cplusplus
}
#endif

#endif
