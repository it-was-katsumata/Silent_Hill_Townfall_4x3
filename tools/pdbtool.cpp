// pdbtool.cpp - Query the shipped Townfall PDB via the DIA SDK.
//
// Usage:
//   pdbtool.exe <pdb> layout <TypeName> [maxDepth]   member offsets (+ bitfields)
//   pdbtool.exe <pdb> func <pattern>                 function RVAs (wildcards * ?)
//   pdbtool.exe <pdb> public <pattern>               public symbol RVAs (undecorated match)
//   pdbtool.exe <pdb> dpublic <name>                 public symbol RVA (exact decorated name)
//   pdbtool.exe <pdb> vfuncs <TypeName>              virtual functions with vtable slot index
//
// Same approach as dia_layout.cpp, but msdia140.dll path is taken from the
// DIA_DLL environment variable so it works with any VS install.
//
// Townfall 4:3 fork, by it-was-katsumata (2026), based on Infinest's
// tools/dia_layout.cpp. GPL-3.0, see LICENSE.
#include <windows.h>
#include <dia2.h>
#include <diacreate.h>

#include <cstdio>
#include <cwchar>

namespace {

IDiaSession* gSession = nullptr;

void DumpType(IDiaSymbol* type, int depth, int maxDepth) {
    DWORD tag = 0;
    type->get_symTag(&tag);
    while (tag == SymTagTypedef || tag == SymTagCustomType || tag == SymTagPointerType ||
           tag == SymTagArrayType) {
        IDiaSymbol* next = nullptr;
        if (FAILED(type->get_type(&next)) || !next) return;
        type = next;
        type->get_symTag(&tag);
    }
    if (tag != SymTagUDT) return;

    ULONGLONG len = 0;
    type->get_length(&len);
    if (depth == 0) wprintf(L"(type len 0x%llX)\n", len);

    IDiaEnumSymbols* children = nullptr;
    if (FAILED(type->findChildren(SymTagNull, nullptr, nsNone, &children)) || !children) return;
    IDiaSymbol* child = nullptr;
    ULONG got = 0;
    while (SUCCEEDED(children->Next(1, &child, &got)) && got == 1) {
        DWORD ctag = 0;
        child->get_symTag(&ctag);
        BSTR name = nullptr;
        child->get_name(&name);
        if (ctag == SymTagData) {
            DWORD loc = 0;
            child->get_locationType(&loc);
            if (loc == LocIsThisRel || loc == LocIsBitField) {
                LONG off = 0;
                child->get_offset(&off);
                ULONGLONG clen = 0;
                child->get_length(&clen);
                if (loc == LocIsBitField) {
                    DWORD bit = 0;
                    child->get_bitPosition(&bit);
                    wprintf(L"%*s0x%04X bit %lu (%llu bits) %ls\n", depth * 2 + 2, L"", (ULONG)off,
                            bit, clen, name ? name : L"?");
                } else {
                    IDiaSymbol* ct = nullptr;
                    BSTR tname = nullptr;
                    if (SUCCEEDED(child->get_type(&ct)) && ct) ct->get_name(&tname);
                    wprintf(L"%*s0x%04X (len 0x%llX) %ls  : %ls\n", depth * 2 + 2, L"", (ULONG)off,
                            clen, name ? name : L"?", tname ? tname : L"");
                    if (tname) SysFreeString(tname);
                }
            }
        } else if (ctag == SymTagBaseClass) {
            LONG off = 0;
            child->get_offset(&off);
            wprintf(L"%*s[base @0x%04X] %ls\n", depth * 2 + 2, L"", (ULONG)off, name ? name : L"?");
            if (depth + 1 < maxDepth) DumpType(child, depth + 1, maxDepth);
        }
        if (name) SysFreeString(name);
        child->Release();
    }
    children->Release();
}

IDiaSymbol* FindUdt(IDiaSymbol* global, const wchar_t* name) {
    IDiaEnumSymbols* e = nullptr;
    if (FAILED(global->findChildren(SymTagUDT, name, nsCaseSensitive, &e)) || !e) return nullptr;
    IDiaSymbol* sym = nullptr;
    ULONG got = 0;
    // Prefer a definition with a non-zero length (forward decls have length 0).
    while (SUCCEEDED(e->Next(1, &sym, &got)) && got == 1) {
        ULONGLONG len = 0;
        sym->get_length(&len);
        if (len) {
            e->Release();
            return sym;
        }
        sym->Release();
    }
    e->Release();
    return nullptr;
}

void ListSymbols(IDiaSymbol* global, enum SymTagEnum tag, const wchar_t* pattern, DWORD flags) {
    IDiaEnumSymbols* e = nullptr;
    if (FAILED(global->findChildrenEx(tag, pattern, flags, &e)) || !e) {
        wprintf(L"(search failed)\n");
        return;
    }
    IDiaSymbol* sym = nullptr;
    ULONG got = 0;
    int n = 0;
    while (SUCCEEDED(e->Next(1, &sym, &got)) && got == 1 && n < 200) {
        DWORD rva = 0;
        sym->get_relativeVirtualAddress(&rva);
        ULONGLONG len = 0;
        sym->get_length(&len);
        BSTR name = nullptr;
        if (tag == SymTagPublicSymbol) sym->get_undecoratedName(&name);
        if (!name) sym->get_name(&name);
        wprintf(L"0x%08lX (len 0x%llX)  %ls\n", rva, len, name ? name : L"?");
        if (name) SysFreeString(name);
        sym->Release();
        ++n;
    }
    e->Release();
    if (n == 0) wprintf(L"(no matches)\n");
}

void ListVirtuals(IDiaSymbol* udt) {
    IDiaEnumSymbols* e = nullptr;
    if (FAILED(udt->findChildren(SymTagFunction, nullptr, nsNone, &e)) || !e) return;
    IDiaSymbol* f = nullptr;
    ULONG got = 0;
    while (SUCCEEDED(e->Next(1, &f, &got)) && got == 1) {
        BOOL isVirtual = FALSE;
        f->get_virtual(&isVirtual);
        if (isVirtual) {
            DWORD voff = 0;
            f->get_virtualBaseOffset(&voff);
            DWORD rva = 0;
            f->get_relativeVirtualAddress(&rva);
            BSTR name = nullptr;
            f->get_name(&name);
            wprintf(L"slot %3lu (vtoff 0x%03lX) rva 0x%08lX  %ls\n", voff / 8, voff, rva,
                    name ? name : L"?");
            if (name) SysFreeString(name);
        }
        f->Release();
    }
    e->Release();
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc < 4) {
        wprintf(L"usage: pdbtool <pdb> layout|func|public|vfuncs <name> [depth]\n");
        return 1;
    }
    CoInitialize(nullptr);

    wchar_t diaPath[MAX_PATH];
    if (!GetEnvironmentVariableW(L"DIA_DLL", diaPath, MAX_PATH)) {
        wprintf(L"set DIA_DLL to the full path of msdia140.dll\n");
        return 1;
    }
    HMODULE dia = LoadLibraryW(diaPath);
    if (!dia) {
        wprintf(L"msdia140.dll load failed %lu\n", GetLastError());
        return 1;
    }
    using DllGetClassObjectFn = HRESULT(WINAPI*)(REFCLSID, REFIID, void**);
    auto getClass = reinterpret_cast<DllGetClassObjectFn>(GetProcAddress(dia, "DllGetClassObject"));
    IClassFactory* factory = nullptr;
    if (!getClass ||
        FAILED(getClass(CLSID_DiaSource, IID_IClassFactory, reinterpret_cast<void**>(&factory)))) {
        wprintf(L"class factory failed\n");
        return 1;
    }
    IDiaDataSource* source = nullptr;
    HRESULT hr = factory->CreateInstance(nullptr, IID_IDiaDataSource, reinterpret_cast<void**>(&source));
    factory->Release();
    if (FAILED(hr) || !source || FAILED(source->loadDataFromPdb(argv[1]))) {
        wprintf(L"loadDataFromPdb failed\n");
        return 1;
    }
    if (FAILED(source->openSession(&gSession)) || !gSession) return 1;
    IDiaSymbol* global = nullptr;
    gSession->get_globalScope(&global);

    const wchar_t* mode = argv[2];
    const wchar_t* name = argv[3];
    if (wcscmp(mode, L"layout") == 0) {
        IDiaSymbol* udt = FindUdt(global, name);
        if (!udt) {
            wprintf(L"type not found: %ls\n", name);
        } else {
            wprintf(L"== %ls ==\n", name);
            DumpType(udt, 0, argc > 4 ? _wtoi(argv[4]) : 1);
        }
    } else if (wcscmp(mode, L"func") == 0) {
        ListSymbols(global, SymTagFunction, name, nsfRegularExpression | nsfCaseSensitive);
    } else if (wcscmp(mode, L"public") == 0) {
        ListSymbols(global, SymTagPublicSymbol, name,
                    nsfRegularExpression | nsfCaseSensitive | nsfUndecoratedName);
    } else if (wcscmp(mode, L"dpublic") == 0) {
        // match against the decorated name, e.g. ??_7SOverlay@@6B@
        ListSymbols(global, SymTagPublicSymbol, name, nsfCaseSensitive);
    } else if (wcscmp(mode, L"vfuncs") == 0) {
        IDiaSymbol* udt = FindUdt(global, name);
        if (udt) ListVirtuals(udt);
        else wprintf(L"type not found: %ls\n", name);
    }
    global->Release();
    gSession->Release();
    source->Release();
    CoUninitialize();
    return 0;
}
