#include "stdafx.h"
#include <timeapi.h>
#pragma comment(lib, "winmm")



#include <GWCA/Utilities/Debug.h>
#include <GWCA/Utilities/Scanner.h>

#include <GWCA/Managers/MemoryMgr.h>

// Used to get precise skill recharge times.
DWORD* GW::MemoryMgr::SkillTimerPtr = NULL;

uintptr_t GW::MemoryMgr::WinHandlePtr = NULL;

uintptr_t GW::MemoryMgr::GetPersonalDirPtr = NULL;

namespace {
    typedef uint32_t(__cdecl* GetGWVersion_pt)(void);
    typedef void*(__stdcall* MemAllocHelper_pt)(size_t,uint8_t,const char*,int);
    typedef void*(__stdcall* MemReallocHelper_pt)(void*,size_t,uint8_t,const char*,int);
    typedef void*(__fastcall* MemFree_pt)(void*);

    GetGWVersion_pt GetGWVersion_Func = nullptr;
    MemAllocHelper_pt MemAllocHelper_Func = nullptr;
    MemReallocHelper_pt MemReallocHelper_Func = nullptr;
    MemFree_pt MemFree_Func = nullptr;
}

bool GW::MemoryMgr::Scan() {
    Scanner::Initialize();

    uintptr_t address;

    // Skill timer to use for exact effect times.
    address = Scanner::Find("\x83\xCA\x01\x89\x15\x00\x00\x00\x00\xFF\xD6\x8B", "xxxxx????xxx", +5);
    if (address)
        SkillTimerPtr = *(DWORD**)address;

    address = Scanner::Find("\x83\xC4\x04\x83\x3D\x00\x00\x00\x00\x00\x75\x31", "xxxxx????xxx", -0xC);
    if (address)
        WinHandlePtr = *(uintptr_t *)address;

    GetPersonalDirPtr = Scanner::Find("\x75\x2E\x6A\x01\x6A\x05\x56\x6A\x00", "xxxxxxxxx", -0x53);

    address = Scanner::Find("\x6A\x00\x68\x00\x00\x01\x00\x89", "xxxxxxxx", 0x42);
    GetGWVersion_Func = (GetGWVersion_pt)Scanner::FunctionFromNearCall(address);

    address = Scanner::Find("\x57\xe8\x00\x00\x00\x00\x8b\xf0\x83\xc4\x04\x85\xf6\x75\x00\x8b", "xx????xxxxxxxx?x", -0x21);
    MemAllocHelper_Func = (MemAllocHelper_pt)address;

    address = Scanner::Find("\x56\xe8\x00\x00\x00\x00\x8b\xf8\x83\xc4\x04\x85\xff\x75\x00\x8b", "xx????xxxxxxxx?x", -0x41);
    MemReallocHelper_Func = (MemReallocHelper_pt)address;

    // MemFree is difficult to scan for because its implementation is small and its usages are very
    // repetitive, formulaic code.  Instead, scanning for when a function wrapping free is bound to a field
    // of Base::ExeAPI::s_api
    address = Scanner::FindAssertion("p:\\code\\base\\os\\win32\\exe\\exeio.cpp", "!s_api.initialize", 0x14);
    address = Scanner::FunctionFromNearCall(address);
    MemFree_Func = (MemFree_pt)*(uintptr_t*)(address + 0x39);

    GWCA_INFO("[SCAN] SkillTimerPtr = %08X", SkillTimerPtr);
    GWCA_INFO("[SCAN] WinHandlePtr = %08X", WinHandlePtr);
    GWCA_INFO("[SCAN] GetPersonalDirPtr = %08X", GetPersonalDirPtr);
    GWCA_INFO("[SCAN] GetGWVersion_Func = %p, %d", GetGWVersion_Func, GetGWVersion());
    GWCA_INFO("[SCAN] MemAllocHelper_Func = %p", MemAllocHelper_Func);
    GWCA_INFO("[SCAN] MemReallocHelper_Func = %p", MemReallocHelper_Func);
    GWCA_INFO("[SCAN] MemFree_Func = %p", MemFree_Func);

#ifdef _DEBUG
    GWCA_ASSERT(SkillTimerPtr);
    GWCA_ASSERT(WinHandlePtr);
    GWCA_ASSERT(GetPersonalDirPtr);
    GWCA_ASSERT(GetGWVersion_Func);
    GWCA_ASSERT(MemAllocHelper_Func);
    GWCA_ASSERT(MemReallocHelper_Func);
    GWCA_ASSERT(MemFree_Func);
#endif

    return SkillTimerPtr && WinHandlePtr && GetPersonalDirPtr && GetGWVersion_Func;
}

DWORD GW::MemoryMgr::GetSkillTimer()
{
    return timeGetTime() + *SkillTimerPtr;
}

uint32_t GW::MemoryMgr::GetGWVersion() {
    return GetGWVersion_Func ? GetGWVersion_Func() : 0;
}

void* GW::MemoryMgr::MemAlloc(size_t size) {
    if (!MemAllocHelper_Func) {
        return nullptr;
    }

    // Parameter 2: flags.  (flags & 2) == zero memory after allocation
    return MemAllocHelper_Func(size, 0, __FILE__, __LINE__);
}

void* GW::MemoryMgr::MemRealloc(void* buf, size_t new_size) {
    if (!MemReallocHelper_Func) {
        return nullptr;
    }

    // Parameter 3: flags.
    //   (flags & 1) == realloc in place and sometimes compact heap.  Only applies if buf is non-null
    //   (flags & 2) == zero memory after allocation.  Only applies if buf is null
    return MemReallocHelper_Func(buf, new_size, 0, __FILE__, __LINE__);
}

void GW::MemoryMgr::MemFree(void* buf) {
    if (MemFree_Func) {
        MemFree_Func(buf);
    }
}
