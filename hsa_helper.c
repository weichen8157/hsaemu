#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "hsa_comm_def.h"
#include "hsa_vmdep.h"
#include "hsa_helper.h"
#include "hsa_help_math.h"

#define _gum(__a, __b) __a##__b
#define gum(__x, __y) _gum(__x, __y)

extern __thread hsa_wg_cntxt_t cu_context;

#define MEM_BIT 8
#include "hsa_mem.c"
#undef MEM_BIT
#define MEM_BIT 16
#include "hsa_mem.c"
#undef MEM_BIT
#define MEM_BIT 32
#include "hsa_mem.c"
#include "hsa_atomic_def.h"
#include "hsa_help_math.c"
#undef MEM_BIT
#define MEM_BIT 64
#include "hsa_mem.c"
#include "hsa_atomic_def.h"
#include "hsa_help_math.c"
#undef MEM_BIT
