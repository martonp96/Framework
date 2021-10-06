#include "exe_ldr.h"

#include <utils/hooking/hooking.h>

namespace Framework::Launcher::Loaders {
    ExecutableLoader::ExecutableLoader(const uint8_t *origBinary) {
        hook::set_base();

        m_origBinary = origBinary;
        m_loadLimit  = UINT_MAX;

        SetLibraryLoader([](const char *name) {
            return LoadLibraryA(name);
        });

        SetFunctionResolver([](HMODULE module, const char *name) {
            return (LPVOID)GetProcAddress(module, name);
        });
    }

    void ExecutableLoader::LoadImports(IMAGE_NT_HEADERS *ntHeader) {
        IMAGE_DATA_DIRECTORY *importDirectory = &ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];

        auto descriptor = GetTargetRVA<IMAGE_IMPORT_DESCRIPTOR>(importDirectory->VirtualAddress);

        while (descriptor->Name) {
            const char *name = GetTargetRVA<char>(descriptor->Name);

            HMODULE module = ResolveLibrary(name);

            if (!module) {
                printf("Could not load dependent module %s. Error code was %i.\n", name, GetLastError());
            }

            // "don't load"
            if (reinterpret_cast<uint32_t>(module) == 0xFFFFFFFF) {
                descriptor++;
                continue;
            }

            auto nameTableEntry    = GetTargetRVA<uintptr_t>(descriptor->OriginalFirstThunk);
            auto addressTableEntry = GetTargetRVA<uintptr_t>(descriptor->FirstThunk);

            // GameShield (Payne) uses FirstThunk for original name addresses
            if (!descriptor->OriginalFirstThunk) {
                nameTableEntry = GetTargetRVA<uintptr_t>(descriptor->FirstThunk);
            }

            while (*nameTableEntry) {
                FARPROC function;
                const char *functionName;

                // is this an ordinal-only import?
                if (IMAGE_SNAP_BY_ORDINAL(*nameTableEntry)) {
                    function     = GetProcAddress(module, MAKEINTRESOURCEA(IMAGE_ORDINAL(*nameTableEntry)));
                    //functionName = va("#%d", IMAGE_ORDINAL(*nameTableEntry));
                }
                else {
                    auto import = GetTargetRVA<IMAGE_IMPORT_BY_NAME>(*nameTableEntry);

                    function     = (FARPROC)m_functionResolver(module, import->Name);
                    functionName = import->Name;
                }

                if (!function) {
                    char pathName[MAX_PATH];
                    GetModuleFileNameA(module, pathName, sizeof(pathName));

                    printf("Could not load function %s in dependent module %s (%s).\n", functionName, name, pathName);
                }

                *addressTableEntry = (uintptr_t)function;

                nameTableEntry++;
                addressTableEntry++;
            }

            descriptor++;
        }
    }

    void ExecutableLoader::LoadSection(IMAGE_SECTION_HEADER *section) {
        void *targetAddress       = GetTargetRVA<uint8_t>(section->VirtualAddress);
        const void *sourceAddress = m_origBinary + section->PointerToRawData;

        if ((uintptr_t)targetAddress >= m_loadLimit) {
            return;
        }

        if (section->SizeOfRawData > 0) {
            uint32_t sizeOfData = std::min(section->SizeOfRawData, section->Misc.VirtualSize);

            DWORD oldProtect;
            VirtualProtect(targetAddress, sizeOfData, PAGE_EXECUTE_READWRITE, &oldProtect);
            memcpy(targetAddress, sourceAddress, sizeOfData);
        }
    }

    void ExecutableLoader::LoadSections(IMAGE_NT_HEADERS *ntHeader) {
        IMAGE_SECTION_HEADER *section = IMAGE_FIRST_SECTION(ntHeader);

        for (int i = 0; i < ntHeader->FileHeader.NumberOfSections; i++) {
            LoadSection(section);

            section++;
        }
    }

#if defined(_M_AMD64)
    void ExecutableLoader::LoadExceptionTable(IMAGE_NT_HEADERS *ntHeader) {
        IMAGE_DATA_DIRECTORY *exceptionDirectory = &ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];

        RUNTIME_FUNCTION *functionList = GetTargetRVA<RUNTIME_FUNCTION>(exceptionDirectory->VirtualAddress);
        DWORD entryCount               = exceptionDirectory->Size / sizeof(RUNTIME_FUNCTION);

        // has no use - inverted function tables get used instead from Ldr; we have no influence on those
        if (!RtlAddFunctionTable(functionList, entryCount, (DWORD64)GetModuleHandle(nullptr))) {
            printf("Setting exception handlers failed.");
        }

        // use CoreRT API instead
        if (HMODULE coreRT = GetModuleHandleW(L"FrameworkCoreRT.dll")) {
            auto sehMapper = (void (*)(void *, void *, PRUNTIME_FUNCTION, DWORD))GetProcAddress(coreRT, "CoreRT_SetupSEHHandler");
            sehMapper(m_module, ((char *)m_module) + ntHeader->OptionalHeader.SizeOfImage, functionList, entryCount);
        }
    }
#endif
    void ExecutableLoader::LoadIntoModule(HMODULE module) {
        m_module = module;

        IMAGE_DOS_HEADER *header = (IMAGE_DOS_HEADER *)m_origBinary;

        if (header->e_magic != IMAGE_DOS_SIGNATURE) {
            return;
        }

        IMAGE_DOS_HEADER *sourceHeader   = (IMAGE_DOS_HEADER *)module;
        IMAGE_NT_HEADERS *sourceNtHeader = GetTargetRVA<IMAGE_NT_HEADERS>(sourceHeader->e_lfanew);

        auto origCheckSum  = sourceNtHeader->OptionalHeader.CheckSum;
        auto origTimeStamp = sourceNtHeader->FileHeader.TimeDateStamp;
        auto origDebugDir  = sourceNtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];

        IMAGE_NT_HEADERS *ntHeader = (IMAGE_NT_HEADERS *)(m_origBinary + header->e_lfanew);
        m_entryPoint               = GetTargetRVA<void>(ntHeader->OptionalHeader.AddressOfEntryPoint);

        LoadSections(ntHeader);
        LoadImports(ntHeader);

        DWORD oldProtect2;
        VirtualProtect(sourceNtHeader, 0x1000, PAGE_EXECUTE_READWRITE, &oldProtect2);

        sourceNtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC] = ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
        ApplyRelocations();

#if defined(_M_AMD64)
        uint32_t tlsIndex = 0;
        void *tlsInit     = nullptr;

        m_tlsInitializer(&tlsInit, &tlsIndex);
        LoadExceptionTable(ntHeader);
#endif

        if (ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].Size) {
            const IMAGE_TLS_DIRECTORY *targetTls = GetTargetRVA<IMAGE_TLS_DIRECTORY>(sourceNtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].VirtualAddress);
            const IMAGE_TLS_DIRECTORY *sourceTls = GetTargetRVA<IMAGE_TLS_DIRECTORY>(ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].VirtualAddress);

#if defined(_M_IX86)
            LPVOID *tlsBase = (LPVOID *)__readfsdword(0x2C);
#elif defined(_M_AMD64)
            LPVOID *tlsBase = (LPVOID *)__readgsqword(0x58);
#endif

            if (sourceTls->StartAddressOfRawData) {
                DWORD oldProtect;
                VirtualProtect(tlsInit, sourceTls->EndAddressOfRawData - sourceTls->StartAddressOfRawData, PAGE_READWRITE, &oldProtect);

                memcpy(tlsBase[tlsIndex], reinterpret_cast<void *>(sourceTls->StartAddressOfRawData), sourceTls->EndAddressOfRawData - sourceTls->StartAddressOfRawData);
                memcpy(tlsInit, reinterpret_cast<void *>(sourceTls->StartAddressOfRawData), sourceTls->EndAddressOfRawData - sourceTls->StartAddressOfRawData);
            }

            if (sourceTls->AddressOfIndex) {
                hook::put(sourceTls->AddressOfIndex, tlsIndex);
            }
        }

        // copy over the offset to the new imports directory
        DWORD oldProtect;
        VirtualProtect(sourceNtHeader, 0x1000, PAGE_EXECUTE_READWRITE, &oldProtect);
        sourceNtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT] = ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];

        memcpy(sourceNtHeader, ntHeader, sizeof(IMAGE_NT_HEADERS) + (ntHeader->FileHeader.NumberOfSections * (sizeof(IMAGE_SECTION_HEADER))));

        sourceNtHeader->OptionalHeader.CheckSum  = origCheckSum;
        sourceNtHeader->FileHeader.TimeDateStamp = origTimeStamp;

        sourceNtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG] = origDebugDir;
    }

    bool ExecutableLoader::ApplyRelocations() {
        IMAGE_DOS_HEADER *dosHeader = reinterpret_cast<IMAGE_DOS_HEADER *>(m_module);

        IMAGE_NT_HEADERS *ntHeader = GetTargetRVA<IMAGE_NT_HEADERS>(dosHeader->e_lfanew);

        IMAGE_DATA_DIRECTORY *relocationDirectory = &ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];

        IMAGE_BASE_RELOCATION *relocation    = GetTargetRVA<IMAGE_BASE_RELOCATION>(relocationDirectory->VirtualAddress);
        IMAGE_BASE_RELOCATION *endRelocation = reinterpret_cast<IMAGE_BASE_RELOCATION *>((char *)relocation + relocationDirectory->Size);

        intptr_t relocOffset = reinterpret_cast<intptr_t>(m_module) - reinterpret_cast<intptr_t>(GetModuleHandle(NULL));

        if (relocOffset == 0) {
            return true;
        }

        // loop
        while (true) {
            // are we past the size?
            if (relocation >= endRelocation) {
                break;
            }

            // is this an empty block?
            if (relocation->SizeOfBlock == 0) {
                break;
            }

            // go through each and every relocation
            size_t numRelocations = (relocation->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(uint16_t);
            uint16_t *relocStart  = reinterpret_cast<uint16_t *>(relocation + 1);

            for (size_t i = 0; i < numRelocations; i++) {
                uint16_t type = relocStart[i] >> 12;
                uint32_t rva  = (relocStart[i] & 0xFFF) + relocation->VirtualAddress;

                void *addr = GetTargetRVA<void>(rva);
                DWORD oldProtect;
                VirtualProtect(addr, 4, PAGE_EXECUTE_READWRITE, &oldProtect);

                if (type == IMAGE_REL_BASED_HIGHLOW) {
                    *reinterpret_cast<int32_t *>(addr) += relocOffset;
                }
                else if (type == IMAGE_REL_BASED_DIR64) {
                    *reinterpret_cast<int64_t *>(addr) += relocOffset;
                }
                else if (type != IMAGE_REL_BASED_ABSOLUTE) {
                    return false;
                }

                VirtualProtect(addr, 4, oldProtect, &oldProtect);
            }

            // on to the next one!
            relocation = reinterpret_cast<IMAGE_BASE_RELOCATION *>((char *)relocation + relocation->SizeOfBlock);
        }

        return true;
    }

    HMODULE ExecutableLoader::ResolveLibrary(const char *name) {
        return m_libraryLoader(name);
    }
} // namespace Framework::Launcher::Loaders
