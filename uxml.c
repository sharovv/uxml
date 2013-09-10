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

typedef struct _uxml_t
{
  const unsigned char *xml;   /* original XML data */
  int xml_index;     /* current character when parse */
  int xml_size;      /* size of original XML data */
  unsigned char *text;        /* textual data, extracted from original XML */
  int text_index;    /* current write index */
  int text_size;     /* text data size, in bytes */
  int text_allocated;/* allocated size for text data */
  uxml_node_t *node; /* array of nodes, first element - emtpy, second element - root node */
  int node_index;    /* current node while parse */
  int nodes_count;   /* count of nodes */
  int nodes_allocated;/* allocated count for nodes */
  int state;         /* current state */
  int c[4];          /* queue of last 4 characters */
  int escape[4];     /* escape flags for last 4 characters */
  int line;          /* current line, freeze at error position */
  int column;        /* current column, freeze at error position */
  const char *error; /* error's text */      
} uxml_t;

struct _uxml_node_t
{
  int type;            /* element's type - XML_NODE, XML_ATTR, XML_INST */
  int name;            /* element's name */
  int content;         /* element's content / attribute value */
  int size;            /* size of element's content */
  uxml_t *instance;    /* UXML instance */
  int parent;          /* index of parent element */
  int child;           /* index of first child element (for XML_NODE only), 0 means no child */
  int next;            /* index of next element (not for XML_INST), 0 means last element */
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
    n->name = p->text_index;           /* name of instruction */
    n->content = 0;                    /* there is no content, tag only */
    n->size = 0;                       /* size of content is 0 */
    n->instance = p;                   /* our instance */
    n->parent = 0;                     /* no parent */
    n->child = 0;                      /* no child node(s) */
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
          if( n->child == 0 )          /* and no one attribute was parsed yet */
          {
            n->child = p->node_index;  /* store attribute index */ 
          }
        }
        if( a != NULL )                /* if it is not first attribute */
        {
          a->next = p->node_index;     /* fill next of previous attribute */
        }
        if( p->node != NULL )          /* real dispatch? */
        {
          a = p->node + p->node_index; /* current node for attribute */
          a->type = XML_ATTR;          /* this is attribute node */
          a->name = p->text_index;     /* name of attribute */
          a->content = 0;              /* add content later */
          a->size = 0;                 /* add size of content */
          a->instance = p;             /* our instance */
          a->parent = node_index;      /* parent process instruction */
          a->child = 0;                /* attribute have no chid nodes */
          a->next = 0;                 /* no next node (yet) */
        }
        p->node_index++;
        if( p->text != NULL )          /* if text buffer present, */
        {
          p->text[ p->text_index ] = c[0]; /* store first character of attribute's name */
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
          p->error = "Ivalid character";
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
          a->content = p->text_index;  /* content points to attribute's value */
        }
      }
      else if( c[0] == '\'' )          /* start attribute's value reading 'value' */
      {
        p->state = INST_ATTR_VALUE_SQ; /* single quoted value */
        if( a != NULL )                /* of course, do not forget real parsing */
        {
          a->content = p->text_index;  /* content points to attribute's value */
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
static int uxml_parse_node( uxml_t *p, int parent_node )
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
    n->name = p->text_index;           /* name of node */
    n->content = 0;                    /* there is no content yet */
    n->size = 0;
    n->instance = p;                   /* our instance */
    n->parent = parent_node;           /* no parent */
    n->child = 0;                      /* no child node(s) yet */
  }                                    
  p->node_index++;                     /* next node */

  if( p->text != NULL )                /* if text buffer present, */
  {
    p->text[ p->text_index ] = p->xml[ p->xml_index - 1 ];   /* store first character of name */
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
          if( parent_node != 0 )
          {
            if( p->node[ parent_node ].child == 0 )
            {
              p->node[ parent_node ].child = node_index;
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
          if( n->child == 0 )          /* and no one attribute was parsed yet */
          {
            n->child = p->node_index;  /* store attribute index */ 
          }
        }
        if( a != NULL )                /* if it is not first attribute */
        {
          a->next = p->node_index;     /* fill next of previous attribute */
        }
        if( p->node != NULL )          /* real dispatch? */
        {
          a = p->node + p->node_index; /* current node for attribute */
          a->type = XML_ATTR;          /* this is attribute node */
          a->name = p->text_index;     /* name of attribute */
          a->content = 0;              /* add content later */
          a->size = 0;
          a->instance = p;             /* our instance */
          a->parent = node_index;      /* parent process instruction */
          a->child = 0;                /* attribute have no chid nodes */
          a->next = 0;                 /* no next node (yet) */
        }
        last_child = p->node_index;    /* new last child */
        p->node_index++;
        if( p->text != NULL )          /* if text buffer present, */
        {
          p->text[ p->text_index ] = c[0]; /* store first character of attribute's name */
        }
        p->text_index++;               /* prepare for next character */
      }
      else if( c[-1] == '/' && c[0] == '>' )
      {
        p->state = state;              /* restore outer state */
        if( p->node != NULL )
        {          
          if( parent_node != 0 )
          {
            if( p->node[ parent_node ].child == 0 )
            {
              p->node[ parent_node ].child = node_index;
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
          a->content = p->text_index;  /* content points to attribute's value */
        }
      }
      else if( c[0] == '\'' )          /* start attribute's value reading 'value' */
      {
        p->state = NODE_ATTR_VALUE_SQ; /* single quoted value */
        if( a != NULL )                /* of course, do not forget real parsing */
        {
          a->content = p->text_index;  /* content points to attribute's value */
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

          i = uxml_parse_node( p, node_index ); /* parse it */
          if( !i )
            return 0;

          if( p->node != NULL )         /* at real parsing stage */
          {
            if( last_child != 0 )
            {
              p->node[ last_child ].next = i; /* set-up next field of last child node */
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
              n->content = content_begin;  /* for real node, save new content's location too */
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
            }
            content_end = p->text_index;   /* and content's end */
          }
        }
        else                           /* need to read content */        
        {
          if( isspace( c[0] ) )        /* all empty characters will replaced with one space */
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
          p->node[ node_index ].content = content_begin;
          if( parent_node != 0 )
          {
            if( p->node[ parent_node ].child == 0 ) /* if this is first child */
            {
              p->node[ parent_node ].child = node_index; /* parent will point to it */
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
  int *escape = p->escape + 3;
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

  p->xml = xml_data;
  p->xml_index = 0;
  p->xml_size = xml_length;
  p->text = NULL;
  p->text_index = 0;
  p->text_size = 0;
  p->text_allocated = 0;
  p->node = NULL;
  p->node_index = 1;
  p->nodes_count = 1;
  p->nodes_allocated = 0;
  p->state = NONE;
  p->c[0] = p->c[1] = p->c[2]= p->c[3] = 0;
  p->escape[0] = p->escape[1] = p->escape[2] = p->escape[3] = 0;
  p->line = 1;
  p->column = 1;
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
  i = sizeof( uxml_t ) + 1 + p->text_index + p->node_index * sizeof( uxml_node_t );

  if( (v = malloc( i )) == NULL )
  {
    error->text = "Insufficient memory";
    return 0;
  }
  memset( v, 0, i );

  p = (uxml_t *)v;
  c = (char *)v;
  memcpy( p, &instance, sizeof( uxml_t ) );
  c += sizeof( uxml_t );
  p->node = (uxml_node_t *)c;
  c += (sizeof( uxml_node_t ) * p->node_index);
  p->text = c;

  p->xml = xml_data;
  p->xml_index = 0;
  p->xml_size = xml_length;
  p->text_size = p->text_index + 1;
  p->text_index = 1;
  p->nodes_count = p->node_index;
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
  p->node[0].next = i;
  return p->node + i;
}

void uxml_free( uxml_node_t *root )
{
  free( root->instance );
}

uxml_node_t *uxml_child( uxml_node_t *node )
{
  return node->child == 0 ? NULL: node->instance->node + node->child;
}

uxml_node_t *uxml_next( uxml_node_t *node )
{
  return node->next == 0 ? NULL: node->instance->node + node->next;
}

uxml_node_t *uxml_node( uxml_node_t *node, const char *ipath )
{
  uxml_t *p = node->instance;
  const char *path = ipath, *s1, *s2;
  int i, k, n = node - p->node;

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
      if( (s2 - s1) == 2 )
      {
        if( s1[0] == '.' && s1[1] == '.' )
        {
          if( p->node[n].parent == 0 )
          {
            return NULL;
          }
          n = p->node[n].parent;
          s1 = s2 + 1;
          continue;
        }
      }
      for( k = p->node[n].child; k != 0; k = p->node[k].next )
      {
        for( i = 0; i != (s2 - s1); i++ )
        {
          if( s1[i] != p->text[ p->node[k].name + i ] )
            break;
        }
        if( i == (s2 - s1) && p->text[ p->node[k].name + i ] == 0 )
        {
          n = k;
          s1 = s2 + 1;
          break;
        }
      }
      if( k == 0 )
      {
        return NULL;
      }
    }
  }
  if( s2 != s1 )
  {
    if( (s2 - s1) == 2 )
    {
      if( s1[0] == '.' && s1[1] == '.' )
      {
        if( p->node[n].parent == 0 )
        {
          return NULL;
        }
        return p->node + p->node[n].parent;
      }
    }
    for( k = p->node[n].child; k != 0; k = p->node[k].next )
    {
      for( i = 0; i != (s2 - s1); i++ )
      {
        if( s1[i] != p->text[ p->node[k].name + i ] )
          break;
      }
      if( i == (s2 - s1) && p->text[ p->node[k].name + i ] == 0 )
      {
        n = k;
        s1 = s2 + 1;
        break;
      }
    }
    if( k == 0 )
    {
      return NULL;
    }
  }
  return p->node + n;
}

const char *uxml_content( uxml_node_t *node, const char *path )
{
  uxml_node_t *n = uxml_node( node, path );
  return node->instance->text + (n == NULL ? 0: n->content );
}

int uxml_content_size( uxml_node_t *node, const char *path )
{
  uxml_node_t *n = uxml_node( node, path );
  return (n == NULL) ? 0: n->size;
}

int uxml_add_child( uxml_node_t *node, uxml_node_t *child )
{
  uxml_t *p = node->instance;
  uxml_t *c = child->instance;
  int i, j, np, nc, k, q, an, at;
  unsigned char *t0 = NULL;
  uxml_node_t *n0 = NULL;

  at = p->text_allocated;
  an = p->nodes_allocated;

  t0 = p->text;                      /* save text pointer */
  n0 = p->node;                      /* save node pointer */
  k = (int)(child - c->node);

  if( p->text_allocated == 0 )         /* if no addition was maid before */
  {
    for( i = 1; i <= (p->text_size + c->text_size); i <<= 1 ); /* calculate needed size as power of 2 */
  }
  else
  {
    for( i = p->text_allocated; i <= (p->text_size + c->text_size); i <<= 1 ); /* new size */
  }   
  if( i != p->text_allocated )         /* if size changed */
  {
    if( (p->text = malloc( i )) == NULL )    /* reserve new memory block */
    {
      p->text = t0;
      return 0;
    }
  }

  if( p->nodes_allocated == 0 )        /* if no addition was maid before */
  {
    for( j = 1; j <= (p->nodes_count + c->nodes_count); j <<= 1 );
  }
  else
  {
    for( j = p->nodes_allocated; j <= (p->nodes_count + c->nodes_count); j <<= 1 );
  }
  if( (p->node = malloc( sizeof( uxml_node_t ) * j )) == NULL ) /* new memory block */
  {
    free( p->text );                         /* insufficient memory, frees text block */
    p->text = t0;
    p->node = n0;
    return 0;
  }
  memcpy( p->text, t0, p->text_size );
  memcpy( p->text + p->text_size, c->text, c->text_size );
  memcpy( p->node, n0, sizeof( uxml_node_t ) * p->nodes_count );
  for( np = p->nodes_count, nc = 0; nc < c->nodes_count; np++, nc++ )
  {
    p->node[ np ].type = c->node[ nc ].type;
    p->node[ np ].name = ((c->node[ nc ].name == 0) ? 0: (p->text_size + c->node[ nc ].name));
    p->node[ np ].content = ((c->node[ nc ].content == 0) ? 0: (p->text_size + c->node[ nc ].content));
    p->node[ np ].size = c->node[ nc ].size;
    p->node[ np ].instance = p;
    p->node[ np ].parent = ((c->node[ nc ].parent == 0) ? 0: (c->node[ nc ].parent + p->nodes_count));
    p->node[ np ].child  = ((c->node[ nc ].child  == 0) ? 0: (c->node[ nc ].child  + p->nodes_count));
    p->node[ np ].next   = ((c->node[ nc ].next   == 0) ? 0: (c->node[ nc ].next   + p->nodes_count));
  }
  if( node->child == 0 )
  {
    node->child = p->nodes_count + k;
    p->node[ p->nodes_count + k ].parent = (int)(node - p->node);

  }
  else
  {
    q = node->child;
    while( p->node[ q ].next != 0 )
    {
      q = p->node[ q ].next;
    }
    p->node[ q ].next = p->nodes_count + k;
    p->node[ p->nodes_count + k ].parent = p->node[ q ].parent;
  }
  p->text_allocated = i;
  p->text_size += c->text_size;
  
  p->nodes_allocated = j;
  p->nodes_count += c->nodes_count;

  ///////
  
  if( at != 0 )
  {
    free( t0 );
  }
  if( an != 0 )
  {
    free( n0 );
  }
  return 1;
}




#include <stdio.h>

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
    printf( "%d: %s name=\"%s\" content=\"%s\" size=%d parent=%d child=%d next=%d\n",
      i, 
      p->node[i].type == XML_NODE ? "node": (p->node[i].type == XML_ATTR ? "attr": (p->node[i].type == XML_INST ? "inst": (p->node[i].type == XML_NONE ? "none": "????"))),
      p->text + p->node[i].name,
      p->text + p->node[i].content,
      p->node[i].size,
      p->node[i].parent, p->node[i].child, p->node[i].next );
  }
}
