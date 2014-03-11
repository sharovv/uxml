#include <uxml.h>
#include <stdlib.h>
#include <string.h>

enum { NONE,
       NODE_NAME, NODE_TAG, NODE_CONTENT_TRIM, NODE_CONTENT,
       NODE_ATTR_NAME, NODE_ATTR_EQ, NODE_ATTR_EQ_FOUND, NODE_ATTR_VALUE_DQ, NODE_ATTR_VALUE_SQ,
       NODE_END,
       INST_NAME, INST_TAG,
       INST_ATTR_NAME, INST_ATTR_EQ, INST_ATTR_EQ_FOUND, INST_ATTR_VALUE_DQ, INST_ATTR_VALUE_SQ,
       COMMENT };

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

/* Maximum number of fragments, for text and nodes.
 * Because every fragment's size is great by 2 than previous at least,
 * the 32 fragments seems to be enough.
 */
#define MAX_FRAG 32

typedef struct _uxml_t
{
  const unsigned char *xml;         /* original XML data */
  int xml_index;                    /* current character when parse */
  int xml_size;                     /* size of original XML data */
  unsigned char *text;              /* text data, extracted from original XML */
  unsigned char *atext[ MAX_FRAG ]; /* text data, added later */
  int text_index;                   /* current write index */
  int text_size[ MAX_FRAG ];        /* text data size, in bytes */
  int text_allocated[ MAX_FRAG ] ;  /* allocated size for text data */
  int text_frag;                    /* current text fragment */
  uxml_node_t *node;                /* array of nodes, first element - emtpy, second element - root node */
  uxml_node_t *anode[ MAX_FRAG ];   /* array of nodes, added later */
  int node_index;                   /* current node while parse */
  int nodes_count[ MAX_FRAG ];      /* count of nodes */
  int nodes_allocated[ MAX_FRAG ];  /* allocated count for nodes */
  int node_frag;                    /* current node fragment */
  int state;                        /* current state */
  int c[4];                         /* queue of last 4 characters */
  int escape[4];                    /* escape flags for last 4 characters */
  int line;                         /* current line, freeze at error position */
  int column;                       /* current column, freeze at error position */
  const char *error;                /* error's text */
  unsigned char *dump;
  int dump_size;
  int dump_index;
  int initial_allocated;
} uxml_t;

struct _uxml_node_t
{
  int type;               /* element's type - XML_NODE, XML_ATTR, XML_INST */
  unsigned char *name;    /* element's name */
  unsigned char *content; /* element's content / attribute value */
  int size;               /* size of element's content */
  int fullsize;           /* full size of element's content */
  int name_length;        /* length of name */
  uxml_t *instance;       /* UXML instance */
  uxml_node_t *parent;    /* index of parent element */
  uxml_node_t *child;     /* index of first child element (for XML_NODE only), 0 means no child */
  uxml_node_t *next;      /* index of next element (not for XML_INST), 0 means last element */
  int modcount;           /* modification count */
  void *user;             /* user pointer */
};

#define isdigit( c ) (c>='0'&&c<='9')
#define isalpha( c ) ((c>='A'&&c<='Z')||(c>='a'&&c<='z'))
#define isspace( c ) (c==' '||c=='\n'||c=='\t'||c=='\r')

/*
 * Get character, dispatch escape sequences in contents and attributes
 */
static int uxml_getc( uxml_t *p )
{
  int i, t, code = 0, dec = 0, hex = 0, v = 0;

  if( p->xml_index != p->xml_size && p->xml[ p->xml_index ] != 0 )
  {
    p->c[0] = p->c[1];
    p->c[1] = p->c[2];
    p->c[2] = p->c[3];

    p->escape[0] = p->escape[1];
    p->escape[1] = p->escape[2];
    p->escape[2] = p->escape[3];
    p->escape[3] = 0;

    if( p->xml[ p->xml_index ] == '&' &&
       (p->state == NODE_CONTENT ||
        p->state == NODE_CONTENT_TRIM ||
        p->state == NODE_ATTR_VALUE_DQ ||
        p->state == NODE_ATTR_VALUE_SQ ||
        p->state == INST_ATTR_VALUE_DQ ||
        p->state == INST_ATTR_VALUE_SQ) )          /* escape sequences in attribute value or the content */
    {
      p->xml_index++;
      p->column++;
      for( i = p->xml_index; p->xml_index != p->xml_size && p->xml[ p->xml_index ] != 0; )
      {
        t = p->xml[ p->xml_index++ ];
        p->column++;

        if( t == ';' )
        {
          if( (p->xml_index - i) == 3 && p->xml[i] == 'l' && p->xml[i+1] == 't' )
          {
            p->c[3] = '<';
            p->escape[3] = 1;
            return 1;
          }
          else if( (p->xml_index - i) == 3 && p->xml[i] == 'g' && p->xml[i+1] == 't' )
          {
            p->c[3] = '>';
            p->escape[3] = 1;
            return 1;
          }
          else if( (p->xml_index - i) == 4 && p->xml[i] == 'a' && p->xml[i+1] == 'm' && p->xml[i+2] == 'p' )
          {
            p->c[3] = '&';
            p->escape[3] = 1;
            return 1;
          }
          else if( (p->xml_index - i) == 5 && p->xml[i] == 'a' && p->xml[i+1] == 'p' && p->xml[i+2] == 'o' && p->xml[i+3] == 's' )
          {
            p->c[3] = '\'';
            p->escape[3] = 1;
            return 1;
          }
          else if( (p->xml_index - i) == 5 && p->xml[i] == 'q' && p->xml[i+1] == 'u' && p->xml[i+2] == 'o' && p->xml[i+3] == 't' )
          {
            p->c[3] = '\"';
            p->escape[3] = 1;
            return 1;
          }
          else if( dec || hex )
          {
            p->c[3] = v;
            p->escape[3] = 1;
            return 1;
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
    p->c[3] = p->xml[ p->xml_index++ ];

    if( p->c[3] == '\n' )
    {
      p->line++;
      p->column = 1;
    }
    else if( p->c[3] == '\r' )
    {
      p->column = 1;
    }
    else
    {
      p->column++;
    }
    return 1;
  }
  return 0;
}

/*
 * parse process instruction
 */
static int uxml_parse_inst( uxml_t *p )
{
  int *c = p->c + 3;
  int *escape = p->escape + 3;
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
    n->fullsize = 0;                   /* full size of content is 0 too */
    n->instance = p;                   /* our instance */
    n->parent = NULL;                  /* no parent */
    n->child = NULL;                   /* no child node(s) */
  }
  p->node_index++;                     /* next node */

  p->state = INST_NAME;                /* new state - read instruction name */
  while( uxml_getc( p ) )          /* read new character */
  {
    if( p->state == INST_NAME )        /* is name reading ? */
    {
      if( !isspace( c[0] ) )           /* non-space character? that is name */
      {
        if( p->text != NULL )          /* if text buffer present, */
        {
          p->text[ p->text_index ] = c[0]; /* store current character of name */
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
      if( isalpha( c[0] ) )            /* attribute begin with alphabet character? */
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
          a->fullsize = 0;             /* full size of content is 0 too */
          a->instance = p;             /* our instance */
          a->parent = p->node + node_index; /* parent process instruction */
          a->child = NULL;                /* attribute have no child nodes */
          a->next = NULL;                 /* no next node (yet) */
        }
        p->node_index++;
        if( p->text != NULL )          /* if text buffer present, */
        {
          p->text[ p->text_index ] = c[0]; /* store first character of attribute's name */
          a->name_length++;
        }
        p->text_index++;               /* prepare for next character */
      }
      else if( c[-1] == '?' && c[0] == '>' )
      {
        p->state = state;              /* restore outer state */
        return node_index;             /* dispatch done */
      }
      else if( !(c[0] == '?') )
      {
        if( !isspace( c[0] ) )
        {
          p->error = "Invalid character";
          return 0;
        }
      }
    }
    else if( p->state == INST_ATTR_NAME )
    {
      if( isspace( c[0] ) )           /* attribute's name end with '=' */
      {
        p->state = INST_ATTR_EQ;       /* new state */
        p->text_index++;               /* end name with zero byte */
      }
      else if( c[0] == '=' )           /* attribute's name end with '=' */
      {
        p->state = INST_ATTR_EQ_FOUND; /* new state */
        p->text_index++;               /* end name with zero byte */
      }
      else                             /* all other characters means error */
      {
        if( p->text != NULL )          /* if text buffer present, */
        {
          p->text[ p->text_index ] = c[0]; /* store next character of attribute's name */
          a->name_length++;
        }
        p->text_index++;               /* go to next character */
      }
    }
    else if( p->state == INST_ATTR_EQ )
    {
      if( c[0] == '=' )
      {
        p->state = INST_ATTR_EQ_FOUND;
      }
      else if( !isspace( c[0] ) )
      {
        p->error = "Extra character after attribute's name";
        return 0;
      }
    }
    else if( p->state == INST_ATTR_EQ_FOUND )
    {
      if( c[0] == '\"' )               /* start attribute's value reading "value" */
      {
        p->state = INST_ATTR_VALUE_DQ; /* double quoted value */
        if( a != NULL )                /* of course, do not forget real parsing */
        {
          a->content = p->text + p->text_index; /* content points to attribute's value */
        }
      }
      else if( c[0] == '\'' )          /* start attribute's value reading 'value' */
      {
        p->state = INST_ATTR_VALUE_SQ; /* single quoted value */
        if( a != NULL )                /* of course, do not forget real parsing */
        {
          a->content = p->text + p->text_index;  /* content points to attribute's value */
        }
      }
      else if( !isspace( c[0] ) )      /* error in other non-space character */
      {
        p->error = "Attribute value must begin with '\"' or '\''";
        return 0;
      }
    }
    else if( p->state == INST_ATTR_VALUE_DQ )
    {
      if( c[0] == '\"' && !escape[0] ) /* value ended? */
      {
        p->text_index++;               /* end value with zero byte */
        p->state = INST_TAG;           /* return to tag dispatch */
      }
      else
      {
        if( p->text != NULL )          /* while real parsing */
        {
          p->text[ p->text_index ] = c[0]; /* store next value character */
        }
        p->text_index++;               /* next byte for value */
        if( a != NULL )
        {
          a->size++;
          a->fullsize++;
        }
      }
    }
    else if( p->state == INST_ATTR_VALUE_SQ )
    {
      if( c[0] == '\'' && !escape[0] ) /* value ended? */
      {
        p->text_index++;               /* end value with zero byte */
        p->state = INST_TAG;           /* return to tag dispatch */
      }
      else
      {
        if( p->text != NULL )          /* while real parsing */
        {
          p->text[ p->text_index ] = c[0]; /* store next value character */
        }
        p->text_index++;               /* next byte for value */
        if( a != NULL )
        {
          a->size++;
          a->fullsize++;
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
  int *c = p->c + 3;
  int *escape = p->escape + 3;
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
    n->fullsize = 0;
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
  while( uxml_getc( p ) )          /* read new character */
  {
    if( p->state == NODE_NAME )        /* is name reading ? */
    {
      if( c[-1] == '/' && c[0] == '>' )
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
      else if( c[0] == '/' )
      {
        continue;
      }
      else if( c[0] == '>' )
      {
        p->text_index++;               /* terminate name with zero byte */
        p->state = NODE_CONTENT_TRIM;  /* start content dispatch */
      }
      else if( !isspace( c[0] ) )           /* non-space character? that is name */
      {
        if( p->text != NULL )          /* if text buffer present, */
        {
          p->text[ p->text_index ] = c[0]; /* store current character of name */
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
      if( isalpha( c[0] ) )            /* attribute begin with alphabet character? */
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
          a->fullsize = 0;
          a->instance = p;             /* our instance */
          a->parent = p->node + node_index; /* parent process instruction */
          a->child = NULL;                /* attribute have no chid nodes */
          a->next = NULL;                 /* no next node (yet) */
        }
        last_child = p->node_index;    /* new last child */
        p->node_index++;
        if( p->text != NULL )          /* if text buffer present, */
        {
          p->text[ p->text_index ] = c[0]; /* store first character of attribute's name */
          a->name_length++;
        }
        p->text_index++;               /* prepare for next character */
      }
      else if( c[-1] == '/' && c[0] == '>' )
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
      else if( c[0] == '>' )           /* node tag over */
      {
        p->state = NODE_CONTENT_TRIM;  /* start content dispatch */
      }
      else if( !isspace( c[0] ) && c[0] != '/' )      /* all other non-spaced symbols (digits, specials) */
      {
        p->error = "Invalid character"; /* means error */
        return 0;
      }
    }
    else if( p->state == NODE_ATTR_NAME )
    {
      if( isspace( c[0] ) )
      {
        p->state = NODE_ATTR_EQ;       /* new state */
        p->text_index++;               /* end name with zero byte */
      }
      else if( c[0] == '=' )
      {
        p->state = NODE_ATTR_EQ_FOUND; /* new state */
        p->text_index++;               /* end name with zero byte */
      }
      else
      {
        if( p->text != NULL )          /* if text buffer present, */
        {
          p->text[ p->text_index ] = c[0]; /* store next character of attribute's name */
          a->name_length++;
        }
        p->text_index++;               /* go to next character */
      }
    }
    else if( p->state == NODE_ATTR_EQ )
    {
      if( c[0] == '=' )
      {
        p->state = NODE_ATTR_EQ_FOUND;
      }
      else if( !isspace( c[0] ) )
      {
        p->error = "Extra character after attribute's name";
        return 0;
      }
    }
    else if( p->state == NODE_ATTR_EQ_FOUND )
    {
      if( c[0] == '\"' )               /* start attribute's value reading "value" */
      {
        p->state = NODE_ATTR_VALUE_DQ; /* double quoted value */
        if( a != NULL )                /* of course, do not forget real parsing */
        {
          a->content = p->text + p->text_index;  /* content points to attribute's value */
        }
      }
      else if( c[0] == '\'' )          /* start attribute's value reading 'value' */
      {
        p->state = NODE_ATTR_VALUE_SQ; /* single quoted value */
        if( a != NULL )                /* of course, do not forget real parsing */
        {
          a->content = p->text + p->text_index;  /* content points to attribute's value */
        }
      }
      else if( !isspace( c[0] ) )      /* error in other non-space character */
      {
        p->error = "Attribute value must begin with '\"' or '\''";
        return 0;
      }
    }
    else if( p->state == NODE_ATTR_VALUE_DQ )
    {
      if( c[0] == '\"' && !escape[0] ) /* value ended? */
      {
        p->text_index++;               /* end value with zero byte */
        p->state = NODE_TAG;           /* return to tag dispatch */
      }
      else
      {
        if( p->text != NULL )          /* while real parsing */
        {
          p->text[ p->text_index ] = c[0]; /* store next value character */
        }
        p->text_index++;               /* next byte for value */
        if( a != NULL )
        {
          a->size++;
          a->fullsize++;
        }
      }
    }
    else if( p->state == NODE_ATTR_VALUE_SQ )
    {
      if( c[0] == '\'' && !escape[0] ) /* value ended? */
      {
        p->text_index++;               /* end value with zero byte */
        p->state = NODE_TAG;           /* return to tag dispatch */
      }
      else
      {
        if( p->text != NULL )          /* while real parsing */
        {
          p->text[ p->text_index ] = c[0]; /* store next value character */
        }
        p->text_index++;               /* next byte for value */
        if( a != NULL )
        {
          a->size++;
          a->fullsize++;
        }
      }
    }
    else if( p->state == NODE_CONTENT_TRIM || p->state == NODE_CONTENT )
    {
      if( (c[-1] == '<' && !escape[-1] && c[0] == '!' && !escape[0] ) ||
          (c[-2] == '<' && !escape[-2] && c[-1] == '!' && !escape[-1] && c[0] == '-' && !escape[0] ) )
      {
        continue;
      }
      if( c[-1] == '<' && !escape[-1] )
      {
        if( isalpha( c[0] ) )          /* open new node? */
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
        else if( c[0] == '/' && !escape[0] )
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
      else if( c[-3]=='<' && !escape[-3] && c[-2]=='!' && !escape[-2] && c[-1]=='-' && !escape[-1] && c[0]=='-' && !escape[0] )
      {
        comment_state = p->state;      /* keep state before comment occured */
        p->state = COMMENT;            /* comment in */
      }
      else if( c[0] != '<' || escape[0] )           /* regular symbol of content */
      {
        if( p->state == NODE_CONTENT_TRIM )
        {
          if( !isspace( c[0] ) )       /* non-space character? */
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
                n->fullsize++;
              }
            }
            else
            {
              content_begin = p->text_index; /* the content begin. */
            }
            if( p->text != NULL )      /* store it, if needed */
            {
              p->text[ p->text_index ] = c[0];
            }
            p->text_index++;               /* next character */
            if( n != NULL )
            {
              n->size++;
              n->fullsize++;
            }
            content_end = p->text_index;   /* and content's end */
          }
        }
        else                           /* need to read content */
        {
          if( isspace( c[0] ) && !escape[0] )        /* all empty non-escaped characters will replaced with one space */
          {
            p->state = NODE_CONTENT_TRIM;
          }
          else
          {
            if( p->text != NULL )      /* store character, if needed */
            {
              p->text[ p->text_index ] = c[0];
            }
            p->text_index++;             /* next character */
            if( n != NULL )
            {
              n->size++;
              n->fullsize++;
            }
            content_end = p->text_index; /* and content's end */
          }
        }
      }
    }
    else if( p->state == COMMENT )     /* is there comments inside? */
    {
      if( c[-2]=='-'&&c[-1]=='-'&&c[0]=='>' ) /* comment over? */
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
      if( c[0] == '>' )                /* node-end tag over? */
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
  int *c = p->c + 3;
  int root = 0;

  while( uxml_getc( p ) )
  {
    if( p->state == NONE )
    {
      if( c[-1] == '<' && isalpha( c[0] ) )
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
      else if( c[-1] == '<' && c[0] == '?' )
      {
        if( !uxml_parse_inst( p ) )
          return 0;
      }
      else if( c[-3]=='<' && c[-2]=='!' && c[-1]=='-' && c[0]=='-' )
      {
        p->state = COMMENT;
      }
      else if( !((c[-2]=='<' && c[-1]=='!' && c[0]=='-') ||
                (c[-1]=='<' && c[0]=='!') ||
                (c[0]=='<')) )
      {
        if( !isspace( c[0] ) )
        {
          p->error = "Unrelated character";
          return 0;
        }
      }
    }
    else if( p->state == COMMENT )
    {
      if( c[-2]=='-'&&c[-1]=='-'&&c[0]=='>' )
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

uxml_node_t *uxml_parse( const char *xml_data, const int xml_length, uxml_error_t *error )
{
  uxml_t instance, *p = &instance;
  void *v;
  char *c;
  int i;

  p->xml = (const unsigned char *)xml_data;
  p->xml_index = 0;
  p->xml_size = xml_length;
  p->text = NULL;
  p->text_index = 0;
  //p->text_size = 0;
  //p->text_allocated = 0;
  p->node = NULL;
  p->node_index = 1;
  //p->nodes_count = 1;
  //p->nodes_allocated = 0;
  p->state = NONE;
  p->c[0] = p->c[1] = p->c[2]= p->c[3] = 0;
  p->escape[0] = p->escape[1] = p->escape[2] = p->escape[3] = 0;
  p->line = 1;
  p->column = 1;
  p->error = NULL;
  p->dump = NULL;
  p->dump_size = 0;
  for( i = 0; i < MAX_FRAG; i++ )
  {
    p->atext[i]           = NULL;
    p->text_size[i]       = 0;
    p->text_allocated[i]  = 0;
    p->anode[i]           = NULL;
    p->nodes_count[i]     = 0;
    p->nodes_allocated[i] = 0;
  }
  p->text_frag = 0;
  p->node_frag = 0;

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
  i = sizeof( uxml_t ) + 1 + p->text_index + p->node_index * sizeof( uxml_node_t );

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
  p->atext[0] = p->text;
  p->text_size[0] = p->text_index + 1;
  p->text_allocated[0] = p->text_size[0];
  p->text_index = 1;
  p->anode[0] = p->node;
  p->nodes_count[0] = p->node_index;
  p->nodes_allocated[0] = p->nodes_count[0];
  p->node_index = 1;
  p->state = NONE;
  p->c[0] = p->c[1] = p->c[2]= p->c[3] = 0;
  p->line = 1;
  p->column = 1;
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
  uxml_t *p = node->instance;
  int i;

  for( i = 1; i < MAX_FRAG; i++ )
  {
    if( p->atext[i] != NULL ) free( p->atext[i] );
    if( p->anode[i] != NULL ) free( p->anode[i] );
  }
  if( p->dump != NULL )
    free( p->dump );
  free( p );
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

uxml_node_t *uxml_next( uxml_node_t *node )
{
  return node->next;
}

const char *uxml_name( uxml_node_t *node )
{
  return (const char *)node->name;
}

uxml_node_t *uxml_node( uxml_node_t *node, const char *ipath )
{
  uxml_t *p = node->instance;
  const char *path = ipath, *s1, *s2;
  int j;
  uxml_node_t *n = node;

  if( path == NULL )
  {
    path = "";
  }
  if( path[0] == '/' )
  {
    n = p->node[0].next;
    path++;
  }
  for( s1 = path, s2 = path; *s2 != 0; s2++ )
  {
    if( *s2 == '/' )
    {
      if( s1 == s2 )
      {
        s1 = s2 + 1;
        continue;
      }
      j = s2 - s1;
      if( j == 2 )
      {
        if( s1[0] == '.' && s1[1] == '.' )
        {
          if( n->parent == NULL )
          {
            return NULL;
          }
          n = n->parent;
          s1 = s2 + 1;
          continue;
        }
      }
      for( n = n->child; n != NULL; n = n->next )
      {
        if( j == n->name_length )
        {
          if( memcmp( s1, n->name, j ) == 0 )
          {
            s1 = s2 + 1;
            break;
          }
        }
      }
      if( n == NULL )
      {
        return NULL;
      }
    }
  }
  if( s2 != s1 )
  {
    j = s2 - s1;
    if( j == 2 )
    {
      if( s1[0] == '.' && s1[1] == '.' )
      {
        return n->parent;
      }
    }
    for( n = n->child; n != NULL; n = n->next )
    {
      if( j == n->name_length )
      {
        if( memcmp( s1, n->name, j ) == 0 )
        {
          break;
        }
      }
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

int uxml_copy( uxml_node_t *node, const char *path, char *buffer, const int buffer_size )
{
  uxml_node_t *n = uxml_node( node, path );
  int i;

  if( n == NULL )
    return 0;

  i = ((n->size == 0) ? 1: (n->size + 1));
  if( i > buffer_size && buffer_size > 1 )
  {
    memcpy( buffer, n->content, buffer_size - 1 );
    buffer[ buffer_size - 1 ] = 0;
    return buffer_size;
  }
  else if( i <= buffer_size )
  {
    memcpy( buffer, n->content, i );
    return i;
  }
  return 0;
}

int uxml_set( uxml_node_t *node, const char *path, const char *value, const int size0 )
{
  uxml_node_t *n = uxml_node( node, path );
  uxml_t *p;
  int size, i;
  unsigned char *t;

  if( n == NULL )
    return 0;

  p = n->instance;

  if( size0 == 0 )
    size = strlen( value ) + 1;
  else
    size = size0 + 1;

  if( size > (n->fullsize + 1) )
  {
    if( size > (p->text_allocated[ p->text_frag ] - p->text_size[ p->text_frag ]) )
    {
      for( i = (p->text_allocated[ p->text_frag ] << 1); i < size; i <<= 1 );
      if( (t = malloc( i )) == NULL )
      {
        p->error = "malloc failed";
        return 0;
      }
      p->text_frag++;
      p->atext[ p->text_frag ] = t;
      p->text_allocated[ p->text_frag ] = i;
      p->text_size[ p->text_frag ] = 0;
    }
    n->content = p->atext[ p->text_frag ] + p->text_size[ p->text_frag ];
    memcpy( n->content, value, size );
    n->size = size - 1;
    n->fullsize = size - 1;
    p->text_size[ p->text_frag ] += size;
  }
  else
  {
    memcpy( n->content, value, size );
    n->size = size - 1;
  }
  n->modcount++;
  return 1;
}

int uxml_modcount( uxml_node_t *node, const char *path )
{
  uxml_node_t *n = uxml_node( node, path );
  return (n == NULL) ? 0: n->modcount;
}

int uxml_size( uxml_node_t *node, const char *path )
{
  uxml_node_t *n = uxml_node( node, path );
  return (n == NULL) ? 0: n->size;
}

static void uxml_add_child_count( uxml_node_t *node, int *ct, int *cn )
{
  uxml_node_t *n;
  (*ct) += (strlen( (char *)node->name ) + 1);
  (*ct) += (node->fullsize + 1);
  (*cn) += 1;

  for( n = node->child; n != NULL; n = n->next )
  {
    uxml_add_child_count( n, ct, cn );
  }
}

static uxml_node_t *uxml_add_child_fill( uxml_t *p, uxml_node_t *s, uxml_node_t *parent_node )
{
  uxml_node_t *n = p->anode[ p->node_frag ] + p->nodes_count[ p->node_frag ];
  uxml_node_t *last_child = NULL, *k, *c;

  p->nodes_count[ p->node_frag ]++;

  n->type = s->type;

  n->name = p->atext[ p->text_frag ] + p->text_size[ p->text_frag ];
  memcpy( n->name, s->name, strlen( (char *)s->name ) + 1 );
  p->text_size[ p->text_frag ] += (strlen( (char *)s->name ) + 1);

  n->content = p->atext[ p->text_frag ] + p->text_size[ p->text_frag ];
  memcpy( n->content, s->content, s->fullsize + 1 );
  p->text_size[ p->text_frag ] += (s->fullsize + 1);

  n->size = s->size;
  n->fullsize = s->fullsize;
  n->instance = p;
  n->parent = parent_node;
  n->next = NULL;
  n->child = NULL;

  if( parent_node->child == NULL )
  {
    parent_node->child = n;
  }
  else
  {
    for( k = parent_node->child; k->next != NULL; k = k->next );
    k->next = n;
  }
  n->modcount = s->modcount;

  for( k = s->child; k != NULL; k = k->next )
  {
    c = uxml_add_child_fill( p, k, n );
    if( last_child != NULL )
    {
      last_child->next = c;
    }
    last_child = c;
  }
  return n;
}

int uxml_add_child( uxml_node_t *node, uxml_node_t *child )
{
  uxml_t *p = node->instance;
  int i, j = 0, k = 0;
  unsigned char *t = NULL;
  uxml_node_t *n = NULL;

  uxml_add_child_count( child, &j, &k );

  if( j > (p->text_allocated[ p->text_frag ] - p->text_size[ p->text_frag ]) )
  {
    for( i = (p->text_allocated[ p->text_frag ] << 1); i < j; i <<= 1 );
    if( (t = malloc( i )) == NULL )
    {
      p->error = "malloc failed";
      return 0;
    }
    memset( t, 0, i );
    p->text_frag++;
    p->atext[ p->text_frag ] = t;
    p->text_allocated[ p->text_frag ] = i;
    p->text_size[ p->text_frag ] = 0;
  }

  if( k > (p->nodes_allocated[ p->node_frag ] - p->nodes_count[ p->node_frag ]) )
  {
    for( i = (p->nodes_allocated[ p->node_frag ] << 1); i < k; i <<= 1 );
    if( (n = malloc( sizeof( uxml_node_t ) * i )) == NULL )
    {
      p->error = "malloc failed";
      return 0;
    }
    memset( n, 0, sizeof( uxml_node_t ) * i );
    p->node_frag++;
    p->anode[ p->node_frag ] = n;
    p->nodes_allocated[ p->node_frag ] = i;
    p->nodes_count[ p->node_frag ] = 0;
  }
  uxml_add_child_fill( p, child, node );
  return 1;
}

static void uxml_putchar( uxml_t *p, const int c )
{
  if( p->dump != NULL && p->dump_index < p->dump_size )
    p->dump[ p->dump_index ] = (unsigned char)c;
  p->dump_index++;
}

static void uxml_put_escape( uxml_t *p, const int c )
{
  switch( c )
  {
  case '<':
    uxml_putchar( p, '&' );
    uxml_putchar( p, 'l' );
    uxml_putchar( p, 't' );
    uxml_putchar( p, ';' );
    break;
  case '>':
    uxml_putchar( p, '&' );
    uxml_putchar( p, 'g' );
    uxml_putchar( p, 't' );
    uxml_putchar( p, ';' );
    break;
  case '&':
    uxml_putchar( p, '&' );
    uxml_putchar( p, 'a' );
    uxml_putchar( p, 'm' );
    uxml_putchar( p, 'p' );
    uxml_putchar( p, ';' );
    break;
  case '\'':
    uxml_putchar( p, '&' );
    uxml_putchar( p, 'a' );
    uxml_putchar( p, 'p' );
    uxml_putchar( p, 'o' );
    uxml_putchar( p, 's' );
    uxml_putchar( p, ';' );
    break;
  case '\"':
    uxml_putchar( p, '&' );
    uxml_putchar( p, 'q' );
    uxml_putchar( p, 'u' );
    uxml_putchar( p, 'o' );
    uxml_putchar( p, 't' );
    uxml_putchar( p, ';' );
    break;
  default:
    if( c < ' ' )
    {
      uxml_putchar( p, '&' );
      uxml_putchar( p, '#' );
      if( c < 10 )
      {
        uxml_putchar( p, '0' + c );
      }
      else
      {
        uxml_putchar( p, '0' + (c / 10) );
        uxml_putchar( p, '0' + (c % 10) );
      }
      uxml_putchar( p, ';' );
    }
    else
    {
      uxml_putchar( p, c );
    }
    break;
  }
}

static void uxml_dump_internal( uxml_t *p, const int offset, uxml_node_t *node )
{
  int i, in = 0;
  uxml_node_t *n;

  for( i = 0; i < offset; i++ )
    uxml_putchar( p, ' ' );

  uxml_putchar( p, '<' );
  if( node->type == XML_INST )
    uxml_putchar( p, '?' );

  for( i = 0; node->name[i] != 0; i++ )
    uxml_putchar( p, node->name[i] );

  for( n = node->child; n != NULL; n = n->next )
  {
    switch( n->type )
    {
    case XML_ATTR:
      uxml_putchar( p, ' ' );
      for( i = 0; n->name[i] != 0; i++ )
        uxml_putchar( p, n->name[i] );
      uxml_putchar( p, '=' );
      uxml_putchar( p, '\"' );
      for( i = 0; i < n->size; i++ )
        uxml_put_escape( p, n->content[i] );
      uxml_putchar( p, '\"' );
      break;
    case XML_NODE: in++; break;
    default: break;
    }
  }
  if( node->type == XML_INST )
  {
    uxml_putchar( p, '?' );
    uxml_putchar( p, '>' );
    uxml_putchar( p, '\n' );
  }
  else if( in != 0 || node->size != 0 )
  {
    uxml_putchar( p, '>' );
    uxml_putchar( p, '\n' );
    if( node->size != 0 )
    {
      for( i = 0; i < offset + 2; i++ )
        uxml_putchar( p, ' ' );
      for( i = 0; i < node->size; i++ )
        uxml_put_escape( p, node->content[i] );
      uxml_putchar( p, '\n' );
    }
    for( n = node->child; n != 0; n = n->next )
    {
      if( n->type == XML_NODE )
      {
        uxml_dump_internal( p, offset + 2, n );
      }
    }
    for( i = 0; i < offset; i++ )
      uxml_putchar( p, ' ' );

    uxml_putchar( p, '<' );
    uxml_putchar( p, '/' );

    for( i = 0; node->name[i] != 0; i++ )
      uxml_putchar( p, node->name[i] );

    uxml_putchar( p, '>' );
    uxml_putchar( p, '\n' );
  }
  else
  {
    uxml_putchar( p, '/' );
    uxml_putchar( p, '>' );
    uxml_putchar( p, '\n' );
  }
}

unsigned char *uxml_dump( uxml_node_t *root, unsigned char *dump, const int max_dump, int *dump_size )
{
  uxml_t *p = root->instance;
  int i = 0;
  unsigned char *t;

  t = p->dump;
  p->dump = NULL;
  p->dump_index = 0;
  for( i = 1; i < (p->node[0].next - p->node); i++ )
  {
    if( p->node[ i ].type == XML_INST )
    {
      uxml_dump_internal( p, 0, p->node + i );
    }
  }
  uxml_dump_internal( p, 0, p->node[0].next );
  uxml_putchar( p, 0 );

  p->dump = t;

  if( p->dump_index == 0 )
    return NULL;

  if( p->dump_index > p->dump_size )
  {
    if( p->dump != NULL )
    {
      free( p->dump );
      p->dump = NULL;
    }
    if( (p->dump = malloc( p->dump_index )) == NULL )
      return NULL;
    p->dump_size = p->dump_index;
  }
  p->dump_index = 0;
  for( i = 1; i < (p->node[0].next - p->node); i++ )
  {
    if( p->node[ i ].type == XML_INST )
    {
      uxml_dump_internal( p, 0, p->node + i );
    }
  }
  uxml_dump_internal( p, 0, p->node[0].next );
  uxml_putchar( p, 0 );
  if( p->dump_index > p->dump_size )
    return NULL;
  t = dump;
  if( t == NULL )
  {
    if( (t = malloc( p->dump_size )) == NULL )
      return NULL;
  }
  else
  {
    if( p->dump_size > max_dump )
      return NULL;
    t = dump;
  }
  memcpy( t, p->dump, p->dump_size );
  if( dump_size != NULL )
    *dump_size = p->dump_size;
  return t;
}

void uxml_dump_free( unsigned char *dump )
{
  if( dump != NULL ) free( dump );
}

static uxml_node_t *uxml_new( uxml_node_t *node, const int node_type, const char *name, const char *content )
{
  uxml_t *p = node->instance;
  uxml_node_t *k;
  int i, j, name_size, content_size;
  unsigned char *t = NULL;
  uxml_node_t *n = NULL;

  name_size = strlen( name ) + 1;
  content_size = (content != NULL) ? (strlen( content ) + 1): 0;
  j = name_size + content_size;

  if( j > (p->text_allocated[ p->text_frag ] - p->text_size[ p->text_frag ]) )
  {
    for( i = (p->text_allocated[ p->text_frag ] << 1); i < j; i <<= 1 );
    if( (t = malloc( i )) == NULL )
    {
      p->error = "malloc failed";
      return 0;
    }
    memset( t, 0, i );
    p->text_frag++;
    p->atext[ p->text_frag ] = t;
    p->text_allocated[ p->text_frag ] = i;
    p->text_size[ p->text_frag ] = 0;
  }
  if( p->nodes_allocated[ p->node_frag ] <= p->nodes_count[ p->node_frag ] )
  {
    i = (p->nodes_allocated[ p->node_frag ] << 1);
    if( (n = malloc( sizeof( uxml_node_t ) * i )) == NULL )
    {
      p->error = "malloc failed";
      return 0;
    }
    memset( n, 0, sizeof( uxml_node_t ) * i );
    p->node_frag++;
    p->anode[ p->node_frag ] = n;
    p->nodes_allocated[ p->node_frag ] = i;
    p->nodes_count[ p->node_frag ] = 0;
  }
  n = p->anode[ p->node_frag ] + p->nodes_count[ p->node_frag ];
  p->nodes_count[ p->node_frag ]++;

  n->type = node_type;

  n->name = p->atext[ p->text_frag ] + p->text_size[ p->text_frag ];
  memcpy( n->name, name, name_size );
  p->text_size[ p->text_frag ] += name_size;

  if( content_size != 0 )
  {
    n->content = p->atext[ p->text_frag ] + p->text_size[ p->text_frag ];
    memcpy( n->content, content, content_size );
    p->text_size[ p->text_frag ] += content_size;
    n->size = content_size - 1;
    n->fullsize = content_size;
  }
  else
  {
    n->content = p->text;
    n->size = 0;
    n->fullsize = 0;
  }
  n->instance = p;
  n->parent = node;
  n->next = NULL;
  n->child = NULL;

  if( node->child == NULL )
  {
    node->child = n;
  }
  else
  {
    for( k = node->child; k->next != NULL; k = k->next );
    k->next = n;
  }
  n->modcount = 0;
  return n;
}

uxml_node_t *uxml_new_node( uxml_node_t *node, const char *name, const char *content )
{
  return uxml_new( node, XML_NODE, name, content );
}

uxml_node_t *uxml_new_attr( uxml_node_t *node, const char *name, const char *content )
{
  return uxml_new( node, XML_ATTR, name, content );
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

  for( i = 0; i < p->nodes_count[0]; i++ )
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
    if( p->node[i].modcount != 0 )
      printf( " modcount=%d", p->node[i].modcount );
    printf( "\n" );
  }
  printf( "Total nodes: %d, text size/allocated: %d/%d\n", p->nodes_count[0], p->text_size[0], p->text_allocated[0] );
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
static const unsigned char base64[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int uxml_encode64( unsigned char *dst, const int n_dst, const unsigned char *src, const int n_src )
{
  unsigned char *d;
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

int uxml_decode64( unsigned char *dst, const int n_dst, const unsigned char *src, const int n_src )
{
  static char decode_tab[128];
  static int decode_tab_initialized = 0;
  unsigned char *d;
  const unsigned char *s;
  int i, n, k, v[4];

  if( !decode_tab_initialized )
  {
    for( i = 0; i < sizeof( decode_tab ); i++ ) decode_tab[i] = -1;
    for( i = 0; i < 64; i++ )
    {
      decode_tab[ base64[i] ] = i;
    }
    decode_tab_initialized = 1;
  }
  for( i = 0, n = n_src, s = src, k = n_dst, d = dst; n != 0; s++, n-- )
  {
    if( decode_tab[ s[0] ] < 0 )
      continue;
    v[ i ] = decode_tab[ s[0] ];
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
  return (d - dst);
}
