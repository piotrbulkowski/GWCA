#pragma once

namespace GW {

    struct MemoryMgr {

        // Skill timer for effects.
        static DWORD* SkillTimerPtr;

        static uintptr_t WinHandlePtr;

        static uintptr_t GetPersonalDirPtr;

        static uint32_t GetGWVersion();

        // Basics
        static bool Scan();

        static DWORD GetSkillTimer();

        static HWND GetGWWindowHandle() {
            return *reinterpret_cast<HWND*>(WinHandlePtr);
        }

        // You probably don't want to use these functions.  These are for allocating
        // memory on the Guild Wars game heap, rather than your own heap.  Memory allocated with
        // these functions cannot be used with RAII and must be manually freed.  USE AT YOUR OWN RISK.
        static void* MemAlloc(size_t size);
        static void* MemRealloc(void* buf, size_t newSize);
        static void MemFree(void* buf);
    };
}
