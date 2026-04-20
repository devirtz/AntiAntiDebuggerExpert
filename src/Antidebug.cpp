#include "pch.h"
#include "Simulator.h"

static int               g_pluginHandle;
static int               g_hMenu;
static UnicornSimulator* g_sim;

#define MENU_UNICORN_ID 1

extern "C" __declspec(dllexport) void CBBREAKPOINT(CBTYPE, PLUG_CB_BREAKPOINT* info) {
    if (!g_sim || !g_sim->isActive()) return;
    if (!info || info->breakpoint->type != bp_hardware) return;

    auto state = g_sim->waitState();
    if (state == UnicornSimulator::WaitState::None) return;

    duint addr = info->breakpoint->addr;
    if (addr != g_sim->bpAddress()) return;

    g_sim->onBreakpoint(addr);
}

extern "C" __declspec(dllexport) void CBMENUENTRY(CBTYPE, PLUG_CB_MENUENTRY* info) {
    if (info->hEntry == MENU_UNICORN_ID) {
        g_sim->setActive(true);
        g_sim->trigger();
    }
}

extern "C" __declspec(dllexport) bool pluginit(PLUG_INITSTRUCT* s) {
    s->pluginVersion = 1;
    s->sdkVersion    = PLUG_SDKVERSION;
    strcpy_s(s->pluginName, "AntiAntiDebugPluginExpert");
    g_pluginHandle = s->pluginHandle;
    return true;
}

extern "C" __declspec(dllexport) void plugsetup(PLUG_SETUPSTRUCT* s) {
    g_hMenu = s->hMenu;
    _plugin_menuaddentry(g_hMenu, MENU_UNICORN_ID, "sys_trace");
    g_sim = new UnicornSimulator();
    _plugin_logputs("[AntiAntiDebugPluginExpert] started");
}

extern "C" __declspec(dllexport) bool plugstop() {
    delete g_sim;
    g_sim = nullptr;
    _plugin_logputs("[AntiAntiDebugPluginExpert] stopped");
    return true;
}

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID) {
    return TRUE;
}
