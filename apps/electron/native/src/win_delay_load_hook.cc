#ifdef _WIN32

#include <windows.h>
#include <delayimp.h>
#include <string.h>

extern "C" FARPROC WINAPI electron_delay_load_hook(unsigned notification, PDelayLoadInfo info) {
  if (notification == dliNotePreLoadLibrary && info && info->szDll) {
    if (_stricmp(info->szDll, "node.exe") == 0 || _stricmp(info->szDll, "node.dll") == 0) {
      return reinterpret_cast<FARPROC>(GetModuleHandleW(nullptr));
    }
  }

  return nullptr;
}

decltype(__pfnDliNotifyHook2) __pfnDliNotifyHook2 = electron_delay_load_hook;

#endif
