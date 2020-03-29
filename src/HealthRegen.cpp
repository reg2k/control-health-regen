#include "HealthRegen.h"

#include "Logger.h"
#include "Utils.h"
#include "rva/RVA.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <cinttypes>
#include <cstdlib>

#define INI_LOCATION "./plugins/HealthRegen.ini"

//===============
// Globals
//===============
Logger g_logger;

//===================
// Addresses [2]
//===================
RVA<uintptr_t> Addr_SetCombatRegenValue("48 8B 41 58 C7 80 84 00 00 00 00 00 80 BF");
RVA<uintptr_t> Addr_SetRegenValue("48 8B 41 38 F3 0F 10 40 ? 48 8B 41 58"); // out of combat

//=============
// Main
//=============

namespace HealthRegen {

    bool InitAddresses() {
        _LOG("Sigscan start");
        RVAUtils::Timer tmr; tmr.start();
        RVAManager::UpdateAddresses(0);

        _LOG("Sigscan elapsed: %llu ms.", tmr.stop());

        // Check if all addresses were resolved
        for (auto rvaData : RVAManager::GetAllRVAs()) {
            if (!rvaData->effectiveAddress) {
                _LOG("Not resolved: %s", rvaData->sig);
            }
        }
        if (!RVAManager::IsAllResolved()) return false;

        return true;
    }

    void Patch() {
        // Get the in-combat regen value
        char szCombatRegenValue[64];
        GetPrivateProfileStringA("HealthRegen", "fCombatRegenValue", "0.2", szCombatRegenValue, sizeof(szCombatRegenValue), INI_LOCATION);

        float fMinCombatRegenLimit = strtof(szCombatRegenValue, NULL);
        if (fMinCombatRegenLimit != -1) { // game default is -1
            _LOG("Setting in-combat regen value to %f.", fMinCombatRegenLimit);
            Utils::WriteMemory(Addr_SetCombatRegenValue.GetUIntPtr() + 10, &fMinCombatRegenLimit, sizeof(fMinCombatRegenLimit));
            _LOG("In-combat regen value set.");
        }

        // Get the out-of-combat regen value
        char szRegenValue[64];
        GetPrivateProfileStringA("HealthRegen", "fRegenValue", "1.0", szRegenValue, sizeof(szRegenValue), INI_LOCATION);
        
        float fMinRegenLimit = strtof(szRegenValue, NULL);
        if (fMinRegenLimit != 0.2) { // game default is 0.2
            _LOG("Setting out-of-combat regen value to %f.", fMinRegenLimit);
            // mov eax, <float value>
            // movd xmm0, eax
            unsigned char data[] = { 0xB8, 0x00, 0x00, 0x00, 0x00, 0x66, 0x0F, 0x6E, 0xC0 };
            *reinterpret_cast<float*>(data + 1) = fMinRegenLimit;
            Utils::WriteMemory(Addr_SetRegenValue.GetUIntPtr(), data, sizeof(data));
            _LOG("Regen value set.");
        }
    }

    void Init() {
        char logPath[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, NULL, logPath))) {
            strcat_s(logPath, "\\Remedy\\Control\\HealthRegen.log");
            g_logger.Open(logPath);
        }
        _LOG("HealthRegen v1.0 by reg2k");
        _LOG("Game version: %" PRIX64, Utils::GetGameVersion());
        _LOG("Module base: %p", GetModuleHandle(NULL));

        // Sigscan
        if (!InitAddresses()) {
            MessageBoxA(NULL, "HealthRegen is not compatible with this version of Control.\nPlease visit the mod page for updates.", "HealthRegen", MB_OK | MB_ICONEXCLAMATION);
            _LOG("FATAL: Incompatible version");
            return;
        }
        _LOG("Addresses set");
        Patch();
        _LOG("Done.");
    }
}
