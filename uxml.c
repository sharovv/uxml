#include <uxml.h>
#include <stdlib.h>
#include <string.h>

enum { NONE,
       NODE_NAME, NODE_TAG, NODE_CONTENT_TRIM_0, NODE_CONTENT_0,
       NODE_ATTR_NAME, NODE_ATTR_EQ, NODE_ATTR_EQ_FOUND, NODE_ATTR_VALUE_DQ_0, NODE_ATTR_VALUE_SQ_0,
       NODE_END,
       INST_NAME, INST_TAG,
       INST_ATTR_NAME, INST_ATTR_EQ, INST_ATTR_EQ_FOUND, INST_ATTR_VALUE_DQ_0, INST_ATTR_VALUE_SQ_0,
       COMMENT };

#define ENABLE_ESCAPE 0x80
#define NODE_CONTENT       (NODE_CONTENT_0 | ENABLE_ESCAPE)
#define NODE_CONTENT_TRIM  (NODE_CONTENT_TRIM_0 | ENABLE_ESCAPE)
#define NODE_ATTR_VALUE_DQ (NODE_ATTR_VALUE_DQ_0 | ENABLE_ESCAPE)
#define NODE_ATTR_VALUE_SQ (NODE_ATTR_VALUE_SQ_0 | ENABLE_ESCAPE)
#define INST_ATTR_VALUE_DQ (INST_ATTR_VALUE_DQ_0 | ENABLE_ESCAPE)
#define INST_ATTR_VALUE_SQ (INST_ATTR_VALUE_SQ_0 | ENABLE_ESCAPE)

/* UXML parser state machine

+- NONE|NODE_CONTENT
|
| +- NODE_NAME if( isalpha( c[0] ) && c[-1]=='<' )
| |
| |    +- NODE_TAG if( state==NODE_NAME && isspace( c[0] ) )
| |    |
| |    |+- ATTR_NAME if( state==NODE_TAG && isalpha( c[0] ) )
| |    ||
| |    ||    +- ATTR_EQ if( state==ATTR_NAME && c[0]=='=' )
| |    ||    |
| |    ||    |+- ATTR_VALUE if( state==ATTR_EQ && c[0]== '\"' )
| |    ||    ||
| |    ||    ||    +- NODE_TAG if( state==ATTR_VALUE && c[0]=='\"' )
| |    ||    ||    |
| |    ||    ||    | +- NODE_CONTENT_TRIM if( state==NODE_TAG && c[0]=='>' )
| |    ||    ||    | |
| |    ||    ||    | | +- NODE_CONTENT if( state==NODE_CONTENT_TRIM && !isspace( c[0] ) )
| |    ||    ||    | | |
| |    ||    ||    | | |            +------ NODE_END if( state==NODE_CONTENT && c[-2]=='<' && c[-1]=='/' )
| |    ||    ||    | | |            |
| |    ||    ||    | | |            |    +- NONE|NODE_CONTENT if( state==NODE_END && c[0]=='>' )
| |    ||    ||    | | |            |    |
 <node1 attr1="val1" > content &lt; </node>

                                   +- NONE if( state==NODE_TAG && c[-1]=='/' && c[0]=='>' )
                                   |
 <node1 attr1="val1" attr2="val2" />


+- NONE

  +- INST_NAME
  |
  |   +- INST_TAG
  |   |
  |   |+- INST_ATTR_NAME
  |   ||
  |   ||    +- INST_ATTR_EQ
  |   ||    |
  |   ||    |+- INST_ATTR_VALUE
  |   ||    ||
  |   ||    ||    +- INST_TAG
  |   ||    ||    |
  |   ||    ||    |  +- NONE
  |   ||    ||    |  |
 <?xml attr1="val1" ?>

+- NONE|NODE_CONTENT|NODE_TAG

    +- COMMENT if( (state==NONE||state==NODE_CONTENT||state==NODE_TAG) && c[-3..0]=="<!--" )

    |                   +- NONE|NODE_CONTENT|NODE_TAG if( state==COMMENT && c[-2..0]=="-->" )
    |                   |
 <!-- comment comment -->

*/

/* entity type - node, attribute or process instruction */
enum { XML_NONE, XML_NODE, XML_ATTR, XML_INST };

typedef struct _uxml_t
{
  const unsigned char *xml;         /* original XML data */
  int xml_index;                    /* current character when parse */
  int xml_size;                     /* size of original XML data */
  unsigned char *text;              /* text data, extracted from original XML */
  int text_index;                   /* current write index */
  int text_size;        /* text data size, in bytes */
  uxml_node_t *node;                /* array of nodes, first element - emtpy, second element - root node */
  int node_index;                   /* current node while parse */
  int nodes_count;      /* count of nodes */
  int state;                        /* current state */
  unsigned int c;                   /* queue of last 4 characters, least byte means last character */
  unsigned int escape;              /* escape flags for last 4 characters, least bit is corresponded to last character */
  int line;                         /* current line, freeze at error position */
  int column;                       /* current column, freeze at error position */
  const char *error;                /* error's text */
  int initial_allocated;
} uxml_t;

struct _uxml_node_t
{
  int type;               /* element's type - XML_NODE, XML_ATTR, XML_INST */
  unsigned char *name;    /* element's name */
  unsigned char *content; /* element's content / attribute value */
  int size;               /* size of element's content */
  int name_length;        /* length of name */
  uxml_t *instance;       /* UXML instance */
  uxml_node_t *parent;    /* index of parent element */
  uxml_node_t *child;     /* index of first child element (for XML_NODE only), 0 means no child */
  uxml_node_t *next;      /* index of next element (not for XML_INST), 0 means last element */
  void *user;             /* user pointer */
};

/*
#define isdigit( c ) (c>='0'&&c<='9')
#define isalpha( c ) ((c>='A'&&c<='Z')||(c>='a'&&c<='z'))
#define isspace( c ) (c==' '||c=='\n'||c=='\t'||c=='\r')
*/

/*
static const char uxml_isdigit_tab[256]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static const char uxml_isalpha_tab[256]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static const char uxml_isspace_tab[256]={0,0,0,0,0,0,0,0,0,1,1,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

#define isdigit( c ) uxml_isdigit_tab[ c ]
#define isalpha( c ) uxml_isalpha_tab[ c ]
#define isspace( c ) uxml_isspace_tab[ c ]
*/

static const unsigned int uxml_isdigit_tab32[8]={0x00000000,0x03FF0000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000};
static const unsigned int uxml_isalpha_tab32[8]={0x00000000,0x00000000,0x87FFFFFE,0x07FFFFFE,0x00000000,0x00000000,0x00000000,0x00000000};
static const unsigned int uxml_isspace_tab32[8]={0x00002600,0x00000001,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000};

#define isdigit( c ) ((uxml_isdigit_tab32[ c >> 5 ] >> (c & 0x1F))&1)
#define isalpha( c ) ((uxml_isalpha_tab32[ c >> 5 ] >> (c & 0x1F))&1)
#define isspace( c ) ((uxml_isspace_tab32[ c >> 5 ] >> (c & 0x1F))&1)

/*
 * Get character, dispatch escape sequences in contents and attributes
 */
static int uxml_get_escape( uxml_t *p )
{
  /* escape sequences in attribute value or the content */
  int i, t, code = 0, dec = 0, hex = 0, v = 0;

  for( i = p->xml_index; p->xml_index != p->xml_size; )
  {
    t = p->xml[ p->xml_index++ ];
    p->column++;

    if( t == ';' )
    {
      switch( p->xml_index - i )
      {
      case 3:
        if( p->xml[i] == 'l' && p->xml[i+1] == 't' )
        {
          return '<';
        }
        else if( p->xml[i] == 'g' && p->xml[i+1] == 't' )
        {
          return '>';
        }
        break;
      case 4:
        if( p->xml[i] == 'a' && p->xml[i+1] == 'm' && p->xml[i+2] == 'p' )
        {
          return '&';
        }
        break;
      case 5:
        if( p->xml[i] == 'a' && p->xml[i+1] == 'p' && p->xml[i+2] == 'o' && p->xml[i+3] == 's' )
        {
          return '\'';
        }
        else if( p->xml[i] == 'q' && p->xml[i+1] == 'u' && p->xml[i+2] == 'o' && p->xml[i+3] == 't' )
        {
          return '\"';
        }
        break;
      default:
        break;
      }
      if( dec || hex )
      {
        return v;
      }
      else
      {
        p->error = "Error in escape sequence";
        return 0;
      }
    }
    else if( t == '#' && (p->xml_index - i) == 1 )
    {
      code = 1;
    }
    else if( code )
    {
      if( (p->xml_index - i) == 2 )
      {
        if( t == 'x' )
        {
          hex = 1;
        }
        else if( isdigit( t ) )
        {
          dec = 1;
          v = t - '0';
        }
        else
        {
          p->error = "Only decimal or hexdecimal escape allowed";
          return 0;
        }
      }
      else
      {
        if( dec )
        {
          if( isdigit( t ) )
          {
            v = v * 10 + (t - '0');
          }
          else
          {
            p->error = "Error in decimal escape";
            return 0;
          }
        }
        else if( hex )
        {
          if( isdigit( t ) )
          {
            v = (v << 4) + (t - '0');
          }
          else if( t >= 'A' && t <= 'F' )
          {
            v = (v << 4) + (t - 'A') + 10;
          }
          else if( t >= 'a' && t <= 'f' )
          {
            v = (v << 4) + (t - 'a') + 10;
          }
          else
          {
            p->error = "Error in hexdecimal escape";
            return 0;
          }
        }
      }
    }
  }
  p->error = "Unterminated escape";
  return 0;
}

/*
 * parse process instruction
 */
static int uxml_parse_inst( uxml_t *p )
{
  int c0;
  int state = p->state;                /* keep current state */
  int node_index = p->node_index;      /* index of process instruction node */
  uxml_node_t *n = NULL;               /* node for process instruction */
  uxml_node_t *a = NULL;               /* node for attribute reading */

  if( p->node != NULL )                /* real parsing? */
  {
    n = p->node + p->node_index;       /* use current node */

    n->type = XML_INST;                /* type of node */
    n->name = p->text + p->text_index; /* name of instruction */
    n->name_length = 0;
    n->content = p->text;              /* there is no content, tag only */
    n->size = 0;                       /* size of content is 0 */
    n->instance = p;                   /* our instance */
    n->parent = NULL;                  /* no parent */
    n->child = NULL;                   /* no child node(s) */
  }
  p->node_index++;                     /* next node */

  p->state = INST_NAME;                /* new state - read instruction name */
  while( p->xml_index != p->xml_size ) /* can read new character? */
  {
    c0 = p->xml[ p->xml_index++ ];     /* get new character */
    p->column++;
    p->c <<= 8;
    p->escape <<= 1;
    switch( c0 )
    {
    case '&':                    /* is there escape? */
      if( (p->state & ENABLE_ESCAPE) != 0 ) /* escape sequences only in attribute value or the content */
      {
        if( (c0 = uxml_get_escape( p )) == 0 )
          return 0;
        p->escape |= 1;
      }
      break;
    case '\n': p->column = 0; p->line++; break;
    case '\r': p->column = 0; break;
    default: break;
    }
    p->c |= c0;
    if( p->state == INST_NAME )        /* is name reading ? */
    {
      if( !isspace( c0 ) )           /* non-space character? that is name */
      {
        if( p->text != NULL )          /* if text buffer present, */
        {
          p->text[ p->text_index ] = c0; /* store current character of name */
          n->name_length++;
        }
        p->text_index++;               /* go to next character */
      }
      else                             /* name over */
      {
        p->text_index++;               /* end name with zero-byte */
        p->state = INST_TAG;           /* go to read whole tag */
      }
    }
    else if( p->state == INST_TAG )
    {
      if( isalpha( c0 ) )            /* attribute begin with alphabet character? */
      {
        p->state = INST_ATTR_NAME;     /* go to new state */
        if( n != NULL )                /* while real parsing */
        {
          if( n->child == NULL )       /* and no one attribute was parsed yet */
          {
            n->child = p->node + p->node_index;  /* store attribute index */
          }
        }
        if( a != NULL )                /* if it is not first attribute */
        {
          a->next = p->node + p->node_index; /* fill next of previous attribute */
        }
        if( p->node != NULL )          /* real dispatch? */
        {
          a = p->node + p->node_index; /* current node for attribute */
          a->type = XML_ATTR;          /* this is attribute node */
          a->name = p->text + p->text_index; /* name of attribute */
          a->name_length = 0;
          a->content = p->text;        /* add content later */
          a->size = 0;                 /* size of content is 0*/
          a->instance = p;             /* our instance */
          a->parent = p->node + node_index; /* parent process instruction */
          a->child = NULL;                /* attribute have no child nodes */
          a->next = NULL;                 /* no next node (yet) */
        }
        p->node_index++;
        if( p->text != NULL )          /* if text buffer present, */
        {
          p->text[ p->text_index ] = c0; /* store first character of attribute's name */
          a->name_length++;
        }
        p->text_index++;               /* prepare for next character */
      }
      else if( (p->c & 0x0000FFFFU) == (('?' << 8) | '>') )
      {
        p->state = state;              /* restore outer state */
        return node_index;             /* dispatch done */
      }
      else if( !(c0 == '?') )
      {
        if( !isspace( c0 ) )
        {
          p->error = "Invalid character";
          return 0;
        }
      }
    }
    else if( p->state == INST_ATTR_NAME )
    {
      if( isspace( c0 ) )           /* attribute's name end with '=' */
      {
        p->state = INST_ATTR_EQ;       /* new state */
        p->text_index++;               /* end name with zero byte */
      }
      else if( c0 == '=' )           /* attribute's name end with '=' */
      {
        p->state = INST_ATTR_EQ_FOUND; /* new state */
        p->text_index++;               /* end name with zero byte */
      }
      else                             /* all other characters means error */
      {
        if( p->text != NULL )          /* if text buffer present, */
        {
          p->text[ p->text_index ] = c0; /* store next character of attribute's name */
          a->name_length++;
        }
        p->text_index++;               /* go to next character */
      }
    }
    else if( p->state == INST_ATTR_EQ )
    {
      if( c0 == '=' )
      {
        p->state = INST_ATTR_EQ_FOUND;
      }
      else if( !isspace( c0 ) )
      {
        p->error = "Extra character after attribute's name";
        return 0;
      }
    }
    else if( p->state == INST_ATTR_EQ_FOUND )
    {
      if( c0 == '\"' )               /* start attribute's value reading "value" */
      {
        p->state = INST_ATTR_VALUE_DQ; /* double quoted value */
        if( a != NULL )                /* of course, do not forget real parsing */
        {
          a->content = p->text + p->text_index; /* content points to attribute's value */
        }
      }
      else if( c0 == '\'' )          /* start attribute's value reading 'value' */
      {
        p->state = INST_ATTR_VALUE_SQ; /* single quoted value */
        if( a != NULL )                /* of course, do not forget real parsing */
        {
          a->content = p->text + p->text_index;  /* content points to attribute's value */
        }
      }
      else if( !isspace( c0 ) )      /* error in other non-space character */
      {
        p->error = "Attribute value must begin with '\"' or '\''";
        return 0;
      }
    }
    else if( p->state == INST_ATTR_VALUE_DQ )
    {
      if( c0 == '\"' && ((p->escape & 1) == 0) ) /* value ended? */
      {
        p->text_index++;               /* end value with zero byte */
        p->state = INST_TAG;           /* return to tag dispatch */
      }
      else
      {
        if( p->text != NULL )          /* while real parsing */
        {
          p->text[ p->text_index ] = c0; /* store next value character */
        }
        p->text_index++;               /* next byte for value */
        if( a != NULL )
        {
          a->size++;
        }
      }
    }
    else if( p->state == INST_ATTR_VALUE_SQ )
    {
      if( c0 == '\'' && ((p->escape & 1) == 0) ) /* value ended? */
      {
        p->text_index++;               /* end value with zero byte */
        p->state = INST_TAG;           /* return to tag dispatch */
      }
      else
      {
        if( p->text != NULL )          /* while real parsing */
        {
          p->text[ p->text_index ] = c0; /* store next value character */
        }
        p->text_index++;               /* next byte for value */
        if( a != NULL )
        {
          a->size++;
        }
      }
    }
  }
  if( p->error == NULL )
    p->error = "Unterminated process instruction";
  return 0;
}

/*
 * parse node
 */
static int uxml_parse_node( uxml_t *p, uxml_node_t *parent_node )
{
  int c0;
  int state = p->state;                /* keep current state */
  int node_index = p->node_index;      /* index of node */
  uxml_node_t *n = NULL;               /* node itself */
  uxml_node_t *a = NULL;               /* node for attribute reading */
  int content_begin = 0;               /* content begin */
  int content_end = 0;                 /* and content end, of course */
  int name = p->xml_index - 1;         /* node's name - on step before */
  int name_end = 0;                    /* node's end name - not found yet */
  int name_len = 1;                    /* node's name length - 1 character at least */
  int comment_state = p->state;        /* place for state when comment dispatch */
  int last_child = 0;                  /* no children nodes yet */

  if( p->node != NULL )                /* real parsing? */
  {
    n = p->node + p->node_index;       /* use current node */

    n->type = XML_NODE;                /* type of node */
    n->name = p->text + p->text_index; /* name of node */
    n->name_length = 0;
    n->content = p->text;              /* there is no content yet */
    n->size = 0;
    n->instance = p;                   /* our instance */
    n->parent = parent_node;           /* no parent */
    n->child = NULL;                   /* no child node(s) yet */
  }
  p->node_index++;                     /* next node */

  if( p->text != NULL )                /* if text buffer present, */
  {
    p->text[ p->text_index ] = p->xml[ p->xml_index - 1 ];   /* store first character of name */
    n->name_length++;
  }
  p->text_index++;                     /* go to next character */

  p->state = NODE_NAME;                /* new state - read instruction name */
  while( p->xml_index != p->xml_size ) /* can read new character? */
  {
    c0 = p->xml[ p->xml_index++ ];     /* get new character */
    p->column++; 
    p->c <<= 8;
    p->escape <<= 1;
    switch( c0 )
    {
    case '&':                    /* is there escape? */
      if( (p->state & ENABLE_ESCAPE) != 0 ) /* escape sequences only in attribute value or the content */
      {
        if( (c0 = uxml_get_escape( p )) == 0 )
          return 0;
        p->escape |= 1;
      }
      break;
    case '\n': p->column = 0; p->line++; break;
    case '\r': p->column = 0; break;
    default: break;
    }
    p->c |= c0;
    if( p->state == NODE_NAME )        /* is name reading ? */
    {
      if( (p->c & 0x0000FFFFU) == (('/' << 8) | '>') )
      {
        p->text_index++;               /* end name with zero-byte */
        p->state = state;              /* restore outer state */
        if( p->node != NULL )
        {
          if( parent_node != NULL )
          {
            if( parent_node->child == NULL )
            {
              parent_node->child = p->node + node_index;
            }
          }
        }
        return node_index;             /* dispatch done */
      }
      else if( c0 == '/' )
      {
        continue;
      }
      else if( c0 == '>' )
      {
        p->text_index++;               /* terminate name with zero byte */
        p->state = NODE_CONTENT_TRIM;  /* start content dispatch */
      }
      else if( !isspace( c0 ) )           /* non-space character? that is name */
      {
        if( p->text != NULL )          /* if text buffer present, */
        {
          p->text[ p->text_index ] = c0; /* store current character of name */
          n->name_length++;
        }
        p->text_index++;               /* go to next character */
        name_len++;                    /* length of node's name */
      }
      else                             /* name over */
      {
        p->text_index++;               /* end name with zero-byte */
        p->state = NODE_TAG;           /* go to read whole tag */
      }
    }
    else if( p->state == NODE_TAG )
    {
      if( isalpha( c0 ) )            /* attribute begin with alphabet character? */
      {
        p->state = NODE_ATTR_NAME;     /* go to new state */
        if( n != NULL )                /* while real parsing */
        {
          if( n->child == NULL )          /* and no one attribute was parsed yet */
          {
            n->child = p->node + p->node_index;  /* store attribute index */
          }
        }
        if( a != NULL )                /* if it is not first attribute */
        {
          a->next = p->node + p->node_index; /* fill next of previous attribute */
        }
        if( p->node != NULL )          /* real dispatch? */
        {
          a = p->node + p->node_index; /* current node for attribute */
          a->type = XML_ATTR;          /* this is attribute node */
          a->name = p->text + p->text_index; /* name of attribute */
          a->name_length = 0;
          a->content = p->text;              /* add content later */
          a->size = 0;
          a->instance = p;             /* our instance */
          a->parent = p->node + node_index; /* parent process instruction */
          a->child = NULL;                /* attribute have no chid nodes */
          a->next = NULL;                 /* no next node (yet) */
        }
        last_child = p->node_index;    /* new last child */
        p->node_index++;
        if( p->text != NULL )          /* if text buffer present, */
        {
          p->text[ p->text_index ] = c0; /* store first character of attribute's name */
          a->name_length++;
        }
        p->text_index++;               /* prepare for next character */
      }
      else if( (p->c & 0x0000FFFFU) == (('/' << 8) | '>') )
      {
        p->state = state;              /* restore outer state */
        if( p->node != NULL )
        {
          if( parent_node != NULL )
          {
            if( parent_node->child == NULL )
            {
              parent_node->child = p->node + node_index;
            }
          }
        }
        return node_index;             /* dispatch done */
      }
      else if( c0 == '>' )           /* node tag over */
      {
        p->state = NODE_CONTENT_TRIM;  /* start content dispatch */
      }
      else if( !isspace( c0 ) && c0 != '/' )      /* all other non-spaced symbols (digits, specials) */
      {
        p->error = "Invalid character"; /* means error */
        return 0;
      }
    }
    else if( p->state == NODE_ATTR_NAME )
    {
      if( isspace( c0 ) )
      {
        p->state = NODE_ATTR_EQ;       /* new state */
        p->text_index++;               /* end name with zero byte */
      }
      else if( c0 == '=' )
      {
        p->state = NODE_ATTR_EQ_FOUND; /* new state */
        p->text_index++;               /* end name with zero byte */
      }
      else
      {
        if( p->text != NULL )          /* if text buffer present, */
        {
          p->text[ p->text_index ] = c0; /* store next character of attribute's name */
          a->name_length++;
        }
        p->text_index++;               /* go to next character */
      }
    }
    else if( p->state == NODE_ATTR_EQ )
    {
      if( c0 == '=' )
      {
        p->state = NODE_ATTR_EQ_FOUND;
      }
      else if( !isspace( c0 ) )
      {
        p->error = "Extra character after attribute's name";
        return 0;
      }
    }
    else if( p->state == NODE_ATTR_EQ_FOUND )
    {
      if( c0 == '\"' )               /* start attribute's value reading "value" */
      {
        p->state = NODE_ATTR_VALUE_DQ; /* double quoted value */
        if( a != NULL )                /* of course, do not forget real parsing */
        {
          a->content = p->text + p->text_index;  /* content points to attribute's value */
        }
      }
      else if( c0 == '\'' )          /* start attribute's value reading 'value' */
      {
        p->state = NODE_ATTR_VALUE_SQ; /* single quoted value */
        if( a != NULL )                /* of course, do not forget real parsing */
        {
          a->content = p->text + p->text_index;  /* content points to attribute's value */
        }
      }
      else if( !isspace( c0 ) )      /* error in other non-space character */
      {
        p->error = "Attribute value must begin with '\"' or '\''";
        return 0;
      }
    }
    else if( p->state == NODE_ATTR_VALUE_DQ )
    {
      if( c0 == '\"' && ((p->escape & 1) == 0) ) /* value ended? */
      {
        p->text_index++;               /* end value with zero byte */
        p->state = NODE_TAG;           /* return to tag dispatch */
      }
      else
      {
        if( p->text != NULL )          /* while real parsing */
        {
          p->text[ p->text_index ] = c0; /* store next value character */
        }
        p->text_index++;               /* next byte for value */
        if( a != NULL )
        {
          a->size++;
        }
      }
    }
    else if( p->state == NODE_ATTR_VALUE_SQ )
    {
      if( c0 == '\'' && ((p->escape & 1) == 0) ) /* value ended? */
      {
        p->text_index++;               /* end value with zero byte */
        p->state = NODE_TAG;           /* return to tag dispatch */
      }
      else
      {
        if( p->text != NULL )          /* while real parsing */
        {
          p->text[ p->text_index ] = c0; /* store next value character */
        }
        p->text_index++;               /* next byte for value */
        if( a != NULL )
        {
          a->size++;
        }
      }
    }
    else if( p->state == NODE_CONTENT_TRIM || p->state == NODE_CONTENT )
    {
      if( ( (p->c & 0x0000FFFFU) == (('<' << 8) | '!') && ((p->escape & 3) != 3) ) ||
          ( (p->c & 0x00FFFFFFU) == (('<' << 16) | ('!' << 8) | '-') && ((p->escape & 7) != 7) ) )
      {
        continue;
      }
      if( ((p->c & 0x0000FF00U) == ('<' << 8)) && ((p->escape & 2) == 0) )
      {
        if( isalpha( c0 ) )          /* open new node? */
        {
          int k = content_end - content_begin;
          int i;

          i = uxml_parse_node( p, p->node + node_index ); /* parse it */
          if( !i )
            return 0;

          if( p->node != NULL )         /* at real parsing stage */
          {
            if( last_child != 0 )
            {
              p->node[ last_child ].next = p->node + i; /* set-up next field of last child node */
            }
          }
          last_child = i;               /* new last child */

          if( k != 0 )                 /* if non-empty content */
          {
            if( p->text != NULL )      /* at real parsing */
            {
              for( i = 0; i != k; i++ )  /* copy content to new location */
              {
                p->text[ p->text_index + i ] = p->text[ content_begin + i ];
              }
            }
            content_begin = p->text_index; /* new content's begin location */
            p->text_index += k;            /* go to the next character */
            content_end = p->text_index;   /* new content's end location */
            if( n != NULL )
            {
              n->content = p->text + content_begin;  /* for real node, save new content's location too */
            }
          }
        }
        else if( c0 == '/' && ((p->escape & 1) == 0) )
        {
          p->text_index++;             /* end content with zero byte */
          p->state = NODE_END;
        }
        else
        {
          p->error = "Invalid character";
          return 0;
        }
      }
      else if( p->c == (('<' << 24) | ('!' << 16) | ('-' << 8) | '-') && ((p->escape & 0xFU) != 0xFU) )
      {
        comment_state = p->state;      /* keep state before comment occured */
        p->state = COMMENT;            /* comment in */
      }
      else if( c0 != '<' || (p->escape & 1) )           /* regular symbol of content */
      {
        if( p->state == NODE_CONTENT_TRIM )
        {
          if( !isspace( c0 ) )       /* non-space character? */
          {
            p->state = NODE_CONTENT;
            if( content_begin != 0 )
            {
              if( p->text != NULL )    /* store it, if needed */
              {
                p->text[ p->text_index ] = ' '; /* store one space instead several */
              }
              p->text_index++;
              if( n != NULL )
              {
                n->size++;
              }
            }
            else
            {
              content_begin = p->text_index; /* the content begin. */
            }
            if( p->text != NULL )      /* store it, if needed */
            {
              p->text[ p->text_index ] = c0;
            }
            p->text_index++;               /* next character */
            if( n != NULL )
            {
              n->size++;
            }
            content_end = p->text_index;   /* and content's end */
          }
        }
        else                           /* need to read content */
        {
          if( isspace( c0 ) && ((p->escape & 1) == 0) )        /* all empty non-escaped characters will replaced with one space */
          {
            p->state = NODE_CONTENT_TRIM;
          }
          else
          {
            if( p->text != NULL )      /* store character, if needed */
            {
              p->text[ p->text_index ] = c0;
            }
            p->text_index++;             /* next character */
            if( n != NULL )
            {
              n->size++;
            }
            content_end = p->text_index; /* and content's end */
          }
        }
      }
    }
    else if( p->state == COMMENT )     /* is there comments inside? */
    {
      if( (p->c & 0x00FFFFFFU) == (('-' << 16) | ('-' << 8) | '>') ) /* comment over? */
      {
        p->state = comment_state;      /* restore state */
      }
    }
    else if( p->state == NODE_END )    /* node end tag */
    {
      if( name_end == 0 )              /* entry to end? */
      {
        name_end = p->xml_index - 1;   /* keep end-name */
      }
      if( c0 == '>' )                /* node-end tag over? */
      {
        int i;

        if( (p->xml_index - name_end - 1) != name_len )
        {
          p->error = "Different length of node's name";
          return 0;
        }
        for( i = 0; i != name_len; i++ )
        {
          if( p->xml[ name + i ] != p->xml[ name_end + i ] )
          {
            p->error = "Different name at end of node";
            return 0;
          }
        }
        if( p->node != NULL )
        {
          p->node[ node_index ].content = p->text + content_begin;
          if( parent_node != 0 )
          {
            if( parent_node->child == 0 ) /* if this is first child */
            {
              parent_node->child = p->node + node_index; /* parent will point to it */
            }
          }
        }
        p->state = state;              /* restore outer state */
        return node_index;             /* dispatch done */
      }
    }
  }
  if( p->error == NULL )
    p->error = "Unterminated node";
  return 0;
}

static int uxml_parse_doc( uxml_t *p )
{
  int root = 0;
  int c0;

  while( p->xml_index != p->xml_size ) /* can read new character? */
  {
    c0 = p->xml[ p->xml_index++ ];     /* get new character */
    p->column++; 
    p->c <<= 8;
    p->escape <<= 1;
    switch( c0 )
    {
    case '&':                    /* is there escape? */
      if( (p->state & ENABLE_ESCAPE) != 0 ) /* escape sequences only in attribute value or the content */
      {
        if( (c0 = uxml_get_escape( p )) == 0 )
          return 0;
        p->escape |= 1;
      }
      break;
    case '\n': p->column = 0; p->line++; break;
    case '\r': p->column = 0; break;
    default: break;
    }
    p->c |= c0;
    if( p->state == NONE )
    {
      if( (p->c & 0x0000FF00U) == ('<' << 8) && isalpha( c0 ) )
      {
        if( root == 0 )
        {
          root = p->node_index;
          if( !uxml_parse_node( p, 0 ) )
            return 0;
        }
        else
        {
          p->error = "Multiple root node";
          return 0;
        }
      }
      else if( (p->c & 0x0000FFFFU) == (('<' << 8) | '?') )
      {
        if( !uxml_parse_inst( p ) )
          return 0;
      }
      else if( p->c == (('<' << 24) | ('!' << 16) | ('-' << 8) | '-') )
      {
        p->state = COMMENT;
      }
      else if( !(( (p->c & 0x00FFFFFFU) == (('<' << 16) | ('!' << 8) | '-') ) ||
                 ( (p->c & 0x0000FFFFU) == (('<' << 8) | '!') ) ||
                 ( c0 == '<' )) )
      {
        if( !isspace( c0 ) )
        {
          p->error = "Unrelated character";
          return 0;
        }
      }
    }
    else if( p->state == COMMENT )
    {
      if( (p->c & 0x00FFFFFFU) == (('-' << 16) | ('-' << 8) | '>') )
      {
        p->state = NONE;
      }
    }
  }
  if( root == 0 )
  {
    if( p->error == NULL )
    {
      p->error = "No root node";
    }
  }
  return root;
}

uxml_node_t *uxml_parse( const char *xml_data, const int xml_length0, uxml_error_t *error )
{
  uxml_t instance, *p = &instance;
  void *v;
  char *c;
  int i, xml_length;

  for( xml_length = xml_length0; xml_length != 0 && xml_data[ xml_length - 1 ] == 0; xml_length-- );

  p->xml = (const unsigned char *)xml_data;
  p->xml_index = 0;
  p->xml_size = xml_length;
  p->text = NULL;
  p->text_index = 0;
  p->node = NULL;
  p->node_index = 1;
  p->state = NONE;
  p->c = 0;
  p->escape = 0;
  p->line = 1;
  p->column = 0;
  p->error = NULL;

  if( p->xml_size >= 3 )               /* if we have 3 bytes at least, */
  {                                    /* check for UTF-8 byte order mark */
    if( p->xml[0] == 0xEF && p->xml[1] == 0xBB && p->xml[2] == 0xBF )
    {
      p->xml += 3;                     /* simple skip BOM */
      p->xml_size -= 3;
    }
  }
  if( !uxml_parse_doc( p ) )
  {
    if( error != NULL )
    {
      error->text = p->error;
      error->line = p->line;
      error->column = p->column;
    }
    return NULL;
  }
  i = sizeof( uxml_t ) + 1 + p->text_index + 1 + p->node_index * sizeof( uxml_node_t );

  if( (v = malloc( i )) == NULL )
  {
    error->text = "Insufficient memory";
    return 0;
  }
  memset( v, 0, i );
  p->initial_allocated = i;

  p = (uxml_t *)v;
  c = (char *)v;
  memcpy( p, &instance, sizeof( uxml_t ) );
  c += sizeof( uxml_t );
  p->node = (uxml_node_t *)c;
  c += (sizeof( uxml_node_t ) * p->node_index);
  p->text = (unsigned char *)c;

  p->xml = (const unsigned char *)xml_data;
  p->xml_index = 0;
  p->xml_size = xml_length;
  p->text_size = p->text_index + 1;
  p->text_index = 1;
  p->nodes_count = p->node_index;
  p->node_index = 1;
  p->state = NONE;
  p->c = 0;
  p->line = 1;
  p->column = 0;
  p->error = NULL;

  if( p->xml_size >= 3 )               /* if we have 3 bytes at least */
  {                                    /* check for UTF-8 byte order mark */
    if( p->xml[0] == 0xEF && p->xml[1] == 0xBB && p->xml[2] == 0xBF )
    {
      p->xml += 3;                     /* simple skip BOM */
      p->xml_size -= 3;
    }
  }
  if( (i = uxml_parse_doc( p )) == 0 )
  {
    if( error != NULL )
    {
      error->text = p->error;
      error->line = p->line;
      error->column = p->column;
    }
    return NULL;
  }
  p->node[0].next = p->node + i;
  return p->node + i;
}

void uxml_free( uxml_node_t *node )
{
  free( node->instance );
}

uxml_node_t *uxml_child( uxml_node_t *node )
{
  return node->child;
}

uxml_node_t *uxml_child_node( uxml_node_t *node )
{
  uxml_node_t *n = node->child;

  while( n != NULL )
  {
    if( n->type == XML_NODE )
      break;
    n = n->next;
  }
  return n;
}

uxml_node_t *uxml_first_attr( uxml_node_t *node )
{
  uxml_node_t *n = node->child;

  while( n != NULL )
  {
    if( n->type == XML_ATTR )
      break;
    n = n->next;
  }
  return n;
}

uxml_node_t *uxml_next_attr( uxml_node_t *node )
{
  uxml_node_t *n = node->next;

  while( n != NULL )
  {
    if( n->type == XML_ATTR )
      break;
    n = n->next;
  }
  return n;
}

uxml_node_t *uxml_next( uxml_node_t *node )
{
  return node->next;
}

uxml_node_t *uxml_prev( uxml_node_t *node )
{
  uxml_node_t *prev = NULL, *n = node->parent;

  if( n == NULL ) return NULL;
  for( n = n->child; n != node; n = n->next )
  {
    prev = n;
  }
  return prev;
}

const char *uxml_name( uxml_node_t *node )
{
  return (const char *)node->name;
}

#define MASK_SQUARE_OPEN  1
#define MASK_SQUARE_CLOSE 2
#define MASK_INDEX        4
#define MASK_WILDCARD     8

uxml_node_t *uxml_node( uxml_node_t *node, const char *ipath )
{
  uxml_t *p = node->instance;
  const char *path = ipath, *s1, *s2;
  int c, i, len, mask = 0, index = 0;
  uxml_node_t *n = node;

  /* NULL path is equal to empty string */
  if( path == NULL )
  {
    path = "";
  }
  /* Go from root? */
  if( path[0] == '/' )
  {
    n = p->node[0].next;
    path++;
  }
  /* scan string */
  for( s1 = path, s2 = path, len = 0; *s2 != 0; s2++ )
  {
    /* current character */
    c = *s2;
    /* separator ? */
    if( c == '/' )
    {
      /* empty name? - skip it */
      if( s1 == s2 )
      {
        s1 = s2 + 1;
        len = 0;
        continue;
      }
      /* special case of ".." - parent node */
      if( len == 2 )
      {
        if( s1[0] == '.' && s1[1] == '.' )
        {
          if( n->parent == NULL )
          {
            return NULL;
          }
          n = n->parent;
          s1 = s2 + 1;
          len = 0;
          continue;
        }
      }
      /* mask of name */
      switch( mask )
      {
      case 0: /* regular case - name only */
        for( n = n->child; n != NULL; n = n->next )
        {
          /* look at only same length names */
          if( len == n->name_length )
          {
            /* is our name? */
            if( memcmp( s1, n->name, len ) == 0 )
            {
              /* go next */
              s1 = s2 + 1;
              len = 0;
              break;
            }
          }
        }
        break;
      case MASK_WILDCARD: /* wildcard "*" instead name */
        n = n->child; /* first child gettin' */
        break;
      case (MASK_SQUARE_OPEN | MASK_INDEX | MASK_SQUARE_CLOSE):
        /* index present, but no wildcard */
        for( i = 0, n = n->child; n != NULL; n = n->next )
        {
          /* only nodes is considered */
          if( n->type != XML_NODE )
            continue;
          /* look at only same length names */
          if( len == n->name_length )
          {
            /* is our name? */
            if( memcmp( s1, n->name, len ) == 0 )
            {
              /* and our index? */
              if( i == index )
              {
                /* go next */
                s1 = s2 + 1;
                len = 0;
                break;
              }
              /* next index */
              i++;
            }
          }
        }
        break;
      case (MASK_WILDCARD | MASK_SQUARE_OPEN | MASK_INDEX | MASK_SQUARE_CLOSE):
        /* wildcard and index, i.e. *[NN] */
        for( i = 0, n = n->child; n != NULL; n = n->next )
        {
          /* only nodes */
          if( n->type != XML_NODE )
            continue;
          /* only index compare */
          if( i == index )
          {
            /* go next */
            s1 = s2 + 1;
            len = 0;
            break;
          }
          /* next index */
          i++;
        }
        break;
      default:
        /* other cases is wrong */
        n = NULL;
        break; 
      }
      /* if NULL, nothing to do */
      if( n == NULL )
      {
        return NULL;
      }
      /* new name, clear mask */
      mask = 0;
    }
    else if( c == '*' && s1 == s2 )
    {
      /* wildcard is only one first character */
      mask |= MASK_WILDCARD;
    }
    else if( c == '[' && s1 != s2 )
    {
      /* square brackets only after non-zero name or wildcard */
      mask |= MASK_SQUARE_OPEN;
      index = 0;
    }
    else if( isdigit( c ) && (mask & MASK_SQUARE_OPEN) != 0 )
    {
      mask |= MASK_INDEX;
      index = index * 10 + (c - '0');
    }
    else if( c == ']' && (mask & (MASK_SQUARE_OPEN | MASK_INDEX)) == (MASK_SQUARE_OPEN | MASK_INDEX) )
    {
      mask |= MASK_SQUARE_CLOSE;
    }
    else if( mask != 0 )
    {
      /* all other cases means bad syntax in path */
      return NULL;
    }
    else
    {
      len++;
    }
  }
  if( s2 != s1 )
  {
    if( len == 2 )
    {
      if( s1[0] == '.' && s1[1] == '.' )
      {
        return n->parent;
      }
    }
    switch( mask )
    {
    case 0:
      for( n = n->child; n != NULL; n = n->next )
      {
        if( len == n->name_length )
        {
          if( memcmp( s1, n->name, len ) == 0 )
          {
            s1 = s2 + 1;
            break;
          }
        }
      }
      break;
    case MASK_WILDCARD:
      n = n->child;
      break;
    case (MASK_SQUARE_OPEN | MASK_INDEX | MASK_SQUARE_CLOSE):
      for( i = 0, n = n->child; n != NULL; n = n->next )
      {
        if( n->type != XML_NODE )
          continue;
        if( len == n->name_length )
        {
          if( memcmp( s1, n->name, len ) == 0 )
          {
            if( i == index )
            {
              s1 = s2 + 1;
              break;
            }
            i++;
          }
        }
      }
      break;
    case (MASK_WILDCARD | MASK_SQUARE_OPEN | MASK_INDEX | MASK_SQUARE_CLOSE):
      for( i = 0, n = n->child; n != NULL; n = n->next )
      {
        if( n->type != XML_NODE )
          continue;
        if( i == index )
        {
          s1 = s2 + 1;
          break;
        }
        i++;
      }
      break;
    default:
      n = NULL;
      break; 
    }
  }
  return n;
}

const char *uxml_get( uxml_node_t *node, const char *path )
{
  uxml_node_t *n = uxml_node( node, path );
  return n == NULL ? NULL: (const char *)n->content;
}

int uxml_int( uxml_node_t *node, const char *path )
{
  const char *s = uxml_get( node, path );
  return (s != NULL) ? (int)strtol( s, NULL, 0 ): 0;
}

#if !defined( UXML_DISABLE_DOUBLE )
double uxml_double( uxml_node_t *node, const char *path )
{
  const char *s = uxml_get( node, path );
  return (s != NULL) ? strtod( s, NULL ): 0.0;
}
#endif

void *uxml_user( uxml_node_t *node, const char *path )
{
  uxml_node_t *n = uxml_node( node, path );
  return (n != NULL) ? n->user: NULL;
}

void uxml_set_user( uxml_node_t *node, const char *path, void *user )
{
  uxml_node_t *n = uxml_node( node, path );
  if( n != NULL )
    n->user = user;
}

int uxml_size( uxml_node_t *node, const char *path )
{
  uxml_node_t *n = uxml_node( node, path );
  return (n == NULL) ? 0: n->size;
}

#include <stdio.h>

#if defined( _MSC_VER )
#pragma warning(disable:4996)
#endif

uxml_node_t *uxml_load( const char *xml_file, uxml_error_t *error )
{
  FILE *fp;
  void *b;
  int size, n;
  uxml_node_t *root;

  if( (fp = fopen( xml_file, "rb" )) == NULL )
  {
    error->text = "fopen failed"; error->line = error->column = 0;
    return NULL;
  }
  fseek( fp, 0, SEEK_END );
  n = ftell( fp );
  fseek( fp, 0, SEEK_SET );
  if( (b = malloc( n )) == NULL )
  {
    fclose( fp );
    error->text = "malloc failed"; error->line = error->column = 0;
    return NULL;
  }
  size = fread( b, 1, n, fp );
  fclose( fp );
  if( size != n )
  {
    free( b );
    error->text = "fread failed"; error->line = error->column = 0;
    return NULL;
  }
  root = uxml_parse( b, n, error );
  free( b );
  return root;
}

void uxml_dump_list( uxml_node_t *root )
{
  uxml_t *p = root->instance;
  int i;

  for( i = 0; i < p->nodes_count; i++ )
  {
    printf( "%d: %s name=\"%s\"(%d) content=\"%s\" size=%d parent=%d child=%d next=%d",
      i,
      p->node[i].type == XML_NODE ? "node": (p->node[i].type == XML_ATTR ? "attr": (p->node[i].type == XML_INST ? "inst": (p->node[i].type == XML_NONE ? "none": "????"))),
      p->node[i].name,
      p->node[i].name_length,
      p->node[i].content,
      p->node[i].size,
      p->node[i].parent == NULL ? 0: p->node[i].parent - p->node,
      p->node[i].child  == NULL ? 0: p->node[i].child - p->node,
      p->node[i].next == NULL ? 0: p->node[i].next - p->node );
    printf( "\n" );
  }
  printf( "Total nodes: %d, text size: %d\n", p->nodes_count, p->text_size );
}

int uxml_get_initial_allocated( uxml_node_t *root )
{
  uxml_t *p = root->instance;
  return p->initial_allocated;
}

/*       7   6   5   4   3   2   1   0
 *      -------------------------------
 * D0 = XXX XXX S07 S06 S05 S04 S03 S02
 * D1 = XXX XXX S01 S00 S17 S16 S15 S14
 * D2 = XXX XXX S13 S12 S11 S10 S27 S26
 * D3 = XXX XXX S25 S24 S23 S22 S21 S20
 */
static const char base64[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char base64_decode_tab[256]={-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,-1,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,-1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};

int uxml_encode64( char *dst, const int n_dst, const void *src, const int n_src )
{
  char *d;
  const unsigned char *s;
  int n, k;

  for( n = n_src, s = src, k = n_dst, d = dst; n >= 3 && k >= 4; n -= 3, s += 3, k -= 4, d += 4 )
  {
    d[0] = base64[ s[0] >> 2 ];
    d[1] = base64[ (((s[0] & 0x03) << 4) | ((s[1] & 0xf0) >> 4)) ];
    d[2] = base64[ (((s[1] & 0x0f) << 2) | ((s[2] & 0xc0) >> 6)) ];
    d[3] = base64[ s[2] & 0x3f ];
  }
  if( n == 0 )
  {
    if( k == 0 )
      return 0;
    d[0] = 0;
    return (d + 1 - dst);
  }
  if( k < 5 )
    return 0;
  d[0] = base64[ s[0] >> 2 ];
  d[1] = base64[ (((s[0] & 0x03) << 4) | (((n > 1 ? s[1]: 0) & 0xf0) >> 4)) ];
  d[2] = (n > 1 ? base64[ (((s[1] & 0x0f) << 2) | (((n > 2 ? s[2]: 0) & 0xc0) >> 6)) ] : '=');
  d[3] = (n > 2 ? base64[ s[2] & 0x3f ] : '=');
  d[4] = 0;
  return (d + 5 - dst);
}

int uxml_decode64( void *dst, const int n_dst, const char *src, const int n_src )
{
  unsigned char *d;
  const unsigned char *s;
  int i, n, k, v[4];

  for( i = 0, n = n_src, s = (const unsigned char *)src, k = n_dst, d = dst; n != 0; s++, n-- )
  {
    if( base64_decode_tab[ s[0] ] < 0 )
      continue;
    v[ i ] = base64_decode_tab[ s[0] ];
    i++;
    if( i >= 4 )
    {
      if( k < 3 )
        break;
      d[0] = ((v[0] << 2) | (v[1] >> 4));
      d[1] = ((v[1] << 4) | (v[2] >> 2));
      d[2] = (((v[2] << 6) & 0xC0) | v[3]);
      d += 3;
      k -= 3;
      i = 0;
    }
  }
  if( i >= 2 && k >= 1 )
  {
    *(d++) = ((v[0] << 2) | (v[1] >> 4));
    if( i >= 3 && k >= 2 )
    {
      *(d++) = ((v[1] << 4) | (v[2] >> 2));
      if( i >= 4 && k >= 3 )
      {
        *(d++) = (((v[2] << 6) & 0xC0) | v[3]);
      }
    }
  }
  return (d - (unsigned char *)dst);
}
