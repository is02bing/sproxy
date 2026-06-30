#ifndef _FILE_MACRO_H
#define _FILE_MACRO_H

// mystrrchr2(“/a/b/c.c”， ‘/’)  返回-> b/c.c
// mystrrchr2(“b/c.c”， ‘/’)  返回-> c.c
const char* mystrrchr2(const char *cp, char ch);
#define __FILENAME__ (mystrrchr2(__FILE__, '/') ? mystrrchr2(__FILE__, '/') + 1 : __FILE__)
#endif
