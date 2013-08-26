#include <uxml.h>
#include <stdlib.h>
#include <string.h>

enum { NONE,
       NODE_NAME, NODE_TAG, NODE_CONTENT_TRIM, NODE_CONTENT,
       NODE_ATTR_NAME, NODE_ATTR_EQ, NODE_ATTR_VALUE_DQ, NODE_ATTR_VALUE_SQ,
       NODE_END,
       INST_NAME, INST_TAG,
       INST_ATTR_NAME, INST_ATTR_EQ, INST_ATTR_VALUE_DQ, INST_ATTR_VALUE_SQ,
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
| |    ||    ||    | | |        +------ NODE_END if( state==NODE_CONTENT && c[-2]=='<' && c[-1]=='/' )
| |    ||    ||    | | |        |
| |    ||    ||    | | |        |    +- NONE|NODE_CONTENT if( state==NODE_END && c[0]=='>' )
| |    ||    ||    | | |        |    |
 <node1 attr1="val1" > content </node>

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

/* тип элемента - узел, атрибут, инструкция */
enum { XML_NONE, XML_NODE, XML_ATTR, XML_INST };

typedef struct _uxml_t
{
  const char *xml;         /* буфер XML-данных на этапе парсинга */
  int xml_index;     /* индекс текущего байта XML-данных на этапе парсинга */
  int xml_size;      /* размер данных в буфере XML, байт */
  char *text;        /* буфер текстовых данных, извлеченных из XML */
  int text_index;    /* текущий записываемый байт в буфер на этапе парсинга*/
  int text_size;     /* размер текста в буфере, байт */
  uxml_node_t *node; /* array of nodes, first element - emtpy, second element - root node */
  int node_index;    /* текущий элемент на этапе парсинга */
  int nodes_count;   /* количество элементов в XML данных*/
  int state;         /* current state */
  int c[4];          /* queue of last 4 characters */
  int line;          /* current line, freeze at error position */
  int column;        /* current column, freeze at error position */
  const char *error; /* error's text */      
} uxml_t;

struct _uxml_node_t
{
  int type;            /* element's type - XML_NODE, XML_ATTR, XML_INST */
  int name;            /* element's name */
  int content;         /* element's content / attribute value */
  uxml_t *instance;    /* UXML instance */
  int parent;          /* index of parent element */
  int child;           /* index of first child element (for XML_NODE only), 0 means no child */
  int next;            /* index of next element (not for XML_INST), 0 means last element */
  int attr;            /* index of first attribute (not for XML_ATTR), 0 - no attribute(s) */
};

#define isdigit( c ) (c>='0'&&c<='9')
#define isalpha( c ) ((c>='A'&&c<='Z')||(c>='a'&&c<='z'))
#define isspace( c ) (c==' '||c=='\n'||c=='\t'||c=='\r')

static int uxml_getc( uxml_t *p, int **c )
{
  *c = p->c + 3;
  if( p->xml_index != p->xml_size && p->xml[ p->xml_index ] != 0 )
  {
    p->c[0] = p->c[1];
    p->c[1] = p->c[2];
    p->c[2] = p->c[3];
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
  int *c;
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
    n->instance = p;                   /* our instance */
    n->parent = 0;                     /* no parent */
    n->child = 0;                      /* no child node(s) */
    n->attr = 0;                       /* and no attributes yet */
  }                                    
  p->node_index++;                     /* next node */
                                       
  p->state = INST_NAME;                /* new state - read instruction name */
  while( uxml_getc( p, &c ) )          /* read new character */
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
          if( n->attr == 0 )           /* and no one attribute was parsed yet */
          {
            n->attr = p->node_index;   /* store attribute index */ 
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
          a->instance = p;             /* our instance */
          a->parent = node_index;      /* parent process instruction */
          a->child = 0;                /* attribute have no chid nodes */
          a->next = 0;                 /* no next node (yet) */
          a->attr = 0;                 /* and no more attributes */
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
      if( isalpha( c[0] ) || isdigit( c[0] ) ) /* valid character for name? */
      {
        if( p->text != NULL )          /* if text buffer present, */
        {
          p->text[ p->text_index ] = c[0]; /* store next character of attribute's name */
        }
        p->text_index++;               /* go to next character */
      }
      else if( c[0] == '=' )           /* attribute's name end with '=' */
      {
        p->state = INST_ATTR_EQ;       /* new state */
        p->text_index++;               /* end name with zero byte */
      }
      else                             /* all other characters means error */
      {
        p->error = "Attribute name must end with '='";
        return 0;
      }
    }
    else if( p->state == INST_ATTR_EQ )
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
      else                             /* error in other case */
      {
        p->error = "Attribute value must begin with '\"'";
        return 0;
      }
    }
    else if( p->state == INST_ATTR_VALUE_DQ )
    {
      if( c[0] == '\"' )               /* value ended? */
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
      }
    }
    else if( p->state == INST_ATTR_VALUE_SQ )
    {
      if( c[0] == '\'' )               /* value ended? */
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
      }
    }
  }
  p->error = "Unterminated process instruction";
  return 0;
}

/*
 * parse node
 */
static int uxml_parse_node( uxml_t *p, int parent_node )
{
  int *c;
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
    n->instance = p;                   /* our instance */
    n->parent = parent_node;           /* no parent */
    n->child = 0;                      /* no child node(s) yet */
    n->attr = 0;                       /* and no attributes yet */
  }                                    
  p->node_index++;                     /* next node */

  if( p->text != NULL )                /* if text buffer present, */
  {
    p->text[ p->text_index ] = p->xml[ p->xml_index - 1 ];   /* store first character of name */
  }
  p->text_index++;                     /* go to next character */

  //content_begin = p->text_index;       /* content's begin index in text buffer*/
  //content_end = p->text_index;         /* content's end index in text buffer */
                                       
  p->state = NODE_NAME;                /* new state - read instruction name */
  while( uxml_getc( p, &c ) )          /* read new character */
  {                                    
    if( p->state == NODE_NAME )        /* is name reading ? */
    {                                  
      if( c[0] == '>' )
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
          if( n->attr == 0 )           /* and no one attribute was parsed yet */
          {
            n->attr = p->node_index;   /* store attribute index */ 
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
          a->instance = p;             /* our instance */
          a->parent = node_index;      /* parent process instruction */
          a->child = 0;                /* attribute have no chid nodes */
          a->next = 0;                 /* no next node (yet) */
          a->attr = 0;                 /* and no more attributes */
        }
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
      if( isalpha( c[0] ) || isdigit( c[0] ) ) /* valid character for name? */
      {
        if( p->text != NULL )          /* if text buffer present, */
        {
          p->text[ p->text_index ] = c[0]; /* store next character of attribute's name */
        }
        p->text_index++;               /* go to next character */
      }
      else if( c[0] == '=' )           /* attribute's name end with equation sign */
      {
        p->state = NODE_ATTR_EQ;       /* new state */
        p->text_index++;               /* end name with zero byte */
      }
      else                             /* all other characters means error */
      {
        p->error = "Attribute name must end with '='";
        return 0;
      }
    }
    else if( p->state == NODE_ATTR_EQ )
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
      else                             /* error in other case */
      {
        p->error = "Attribute value must begin with '\"'";
        return 0;
      }
    }
    else if( p->state == NODE_ATTR_VALUE_DQ )
    {
      if( c[0] == '\"' )               /* value ended? */
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
      }
    }
    else if( p->state == NODE_ATTR_VALUE_SQ )
    {
      if( c[0] == '\'' )               /* value ended? */
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
      }
    }
    else if( p->state == NODE_CONTENT_TRIM || p->state == NODE_CONTENT )
    {
      if( c[-1] == '<' )
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
        else if( c[0] == '/' )
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
      else if( c[-3]=='<' && c[-2]=='!' && c[-1]=='-' && c[0]=='-' )
      {
        comment_state = p->state;      /* keep state before comment occured */
        p->state = COMMENT;            /* comment in */
      }
      else if( c[0] != '<' )           /* regular symbol of content */
      {
        if( p->state == NODE_CONTENT_TRIM )
        {
          if( !isspace( c[0] ) )       /* first non-space character? */
          {
            p->state = NODE_CONTENT;
            if( p->text != NULL )      /* store it, if needed */
            {
              p->text[ p->text_index ] = c[0];
            }
            content_begin = p->text_index; /* the content begin. */
            p->text_index++;               /* next character */
            content_end = p->text_index;   /* and content's end */
          }
        }
        else                            /* need to read content */
        {
          if( p->text != NULL )        /* store character, if needed */
          {
            p->text[ p->text_index ] = c[0];
          }
          p->text_index++;             /* next character */
          content_end = p->text_index; /* and content's end */
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
            if( p->node[ parent_node ].child != 0 )
            {
              p->node[ parent_node ].child = node_index;
            }
          }
        }
        p->state = state;              /* restore outer state */
        return node_index;             /* dispatch done */
      }
    }
  } 
  p->error = "Unterminated node";
  return 0;
}

static int uxml_parse_doc( uxml_t *p )
{
  int *c;
  int root = 0;

  while( uxml_getc( p, &c ) )
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
          p->error = "Multiple root";
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
  p->node = NULL;
  p->node_index = 1;
  p->nodes_count = 1;
  p->state = NONE;
  p->c[0] = p->c[1] = p->c[2]= p->c[3] = 0;
  p->line = 1;
  p->column = 1;
  p->error = NULL;

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
  return node->instance->node + node->child;
}

uxml_node_t *uxml_next( uxml_node_t *node )
{
  return node->instance->node + node->next;
}

const char *uxml_attr( uxml_node_t *node, const char *name )
{
  uxml_t *p = node->instance;
  int i = node->attr;

  while( i != 0 && p->node[i].type == XML_ATTR )
  {
    if( strcmp( p->text + p->node[i].name, name ) == 0 )
    {
      return p->text + p->node[i].content;
    }
    i++;
  }
  return "";
}

#include <stdio.h>
void uxml_dump_list( uxml_node_t *root )
{
  uxml_t *p = root->instance;
  int i;

  for( i = 0; i < p->nodes_count; i++ )
  {
    printf( "%d: %s name=\"%s\" content=\"%s\" parent=%d child=%d next=%d attr=%d\n",
      i, 
      p->node[i].type == XML_NODE ? "node": (p->node[i].type == XML_ATTR ? "attr": (p->node[i].type == XML_INST ? "inst": "????")),
      p->text + p->node[i].name,
      p->text + p->node[i].content,
      p->node[i].parent, p->node[i].child, p->node[i].next, p->node[i].attr );
  }
}
