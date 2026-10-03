// UTF-16LE pipe interface retained for GameSentenceMiner and Sugoi Hook.
#include "host.h"
#include <io.h>
#include <fcntl.h>
#include <cerrno>
#include <climits>

namespace
{
    void Console(const std::wstring &message)
    {
        wprintf_s(L"[Console] %s\n", message.c_str());
        fflush(stdout);
    }

    bool ParseNumber(const std::wstring &text, unsigned long long &number)
    {
        if (text.empty() || text.find_first_not_of(L"0123456789") != std::wstring::npos)
            return false;
        errno = 0;
        wchar_t *end = nullptr;
        number = wcstoull(text.c_str(), &end, 10);
        return errno != ERANGE && end && *end == 0;
    }
}

int main()
{
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stdin), _O_U16TEXT);
    Host::Start(
        [](DWORD pid) { Console(L"Connected process " + std::to_wstring(pid)); },
        [](DWORD pid) { Console(L"Disconnected process " + std::to_wstring(pid)); },
        [](TextThread &thread)
        {
            wprintf_s(L"[Hook #%I64X created] Handle: %I64X\n", thread.handle, thread.handle);
            fflush(stdout);
        }, {},
        [](TextThread &thread, std::wstring &output)
        {
            wprintf_s(L"[#%I64X|%I64X:%I32X:%I64X:%I64X:%I64X:%s:%s] %s\n",
                thread.handle, thread.handle, thread.tp.processId, thread.tp.addr,
                thread.tp.ctx, thread.tp.ctx2, thread.name.c_str(),
                thread.hp.hookcode, output.c_str());
            fflush(stdout);
        },
        [](HOSTINFO type, const std::wstring &text)
        {
            if (type == HOSTINFO::Console)
                Console(text);
        }, {}, {}, {}, {});
    Console(L"Usage: {attach|detach|find|hookcode} -Pprocessid");

    std::wstring input;
    while (std::getline(std::wcin, input))
    {
        if (!input.empty() && input.back() == L'\r') input.pop_back();
        if (input.empty() || input == L"showall") continue; // All threads are always emitted.
        const auto separator = input.rfind(L" -P");
        unsigned long long processId;
        if (separator == std::wstring::npos || !ParseNumber(input.substr(separator + 3), processId)
            || processId == 0 || processId > MAXDWORD)
        {
            Console(L"Invalid command: expected a positive process ID after -P.");
            continue;
        }
        const auto command = input.substr(0, separator);
        const auto pid = static_cast<DWORD>(processId);
        if (_wcsicmp(command.c_str(), L"attach") == 0)
            Host::ConnectAndInjectProcess(pid);
        else if (_wcsicmp(command.c_str(), L"detach") == 0)
            Host::DetachProcess(pid);
        else if (_wcsicmp(command.c_str(), L"find") == 0)
        {
            SearchParam search;
            search.isjithook = false;
            search.length = 0;
            search.codepage = Host::defaultCodepage;
            Host::FindHooks(pid, search, [](const std::wstring &code, const std::wstring &text)
            {
                Console(code + L" => " + text);
            });
        }
        else if (!command.empty() && (command[0] == L'=' || command[0] == L'+' || command[0] == L'-'))
        {
            unsigned long long value;
            if (!ParseNumber(command.substr(1), value)
                || (command[0] != L'-' && value > INT_MAX))
            {
                Console(L"Invalid numeric setting.");
                continue;
            }
            if (command[0] == L'-') Host::RemoveHook(pid, value);
            else if (command[0] == L'=')
            {
                Host::defaultCodepage = static_cast<int>(value);
                Host::BroadCastCodePage();
            }
            else TextThread::flushDelay = static_cast<int>(value);
        }
        else if (!command.empty() && (command[0] == L'H' || command[0] == L'R' || command[0] == L'E'))
            Host::InsertHook(pid, command);
        else Console(L"Unknown command.");
    }
    // Host owns detached pipe threads; do not race their callbacks during static teardown.
    ExitProcess(0);
}
