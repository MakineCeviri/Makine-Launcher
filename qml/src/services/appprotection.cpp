/**
 * @file appprotection.cpp
 * @brief Anti-reverse-engineering and tamper detection implementation
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Release builds (NDEBUG): 8 anti-debug checks, periodic re-checks,
 * and delayed exit. Extended checks (RE tools, VM, DLL) in separate module.
 * Debug builds: every function is a harmless no-op.
 */

#include "appprotection.h"

#ifdef NDEBUG // ==================== RELEASE BUILD ====================

#include "obfstring.h"

#include <atomic>
#include <cstdlib>

#include <QCoreApplication>
#include <QGuiApplication>
#include <QTimer>

#include <windows.h>

// ───────── state ─────────
static std::atomic<bool> g_flagged{false};
static std::atomic<int>  g_checkFailCount{0};

// ───────── decoy strings (mislead reversers) ─────────
// These are never referenced in real logic — they exist only to appear
// in memory dumps and waste the reverser's time.
[[maybe_unused]] static volatile const char* g_decoys[] = {
    "https://api.makineceviri.org/v2/license/check",
    "X-HWID-Signature",
    "AES-256-CBC",
    "remote_kill_endpoint",
    "HKEY_LOCAL_MACHINE\\SOFTWARE\\MakineCeviri\\License",
    "jwt_validation_server",
    "telemetry_heartbeat",
    "drm_challenge_response",
    "device_fingerprint_v3",
    // Reverser easter eggs
    "Buraya kadar geldiysen tebrikler, ama daha yolun var :)",
    "Hala mi ugrasiyorsun? Cay koy gel rahatla",
    "GGWP - Makine Protection v3.7.2",
};

// ───────── helpers ─────────
namespace {

// Dynamic-resolve a function from a DLL, hiding the import from the IAT
template <typename FuncPtr>
FuncPtr resolveApi(const std::string& dll, const std::string& func)
{
    HMODULE mod = GetModuleHandleA(dll.c_str());
    if (!mod) mod = LoadLibraryExA(dll.c_str(), NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!mod) return nullptr;
    return reinterpret_cast<FuncPtr>(GetProcAddress(mod, func.c_str()));
}

// Anti-disassembly: a short inline-asm island that confuses linear
// disassemblers (they interpret the 0xE8 byte as a CALL opcode).
// Recursive-descent disassemblers (IDA/Ghidra) handle this fine,
// but simpler tools and AI hex-dump analysis will stumble.
#if defined(__GNUC__) || defined(__clang__)
#define ANTI_DISASM()                   \
    __asm__ volatile (                  \
        "jmp 1f\n\t"                    \
        ".byte 0xE8\n"                  \
        "1:\n\t"                        \
        ::: "memory"                    \
    )
#else
#define ANTI_DISASM() ((void)0)
#endif

// ───────── individual checks ─────────

// 1. IsDebuggerPresent (simplest, but still catches casual attachers)
bool checkIsDebuggerPresent()
{
    return IsDebuggerPresent() != 0;
}

// 2. CheckRemoteDebuggerPresent (catches remote debuggers like WinDbg)
bool checkRemoteDebugger()
{
    BOOL present = FALSE;
    CheckRemoteDebuggerPresent(GetCurrentProcess(), &present);
    return present != 0;
}

// 3. NtQueryInformationProcess — DebugPort (ProcessDebugPort = 7)
bool checkDebugPort()
{
    using NtQIP = LONG(NTAPI*)(HANDLE, ULONG, PVOID, ULONG, PULONG);
    auto fn = resolveApi<NtQIP>(OBF("ntdll.dll"), OBF("NtQueryInformationProcess"));
    if (!fn) return false;

    ULONG_PTR debugPort = 0;
    LONG status = fn(GetCurrentProcess(), 7, &debugPort, sizeof(debugPort), nullptr);
    return status == 0 && debugPort != 0;
}

// 4. NtQueryInformationProcess — DebugFlags (ProcessDebugFlags = 0x1F)
bool checkDebugFlags()
{
    using NtQIP = LONG(NTAPI*)(HANDLE, ULONG, PVOID, ULONG, PULONG);
    auto fn = resolveApi<NtQIP>(OBF("ntdll.dll"), OBF("NtQueryInformationProcess"));
    if (!fn) return false;

    ULONG debugFlags = 1;
    LONG status = fn(GetCurrentProcess(), 0x1F, &debugFlags, sizeof(debugFlags), nullptr);
    // DebugFlags == 0 means process IS being debugged
    return status == 0 && debugFlags == 0;
}

// 5. NtGlobalFlag — heap debug markers (FLG_HEAP_ENABLE_TAIL_CHECK etc.)
bool checkNtGlobalFlag()
{
    using RtlGetNtGlobalFlags_t = ULONG(NTAPI*)();
    auto fn = resolveApi<RtlGetNtGlobalFlags_t>(OBF("ntdll.dll"), OBF("RtlGetNtGlobalFlags"));
    if (!fn) return false;

    constexpr ULONG kHeapDebugFlags = 0x70; // FLG_HEAP_ENABLE_TAIL_CHECK | FREE_CHECK | VALIDATE_PARAMS
    return (fn() & kHeapDebugFlags) != 0;
}

// 6. Timing check — single-step debugging makes code run orders of magnitude slower
bool checkTiming()
{
    ANTI_DISASM();
    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);

    // Lightweight work that takes <1 ms normally
    volatile int dummy = 0;
    for (int i = 0; i < 1000; ++i) dummy += i;

    QueryPerformanceCounter(&end);
    double ms = (double)(end.QuadPart - start.QuadPart) / (double)freq.QuadPart * 1000.0;
    return ms > 200.0; // stepping through would blow past 200 ms easily
}

// 7. Hardware breakpoints — DR0-DR3 registers via GetThreadContext
bool checkHardwareBreakpoints()
{
    CONTEXT ctx{};
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (!GetThreadContext(GetCurrentThread(), &ctx))
        return false;
    return (ctx.Dr0 | ctx.Dr1 | ctx.Dr2 | ctx.Dr3) != 0;
}

// 8. API hook detection — check if IsDebuggerPresent has been patched
bool checkApiHook()
{
    auto fn = resolveApi<FARPROC>(OBF("kernel32.dll"), OBF("IsDebuggerPresent"));
    if (!fn) return false;

    // First byte of the real function should be a common prologue opcode
    // (0x64 = fs: prefix for PEB access, 0x48 = REX.W prefix on x64).
    // A JMP hook starts with 0xE9 or 0xFF.
    unsigned char first = *reinterpret_cast<unsigned char*>(fn);
    return first == 0xE9 || first == 0xFF;
}

// ───────── run all checks ─────────
void runAllChecks()
{
    ANTI_DISASM();

    bool hit = false;
    // Each check is independent — if one is NOP'd, the others still work
    hit |= checkIsDebuggerPresent();    // 1
    hit |= checkRemoteDebugger();       // 2
    hit |= checkDebugPort();            // 3
    hit |= checkDebugFlags();           // 4
    hit |= checkNtGlobalFlag();         // 5
    hit |= checkTiming();               // 6
    hit |= checkHardwareBreakpoints();  // 7
    hit |= checkApiHook();              // 8

    if (hit) {
        g_flagged.store(true, std::memory_order_relaxed);
        g_checkFailCount.fetch_add(1, std::memory_order_relaxed);
    }
}

} // anonymous namespace

// ───────── public API ─────────

void makine::protection::initialize()
{
    ANTI_DISASM();
    runAllChecks();
}

void makine::protection::schedulePeriodicChecks()
{
    // Re-run checks every 45–90 seconds (varies by __LINE__ seed to
    // make the interval harder to predict from static analysis)
    constexpr int kBaseMs = 45000;
    constexpr int kJitterMs = 45000;
    int intervalMs = kBaseMs + static_cast<int>(__LINE__ * 137u % kJitterMs);

    auto* timer = new QTimer(qApp);
    timer->setTimerType(Qt::VeryCoarseTimer);
    QObject::connect(timer, &QTimer::timeout, []() {
        runAllChecks();
        if (g_checkFailCount.load(std::memory_order_relaxed) > 5) {
            QCoreApplication::exit(0);
        }
    });

    // Pause checks when app is inactive (minimized/tray) to save resources
    QObject::connect(qApp, &QGuiApplication::applicationStateChanged, timer,
        [timer, intervalMs](Qt::ApplicationState state) {
            if (state == Qt::ApplicationActive) {
                if (!timer->isActive()) timer->start(intervalMs);
            } else {
                timer->stop();
            }
        });

    timer->start(intervalMs);
}

bool makine::protection::isCompromised()
{
    return g_flagged.load(std::memory_order_relaxed);
}

void makine::protection::onViolation(int site)
{
    // Delayed exit: 10–60 seconds — makes it very hard to correlate
    // the trigger with the actual check that detected the debugger
    int delayMs = 10000 + static_cast<int>(static_cast<unsigned>(site) * 97u % 50000u);

    // Cap violations — after 5, exit immediately
    if (g_checkFailCount.load(std::memory_order_relaxed) > 5) {
        QCoreApplication::exit(0);
        return;
    }

    QTimer::singleShot(delayMs, qApp, []() {
        QCoreApplication::exit(0);
    });
}

#else // ==================== DEBUG BUILD ====================

// All functions are no-ops — zero overhead, no false positives
void makine::protection::initialize() {}
void makine::protection::schedulePeriodicChecks() {}
bool makine::protection::isCompromised() { return false; }
void makine::protection::onViolation(int) {}

#endif
