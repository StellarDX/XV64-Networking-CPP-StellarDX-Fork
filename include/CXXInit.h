#pragma once

#ifndef CXXINIT_H
#define CXXINIT_H

#define ATEXIT_MAX_FUNCS	128

#ifdef __cplusplus
#include "UDef.hh"
_EXTERN_C
#endif

void CXXInit();

#ifdef __cplusplus
_END_EXTERN_C
#endif

#endif // CXXINIT_H
