#pragma once
#include "../../../../pch.h"

inline thread_local int GCrashGuardDepth = 0;

class FCrashReporter
{
public:
    static void Register();

    
    
    static void EnterGuardedSection();
    static void LeaveGuardedSection();

    static int GetRecoveredNullCallCount();

    
    
    
    static void SetBreadcrumb(const char* Where);
    static const char* GetBreadcrumb();

    struct FScopedGuard
    {
        FScopedGuard()
        {
            FCrashReporter::EnterGuardedSection();
        }
        ~FScopedGuard()
        {
            FCrashReporter::LeaveGuardedSection();
        }
    };
};
