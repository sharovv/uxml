#include <stdio.h>

#define isdigit( c ) (c>='0'&&c<='9')
#define isalpha( c ) ((c>='A'&&c<='Z')||(c>='a'&&c<='z'))
#define isspace( c ) (c==' '||c=='\n'||c=='\t'||c=='\r')
static const unsigned char base64[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static signed char base64_decode_tab[256];

int main()
{
  static unsigned long u;
  static const char *comma[2] = { ",", "" };
  int i, j;

  printf( "static const uxml_isdigit_tab[256]={" );
  for( i = 0; i < 256; i++ ) printf( "%s%d", comma[ i == 0 ], isdigit( i ) );
  printf( "};\n" );

  printf( "static const uxml_isalpha_tab[256]={" );
  for( i = 0; i < 256; i++ ) printf( "%s%d", comma[ i == 0 ], isalpha( i ) );
  printf( "};\n" );

  printf( "static const uxml_isspace_tab[256]={" );
  for( i = 0; i < 256; i++ ) printf( "%s%d", comma[ i == 0 ], isspace( i ) );
  printf( "};\n" );

  printf( "static const unsigned long uxml_isdigit_tab32[8]={" );
  for( i = 0; i < 8; i++ )
  {
    u = 0;
    for( j = 0; j < 32; j++ )
    {
      u |= (isdigit( i * 32 + j ) << j);
    }
    printf( "%s0x%08lX", comma[ i == 0 ], u );
  }
  printf( "};\n" );


  printf( "static const unsigned long uxml_isalpha_tab32[8]={" );
  for( i = 0; i < 8; i++ )
  {
    u = 0;
    for( j = 0; j < 32; j++ )
    {
      u |= (isalpha( i * 32 + j ) << j);
    }
    printf( "%s0x%08lX", comma[ i == 0 ], u );
  }
  printf( "};\n" );

  printf( "static const unsigned long uxml_isspace_tab32[8]={" );
  for( i = 0; i < 8; i++ )
  {
    u = 0;
    for( j = 0; j < 32; j++ )
    {
      u |= (isspace( i * 32 + j ) << j);
    }
    printf( "%s0x%08lX", comma[ i == 0 ], u );
  }
  printf( "};\n" );

  printf( "static const char base64_decode_tab[256]={" );
  for( i = 0; i < sizeof( base64_decode_tab ); i++ ) base64_decode_tab[i] = -1;
  for( i = 0; i < 64; i++ )
  {
    base64_decode_tab[ base64[i] ] = i;
  }
  for( i = 0; i < sizeof( base64_decode_tab ); i++ ) printf( "%s%d", comma[ i == 0 ], base64_decode_tab[i] );
  printf( "};\n" );

  return 0;
}
