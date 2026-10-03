#include <windows.h>
#include <cstdio>

int main()
{
    static wchar_t text[] = L"GSM LunaHook smoke test \u65e5\u672c\u8a9e";
    printf("%lu %p\n", GetCurrentProcessId(), static_cast<void *>(text));
    fflush(stdout);
    // Only the test harness launches and attaches to this process.
    getchar();
    return 0;
}
