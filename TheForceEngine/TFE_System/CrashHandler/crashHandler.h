#pragma once
#include <TFE_System/types.h>

namespace TFE_CrashHandler  
{
    // Sets exception handlers that work on per-process basis
    void setProcessExceptionHandlers();

    // Installs C++ exception handlers that function on per-thread basis
    void setThreadExceptionHandlers();
};


