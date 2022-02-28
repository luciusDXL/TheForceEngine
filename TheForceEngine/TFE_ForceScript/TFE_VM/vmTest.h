#pragma once
/***********************************************************
 TFE VM
 -----------------------------------------------------------
 The core idea is to create the VM and assembly language
 first with the idea that the JIT can be implemented
 against the VM and performance tested before layering on
 the high level language.
 -----------------------------------------------------------
 Core concepts (VM)
 -----------------------------------------------------------
 * Register based.
 * Simple structure to call into native code.
 * Simple to call into script functions.
 * Compiled as modules, where each module has its own memory
   and state. So there can be many instances of the same
   module running at the same time.
 * Structures can be created, but they are POD types.
 * Members must be predefined (i.e. no Lua style runtime 
   structure changes), so member accesses are indices in the
   VM.
 * Global variables can exist but are local to the current
   running script (i.e. each instance has its own memory).
 * Global variables are predefined and so can be indexed at
   runtime.
 * Local variables are added and removed based on scope.
************************************************************/
#include "vm.h"

void vm_test();