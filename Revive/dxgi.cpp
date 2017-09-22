#include <Windows.h>
#include <dxgitype.h>

typedef HRESULT(__stdcall* _CreateDXGIFactory)(REFIID, void**);
typedef HRESULT(__stdcall* _CreateDXGIFactory2)(UINT, REFIID, void**);

HMODULE GetDXGIModule()
{
    static HMODULE module = NULL;
    if (module)
        return module;

    wchar_t path[MAX_PATH];
    GetWindowsDirectory(path, MAX_PATH);
    wcsncat_s(path, L"\\System32\\dxgi.dll", MAX_PATH);
    module = LoadLibrary(path);
    return module;
}

HRESULT WINAPI CreateDXGIFactory(REFIID riid, _Out_ void **ppFactory)
{
    static _CreateDXGIFactory func = NULL;
    if (!func)
        func = (_CreateDXGIFactory)GetProcAddress(GetDXGIModule(), "CreateDXGIFactory");

    return func(riid, ppFactory);
}

HRESULT WINAPI CreateDXGIFactory1(REFIID riid, _Out_ void **ppFactory)
{
    static _CreateDXGIFactory func = NULL;
    if (!func)
        func = (_CreateDXGIFactory)GetProcAddress(GetDXGIModule(), "CreateDXGIFactory1");

    return func(riid, ppFactory);
}

HRESULT WINAPI CreateDXGIFactory2(UINT flags, REFIID riid, _Out_ void **ppFactory)
{
    static _CreateDXGIFactory2 func = NULL;
    if (!func)
        func = (_CreateDXGIFactory2)GetProcAddress(GetDXGIModule(), "CreateDXGIFactory2");

    return func(flags, riid, ppFactory);
}
