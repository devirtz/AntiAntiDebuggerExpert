#include "pch.h"
#include "Simulator.h"

UnicornSimulator::UnicornSimulator()
    : m_event(nullptr), m_thread(nullptr), m_quit(FALSE)
    , m_active(false), m_waitState(WaitState::None), m_bpAddress(0)
    , m_stopReason(StopReason::None), m_moduleBase(0), m_moduleSize(0)
{
    m_event  = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    m_thread = CreateThread(nullptr, 0, workerEntry, this, 0, nullptr);
}

UnicornSimulator::~UnicornSimulator() {
    shutdown();
}

void UnicornSimulator::trigger() {
    SetEvent(m_event);
}

void UnicornSimulator::shutdown() {
    if (!m_event) return;
    m_quit = TRUE;
    SetEvent(m_event);
    if (m_thread) {
        WaitForSingleObject(m_thread, INFINITE);
        CloseHandle(m_thread);
        m_thread = nullptr;
    }
    CloseHandle(m_event);
    m_event = nullptr;
}

void UnicornSimulator::onBreakpoint(duint addr) {
    Script::Debug::DeleteHardwareBreakpoint(addr);
    bool resume = (m_waitState == WaitState::Rdtsc || m_waitState == WaitState::ErrorResume);
    m_waitState = WaitState::None;
    m_bpAddress = 0;
    if (resume)
        SetEvent(m_event);
}

DWORD WINAPI UnicornSimulator::workerEntry(LPVOID param) {
    static_cast<UnicornSimulator*>(param)->run();
    return 0;
}

void UnicornSimulator::hookCode(uc_engine* uc, uint64_t addr, uint32_t size, void* ud) {
    auto* self = static_cast<UnicornSimulator*>(ud);
    WORD op = 0;
    uc_mem_read(uc, addr, &op, 2);

    if (op == 0x310f) {  // rdtsc: 0F 31
        _plugin_logprintf("[AntiAntiDebugPluginExpert] rdtsc at 0x%llX\n", addr);
        self->m_stopReason = StopReason::Rdtsc;
        self->m_bpAddress  = addr + size;
        uc_emu_stop(uc);
        return;
    }
    if (op == 0x050f) {  // syscall: 0F 05
        _plugin_logprintf("[AntiAntiDebugPluginExpert] syscall at 0x%llX\n", addr);
        self->m_stopReason = StopReason::Syscall;
        uc_emu_stop(uc);
        return;
    }
    if (addr < self->m_moduleBase || addr >= self->m_moduleBase + self->m_moduleSize) {
        _plugin_logprintf("[AntiAntiDebugPluginExpert] out-of-module at 0x%llX\n", addr);
        self->m_stopReason = StopReason::OutOfModule;
        uc_emu_stop(uc);
    }
}

bool UnicornSimulator::hookMemUnmapped(uc_engine* uc, uc_mem_type type, uint64_t addr, int size, int64_t, void* ud) {
    auto* self = static_cast<UnicornSimulator*>(ud);
    uint64_t rip = 0;
    uc_reg_read(uc, UC_X86_REG_RIP, &rip);
    _plugin_logprintf("[AntiAntiDebugPluginExpert] MEM_UNMAPPED RIP=0x%llX addr=0x%llX type=%d size=%d\n", rip, addr, type, size);

    duint page = (duint)addr & ~(duint)0xFFF;
    if (!Script::Memory::IsValidPtr(page)) {
        _plugin_logprintf("[AntiAntiDebugPluginExpert] invalid pointer 0x%llX\n", page);
        uc_emu_stop(uc);
        return false;
    }

    uc_err err = uc_mem_map(uc, page, 0x1000, UC_PROT_ALL);
    if (err != UC_ERR_OK && err != UC_ERR_MAP) {
        _plugin_logprintf("[AntiAntiDebugPluginExpert] mem_map failed: %s\n", uc_strerror(err));
        return false;
    }

    (void)self;
    unsigned char buf[0x1000] = {};
    if (Script::Memory::Read(page, buf, 0x1000, nullptr)) {
        uc_mem_write(uc, page, buf, 0x1000);
        return true;
    }
    return false;
}

void UnicornSimulator::mapAllMemory(uc_engine* uc) {
    MEMMAP mm = {};
    DbgMemMap(&mm);
    for (int i = 0; i < mm.count; ++i) {
        auto& p = mm.page[i];
        if (p.mbi.State != MEM_COMMIT) continue;
        duint  base  = (duint)p.mbi.BaseAddress;
        SIZE_T sz    = p.mbi.RegionSize;
        duint  aBase = base & ~(duint)0xFFF;
        duint  aSz   = (sz + 0xFFF) & ~(SIZE_T)0xFFF;
        uc_mem_map(uc, aBase, (size_t)aSz, UC_PROT_ALL);
        std::vector<uint8_t> buf(sz);
        if (Script::Memory::Read(base, buf.data(), sz, nullptr))
            uc_mem_write(uc, base, buf.data(), sz);
    }
}

bool UnicornSimulator::syncRegisters(uc_engine* uc, uint64_t& outRip) {
    REGDUMP_AVX512 rd = {};
    if (!DbgGetRegDumpEx(&rd, sizeof(rd))) {
        _plugin_logprintf("[AntiAntiDebugPluginExpert] failed to read registers\n");
        return false;
    }

    auto& r = rd.regcontext;
    uint64_t rflags = r.eflags & ~(uint64_t)0x100;  // clear trap flag

    struct { int reg; uint64_t val; } regs[] = {
        { UC_X86_REG_RAX,    r.cax    }, { UC_X86_REG_RBX, r.cbx },
        { UC_X86_REG_RCX,    r.ccx    }, { UC_X86_REG_RDX, r.cdx },
        { UC_X86_REG_RSI,    r.csi    }, { UC_X86_REG_RDI, r.cdi },
        { UC_X86_REG_RSP,    r.csp    }, { UC_X86_REG_RBP, r.cbp },
        { UC_X86_REG_RIP,    r.cip    }, { UC_X86_REG_RFLAGS, rflags },
        { UC_X86_REG_R8,     r.r8     }, { UC_X86_REG_R9,  r.r9  },
        { UC_X86_REG_R10,    r.r10    }, { UC_X86_REG_R11, r.r11 },
        { UC_X86_REG_R12,    r.r12    }, { UC_X86_REG_R13, r.r13 },
        { UC_X86_REG_R14,    r.r14    }, { UC_X86_REG_R15, r.r15 },
    };
    for (auto& [reg, val] : regs)
        uc_reg_write(uc, reg, &val);

    outRip = r.cip;
    return true;
}

void UnicornSimulator::setHwBp(duint addr, WaitState state) {
    m_bpAddress = addr;
    m_waitState = state;
    Script::Debug::SetHardwareBreakpoint(addr);
    DbgCmdExec("go");
}

void UnicornSimulator::handleStop(uc_engine* uc, uint64_t stopRip) {
    _plugin_logprintf("[AntiAntiDebugPluginExpert] stop at 0x%llX reason=%d\n", stopRip, (int)m_stopReason);

    switch (m_stopReason) {
    case StopReason::Rdtsc:
        setHwBp(m_bpAddress, WaitState::Rdtsc);
        break;

    case StopReason::Syscall:
        setHwBp(stopRip, WaitState::Syscall);
        break;

    case StopReason::OutOfModule:
        setHwBp(stopRip, WaitState::ApiDone);
        break;

    default: {
        BASIC_INSTRUCTION_INFO info = {};
        DbgDisasmFastAt(stopRip, &info);
        setHwBp(stopRip + info.size, WaitState::ErrorResume);
        break;
    }
    }
}

void UnicornSimulator::run() {
    while (!m_quit) {
        WaitForSingleObject(m_event, INFINITE);
        if (m_quit) break;

        uc_engine* uc = nullptr;
        if (uc_open(UC_ARCH_X86, UC_MODE_64, &uc) != UC_ERR_OK) {
            _plugin_logprintf("[AntiAntiDebugPluginExpert] uc_open failed\n");
            continue;
        }

        m_moduleBase = Script::Module::GetMainModuleBase();
        m_moduleSize = Script::Module::SizeFromAddr(m_moduleBase);
        mapAllMemory(uc);

        uint64_t rip = 0;
        if (!syncRegisters(uc, rip)) {
            uc_close(uc);
            continue;
        }

        uc_hook hCode, hMem;
        uc_hook_add(uc, &hCode, UC_HOOK_CODE,         reinterpret_cast<void*>(hookCode),       this, 1, 0);
        uc_hook_add(uc, &hMem,  UC_HOOK_MEM_UNMAPPED, reinterpret_cast<void*>(hookMemUnmapped), this, 1, 0);

        _plugin_logprintf("[AntiAntiDebugPluginExpert] start at 0x%llX\n", rip);
        m_stopReason = StopReason::None;

        uc_err err = uc_emu_start(uc, rip, UINT64_MAX, 0, 0);
        if (err != UC_ERR_OK)
            _plugin_logprintf("[AntiAntiDebugPluginExpert] emu_start: %s\n", uc_strerror(err));

        uint64_t stopRip = 0;
        uc_reg_read(uc, UC_X86_REG_RIP, &stopRip);

        handleStop(uc, stopRip);
        uc_close(uc);
    }
}
