//
// General type definitions for portability
// 

#ifndef US_TYPEDEFS
#define US_TYPEDEFS

typedef signed   int     BOOL32;
typedef unsigned char    U8 ;
typedef unsigned short   U16;
typedef unsigned int     U32;
typedef          char    C8 ;
typedef signed   char    S8 ;
typedef signed   short   S16;
typedef signed   int     S32;

#ifdef __cplusplus      // (for H2INC compatibility)
typedef unsigned __int64 U64;
typedef signed   __int64 S64;
#endif

#endif

#ifndef F_TYPEDEFS
#define F_TYPEDEFS

typedef float            SINGLE;
typedef float            F32;
typedef double           DOUBLE;
typedef double           F64;

#endif

#ifndef YES
#define YES 1
#endif

#ifndef NO
#define NO  0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE  0
#endif

