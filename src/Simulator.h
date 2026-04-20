#pragma once
#include "pch.h"
#include "bridgemain.h"
#include "_plugins.h"
#include "_scriptapi_module.h"
#include "_scriptapi_memory.h"
#include "_scriptapi_debug.h"
#include "unicorn/unicorn.h"

class UnicornSimulator {
public:
    enum class WaitState { None, Rdtsc, Syscall, ErrorResume, ApiDone };

    UnicornSimulator();
    ~UnicornSimulator();

    void trigger();
    void shutdown();

    bool      isActive()   const { return m_active; }
    void      setActive(bool v)  { m_active = v; }
    WaitState waitState()  const { return m_waitState; }
    duint     bpAddress()  const { return m_bpAddress; }

    void onBreakpoint(duint addr);

private:
    static DWORD WINAPI workerEntry(LPVOID param);
    static void         hookCode(uc_engine* uc, uint64_t addr, uint32_t size, void* ud);
    static bool         hookMemUnmapped(uc_engine* uc, uc_mem_type type, uint64_t addr, int size, int64_t value, void* ud);

    void run();
    void mapAllMemory(uc_engine* uc);
    bool syncRegisters(uc_engine* uc, uint64_t& outRip);
    void handleStop(uc_engine* uc, uint64_t stopRip);
    void setHwBp(duint addr, WaitState state);

    enum class StopReason { None, Rdtsc, Syscall, OutOfModule, Error };

    HANDLE         m_event;
    HANDLE         m_thread;
    volatile BOOL  m_quit;
    bool           m_active;
    WaitState      m_waitState;
    duint          m_bpAddress;
    StopReason     m_stopReason;
    duint          m_moduleBase;
    duint          m_moduleSize;
};
