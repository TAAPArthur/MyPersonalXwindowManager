#ifndef MPX_TESTER_H_
#define MPX_TESTER_H_
#include <scutest/tester.h>
#include <assert.h>
#include <stdio.h>
#include "../util/logger.h"
#include "../globals.h"
#include "../boundfunction.h"


#define getArraySize(A) (A)->size

#define assertAndDelete(X) do {auto* _x=X;assert(_x); delete _x;} while(0)

//#define assertEquals(A,B)do{long__A=(long)A; long __B =(long)B; int __result=(__A==__B); if(!__result){std::cout<<__A<<"!="<<__B<<"\n";assert(0 && #A "!=" #B);}}while(0)
#define assertEquals(A,B) do{long __A=(long)A; long __B =(long)B; \
    int __result=(__A==__B); \
    if(!__result){ \
        printf("\n%ld != %ld\n", __A, __B); \
        assert(0 && #A "!=" #B);\
    }\
}while(0)

#define assertEqualsStr(A,B) do{const char*__A=A, * __B =B; \
    int __result=strcmp(__A,__B); \
    if(__result){ \
        printf("\n'%s' (%ld) != '%s' (%ld)\n", __A, strlen(__A), __B, strlen(__B)); \
        assert(0 && #A "!=" #B);\
    }\
}while(0)

static inline bool returnFalse() {return 0;}
static inline bool returnTrue() {return 1;}
static volatile int count = 0;
static inline void incrementCount(void) {
    count++;
}
static inline void decrementCount(void) {
    LOG_RUN(0, printStackTrace());
    count--;
}
static inline int getCount() {return count;}
static inline int getAndResetCount() {
    int c = count;
    count = 0;
    return c;
}
#endif
