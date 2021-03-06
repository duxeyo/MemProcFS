// m_status.c : implementation of the .status built-in module.
//
// (c) Ulf Frisk, 2018-2021
// Author: Ulf Frisk, pcileech@frizk.net
//

/*
* The m_status module registers itself with the name '.status' with the plugin manager.
* 
* The module showcase a "root" directory module as well as a stateless module.
*
* The module implements listing of directories as well as read and write.
* Read/Write happens, if allowed, to various configuration and status settings
* related to the VMM and Memory Process File System.
*/

#include "pdb.h"
#include "pluginmanager.h"
#include "util.h"
#include "vmm.h"
#include "vmmproc.h"
#include "vmmwinreg.h"
#include "statistics.h"

/*
* Read : function as specified by the module manager. The module manager will
* call into this callback function whenever a read shall occur from a "file".
* -- ctx
* -- pb
* -- cb
* -- pcbRead
* -- cbOffset
* -- return
*/
NTSTATUS MStatus_Read(_In_ PVMMDLL_PLUGIN_CONTEXT ctx, _Out_writes_to_(cb, *pcbRead) PBYTE pb, _In_ DWORD cb, _Out_ PDWORD pcbRead, _In_ QWORD cbOffset)
{
    DWORD cchBuffer;
    CHAR szBuffer[0x800];
    DWORD cbCallStatistics = 0;
    LPSTR szCallStatistics = NULL;
    QWORD cPageReadTotal, cPageFailTotal;
    NTSTATUS nt = VMMDLL_STATUS_FILE_INVALID;
    if(!_wcsicmp(ctx->wszPath, L"config_process_show_terminated")) {
        return Util_VfsReadFile_FromBOOL(ctxVmm->flags & VMM_FLAG_PROCESS_SHOW_TERMINATED, pb, cb, pcbRead, cbOffset);
    }
    if(!_wcsicmp(ctx->wszPath, L"config_cache_enable")) {
        return Util_VfsReadFile_FromBOOL(!(ctxVmm->flags & VMM_FLAG_NOCACHE), pb, cb, pcbRead, cbOffset);
    }
    if(!_wcsicmp(ctx->wszPath, L"config_paging_enable")) {
        return Util_VfsReadFile_FromBOOL(!(ctxVmm->flags & VMM_FLAG_NOPAGING), pb, cb, pcbRead, cbOffset);
    }
    if(!_wcsicmp(ctx->wszPath, L"config_statistics_fncall")) {
        return Util_VfsReadFile_FromBOOL(Statistics_CallGetEnabled(), pb, cb, pcbRead, cbOffset);
    }
    if(!_wcsicmp(ctx->wszPath, L"config_refresh_enable")) {
        return Util_VfsReadFile_FromBOOL(ctxVmm->ThreadProcCache.fEnabled, pb, cb, pcbRead, cbOffset);
    }
    if(!_wcsicmp(ctx->wszPath, L"config_refresh_tick_period_ms")) {
        return Util_VfsReadFile_FromDWORD(ctxVmm->ThreadProcCache.cMs_TickPeriod, pb, cb, pcbRead, cbOffset, FALSE);
    }
    if(!_wcsicmp(ctx->wszPath, L"config_refresh_read")) {
        return Util_VfsReadFile_FromDWORD(ctxVmm->ThreadProcCache.cTick_MEM, pb, cb, pcbRead, cbOffset, FALSE);
    }
    if(!_wcsicmp(ctx->wszPath, L"config_refresh_tlb")) {
        return Util_VfsReadFile_FromDWORD(ctxVmm->ThreadProcCache.cTick_TLB, pb, cb, pcbRead, cbOffset, FALSE);
    }
    if(!_wcsicmp(ctx->wszPath, L"config_refresh_proc_partial")) {
        return Util_VfsReadFile_FromDWORD(ctxVmm->ThreadProcCache.cTick_Fast, pb, cb, pcbRead, cbOffset, FALSE);
    }
    if(!_wcsicmp(ctx->wszPath, L"config_refresh_proc_total")) {
        return Util_VfsReadFile_FromDWORD(ctxVmm->ThreadProcCache.cTick_Medium, pb, cb, pcbRead, cbOffset, FALSE);
    }
    if(!_wcsicmp(ctx->wszPath, L"config_refresh_registry")) {
        return Util_VfsReadFile_FromDWORD(ctxVmm->ThreadProcCache.cTick_Slow, pb, cb, pcbRead, cbOffset, FALSE);
    }
    if(!_wcsicmp(ctx->wszPath, L"statistics")) {
        cPageReadTotal = ctxVmm->stat.page.cPrototype + ctxVmm->stat.page.cTransition + ctxVmm->stat.page.cDemandZero + ctxVmm->stat.page.cVAD + ctxVmm->stat.page.cCacheHit + ctxVmm->stat.page.cPageFile + ctxVmm->stat.page.cCompressed;
        cPageFailTotal = ctxVmm->stat.page.cFailCacheHit + ctxVmm->stat.page.cFailVAD + ctxVmm->stat.page.cFailPageFile + ctxVmm->stat.page.cFailCompressed + ctxVmm->stat.page.cFail;
        cchBuffer = snprintf(szBuffer, 0x800,
            "VMM STATISTICS   (4kB PAGES / COUNTS - HEXADECIMAL)\n" \
            "===================================================\n" \
            "PHYSICAL MEMORY:                      \n" \
            "  READ CACHE HIT:               %16llx\n" \
            "  READ RETRIEVED:               %16llx\n" \
            "  READ FAIL:                    %16llx\n" \
            "  WRITE:                        %16llx\n" \
            "PAGED VIRTUAL MEMORY:                 \n" \
            "  READ SUCCESS:                 %16llx\n" \
            "    Prototype:                  %16llx\n" \
            "    Transition:                 %16llx\n" \
            "    DemandZero:                 %16llx\n" \
            "    VAD:                        %16llx\n" \
            "    Cache:                      %16llx\n" \
            "    PageFile:                   %16llx\n" \
            "    Compressed:                 %16llx\n" \
            "  READ FAIL:                    %16llx\n" \
            "    Cache:                      %16llx\n" \
            "    VAD:                        %16llx\n" \
            "    PageFile:                   %16llx\n" \
            "    Compressed:                 %16llx\n" \
            "TLB (PAGE TABLES):                    \n" \
            "  CACHE HIT:                    %16llx\n" \
            "  RETRIEVED:                    %16llx\n" \
            "  FAILED:                       %16llx\n" \
            "PHYSICAL MEMORY REFRESH:        %16llx\n" \
            "TLB MEMORY REFRESH:             %16llx\n" \
            "PROCESS PARTIAL REFRESH:        %16llx\n" \
            "PROCESS FULL REFRESH:           %16llx\n",
            ctxVmm->stat.cPhysCacheHit, ctxVmm->stat.cPhysReadSuccess, ctxVmm->stat.cPhysReadFail, ctxVmm->stat.cPhysWrite,
            cPageReadTotal, ctxVmm->stat.page.cPrototype, ctxVmm->stat.page.cTransition, ctxVmm->stat.page.cDemandZero, ctxVmm->stat.page.cVAD, ctxVmm->stat.page.cCacheHit, ctxVmm->stat.page.cPageFile, ctxVmm->stat.page.cCompressed,
            cPageFailTotal, ctxVmm->stat.page.cFailCacheHit, ctxVmm->stat.page.cFailVAD, ctxVmm->stat.page.cFailPageFile, ctxVmm->stat.page.cFailCompressed,
            ctxVmm->stat.cTlbCacheHit, ctxVmm->stat.cTlbReadSuccess, ctxVmm->stat.cTlbReadFail,
            ctxVmm->stat.cPhysRefreshCache, ctxVmm->stat.cTlbRefreshCache, ctxVmm->stat.cProcessRefreshPartial, ctxVmm->stat.cProcessRefreshFull
        );
        return Util_VfsReadFile_FromPBYTE(szBuffer, cchBuffer, pb, cb, pcbRead, cbOffset);
    }
    if(!_wcsicmp(ctx->wszPath, L"statistics_fncall")) {
        if(Statistics_CallToString(&szCallStatistics, &cbCallStatistics)) {
            nt = Util_VfsReadFile_FromPBYTE(szCallStatistics, cbCallStatistics, pb, cb, pcbRead, cbOffset);
            LocalFree(szCallStatistics);
        }
        return nt;
    }
    if(!_wcsicmp(ctx->wszPath, L"config_printf_enable")) {
        return Util_VfsReadFile_FromBOOL(ctxMain->cfg.fVerboseDll, pb, cb, pcbRead, cbOffset);
    }
    if(!_wcsicmp(ctx->wszPath, L"config_printf_v")) {
        return Util_VfsReadFile_FromBOOL(ctxMain->cfg.fVerbose, pb, cb, pcbRead, cbOffset);
    }
    if(!_wcsicmp(ctx->wszPath, L"config_printf_vv")) {
        return Util_VfsReadFile_FromBOOL(ctxMain->cfg.fVerboseExtra, pb, cb, pcbRead, cbOffset);
    }
    if(!_wcsicmp(ctx->wszPath, L"config_printf_vvv")) {
        return Util_VfsReadFile_FromBOOL(ctxMain->cfg.fVerboseExtraTlp, pb, cb, pcbRead, cbOffset);
    }
    if(!_wcsicmp(ctx->wszPath, L"native_max_address")) {
        return Util_VfsReadFile_FromQWORD(ctxMain->dev.paMax, pb, cb, pcbRead, cbOffset, FALSE);
    }
    if(!_wcsnicmp(ctx->wszPath, L"config_symbol", 13)) {
        if(!_wcsicmp(ctx->wszPath, L"config_symbol_enable")) {
            return Util_VfsReadFile_FromBOOL(ctxMain->pdb.fEnable, pb, cb, pcbRead, cbOffset);
        }
        if(!_wcsicmp(ctx->wszPath, L"config_symbolcache")) {
            return Util_VfsReadFile_FromPBYTE(ctxMain->pdb.szLocal, strlen(ctxMain->pdb.szLocal), pb, cb, pcbRead, cbOffset);
        }
        if(!_wcsicmp(ctx->wszPath, L"config_symbolserver")) {
            return Util_VfsReadFile_FromPBYTE(ctxMain->pdb.szServer, strlen(ctxMain->pdb.szServer), pb, cb, pcbRead, cbOffset);
        }
        if(!_wcsicmp(ctx->wszPath, L"config_symbolserver_enable")) {
            return Util_VfsReadFile_FromBOOL(ctxMain->pdb.fServerEnable, pb, cb, pcbRead, cbOffset);
        }
    }
    return VMMDLL_STATUS_FILE_INVALID;
}

NTSTATUS MStatus_Write_NotifyVerbosityChange(_In_ NTSTATUS nt)
{
    if(nt == VMMDLL_STATUS_SUCCESS) {
        PluginManager_Notify(VMMDLL_PLUGIN_NOTIFY_VERBOSITYCHANGE, NULL, 0);
    }
    return nt;
}

/*
* Write : function as specified by the module manager. The module manager will
* call into this callback function whenever a write shall occur from a "file".
* -- ctx
* -- pb
* -- cb
* -- pcbWrite
* -- cbOffset
* -- return
*/
NTSTATUS MStatus_Write(_In_ PVMMDLL_PLUGIN_CONTEXT ctx, _In_reads_(cb) PBYTE pb, _In_ DWORD cb, _Out_ PDWORD pcbWrite, _In_ QWORD cbOffset)
{
    NTSTATUS nt;
    BOOL fEnable = FALSE;
    if(!_wcsicmp(ctx->wszPath, L"config_process_show_terminated")) {
        nt = Util_VfsWriteFile_BOOL(&fEnable, pb, cb, pcbWrite, cbOffset);
        if(nt == VMMDLL_STATUS_SUCCESS) {
            ctxVmm->flags &= ~VMM_FLAG_PROCESS_SHOW_TERMINATED;
            ctxVmm->flags |= fEnable ? VMM_FLAG_PROCESS_SHOW_TERMINATED : 0;
        }
        return nt;
    }
    if(!_wcsicmp(ctx->wszPath, L"config_cache_enable")) {
        nt = Util_VfsWriteFile_BOOL(&fEnable, pb, cb, pcbWrite, cbOffset);
        if(nt == VMMDLL_STATUS_SUCCESS) {
            ctxVmm->flags &= ~VMM_FLAG_NOCACHE;
            ctxVmm->flags |= fEnable ? 0 : VMM_FLAG_NOCACHE;
        }
        return nt;
    }
    if(!_wcsicmp(ctx->wszPath, L"config_paging_enable")) {
        nt = Util_VfsWriteFile_BOOL(&fEnable, pb, cb, pcbWrite, cbOffset);
        if(nt == VMMDLL_STATUS_SUCCESS) {
            ctxVmm->flags &= ~VMM_FLAG_NOPAGING;
            ctxVmm->flags |= fEnable ? 0 : VMM_FLAG_NOPAGING;
        }
        return nt;
    }
    if(!_wcsicmp(ctx->wszPath, L"config_statistics_fncall")) {
        nt = Util_VfsWriteFile_BOOL(&fEnable, pb, cb, pcbWrite, cbOffset);
        if(nt == VMMDLL_STATUS_SUCCESS) {
            Statistics_CallSetEnabled(fEnable);
        }
        return nt;
    }
    if(!_wcsicmp(ctx->wszPath, L"config_refresh_tick_period_ms")) {
        return Util_VfsWriteFile_DWORD(&ctxVmm->ThreadProcCache.cMs_TickPeriod, pb, cb, pcbWrite, cbOffset, 50, 0);
    }
    if(!_wcsicmp(ctx->wszPath, L"config_refresh_read")) {
        return Util_VfsWriteFile_DWORD(&ctxVmm->ThreadProcCache.cTick_MEM, pb, cb, pcbWrite, cbOffset, 1, 0);
    }
    if(!_wcsicmp(ctx->wszPath, L"config_refresh_tlb")) {
        return Util_VfsWriteFile_DWORD(&ctxVmm->ThreadProcCache.cTick_TLB, pb, cb, pcbWrite, cbOffset, 1, 0);
    }
    if(!_wcsicmp(ctx->wszPath, L"config_refresh_proc_partial")) {
        return Util_VfsWriteFile_DWORD(&ctxVmm->ThreadProcCache.cTick_Fast, pb, cb, pcbWrite, cbOffset, 1, 0);
    }
    if(!_wcsicmp(ctx->wszPath, L"config_refresh_proc_total")) {
        return Util_VfsWriteFile_DWORD(&ctxVmm->ThreadProcCache.cTick_Medium, pb, cb, pcbWrite, cbOffset, 1, 0);
    }
    if(!_wcsicmp(ctx->wszPath, L"config_refresh_registry")) {
        VmmWinReg_Refresh();
        return Util_VfsWriteFile_DWORD(&ctxVmm->ThreadProcCache.cTick_Slow, pb, cb, pcbWrite, cbOffset, 1, 0);
    }
    if(!_wcsicmp(ctx->wszPath, L"config_printf_enable")) {
        return MStatus_Write_NotifyVerbosityChange(
            Util_VfsWriteFile_BOOL(&ctxMain->cfg.fVerboseDll, pb, cb, pcbWrite, cbOffset));
    }
    if(!_wcsicmp(ctx->wszPath, L"config_printf_v")) {
        return MStatus_Write_NotifyVerbosityChange(
            Util_VfsWriteFile_BOOL(&ctxMain->cfg.fVerbose, pb, cb, pcbWrite, cbOffset));
    }
    if(!_wcsicmp(ctx->wszPath, L"config_printf_vv")) {
        return MStatus_Write_NotifyVerbosityChange(
            Util_VfsWriteFile_BOOL(&ctxMain->cfg.fVerboseExtra, pb, cb, pcbWrite, cbOffset));
    }
    if(!_wcsicmp(ctx->wszPath, L"config_printf_vvv")) {
        return MStatus_Write_NotifyVerbosityChange(
            Util_VfsWriteFile_BOOL(&ctxMain->cfg.fVerboseExtraTlp, pb, cb, pcbWrite, cbOffset));
    }
    if(!_wcsnicmp(ctx->wszPath, L"config_symbol", 13)) {
        nt = VMMDLL_STATUS_FILE_INVALID;
        if(!_wcsicmp(ctx->wszPath, L"config_symbol_enable")) {
            nt = Util_VfsWriteFile_DWORD(&ctxMain->pdb.fEnable, pb, cb, pcbWrite, cbOffset, 1, 0);
        }
        if(!_wcsicmp(ctx->wszPath, L"config_symbolcache")) {
            nt = Util_VfsWriteFile_PBYTE(ctxMain->pdb.szLocal, _countof(ctxMain->pdb.szLocal) - 1, pb, cb, pcbWrite, cbOffset, TRUE);
        }
        if(!_wcsicmp(ctx->wszPath, L"config_symbolserver")) {
            nt = Util_VfsWriteFile_PBYTE(ctxMain->pdb.szServer, _countof(ctxMain->pdb.szServer) - 1, pb, cb, pcbWrite, cbOffset, TRUE);
        }
        if(!_wcsicmp(ctx->wszPath, L"config_symbolserver_enable")) {
            nt = Util_VfsWriteFile_DWORD(&ctxMain->pdb.fServerEnable, pb, cb, pcbWrite, cbOffset, 1, 0);
        }
        PDB_ConfigChange();
        return nt;
    }
    return VMMDLL_STATUS_FILE_INVALID;
}

/*
* List : function as specified by the module manager. The module manager will
* call into this callback function whenever a list directory shall occur from
* the given module.
* -- ctx
* -- pFileList
* -- return
*/
BOOL MStatus_List(_In_ PVMMDLL_PLUGIN_CONTEXT ctx, _Inout_ PHANDLE pFileList)
{
    DWORD cbCallStatistics = 0;
    // not module root directory -> fail!
    if(ctx->wszPath[0]) { return FALSE; }
    // "root" view
    if(!ctx->pProcess) {
        Statistics_CallToString(NULL, &cbCallStatistics);
        VMMDLL_VfsList_AddFile(pFileList, L"config_cache_enable", 1, NULL);
        VMMDLL_VfsList_AddFile(pFileList, L"config_paging_enable", 1, NULL);
        VMMDLL_VfsList_AddFile(pFileList, L"config_statistics_fncall", 1, NULL);
        VMMDLL_VfsList_AddFile(pFileList, L"config_refresh_enable", 1, NULL);
        VMMDLL_VfsList_AddFile(pFileList, L"config_refresh_tick_period_ms", 8, NULL);
        VMMDLL_VfsList_AddFile(pFileList, L"config_refresh_read", 8, NULL);
        VMMDLL_VfsList_AddFile(pFileList, L"config_refresh_tlb", 8, NULL);
        VMMDLL_VfsList_AddFile(pFileList, L"config_refresh_proc_partial", 8, NULL);
        VMMDLL_VfsList_AddFile(pFileList, L"config_refresh_proc_total", 8, NULL);
        VMMDLL_VfsList_AddFile(pFileList, L"config_refresh_registry", 8, NULL);
        VMMDLL_VfsList_AddFile(pFileList, L"config_symbol_enable", 1, NULL);
        VMMDLL_VfsList_AddFile(pFileList, L"config_symbolcache", strlen(ctxMain->pdb.szLocal), NULL);
        VMMDLL_VfsList_AddFile(pFileList, L"config_symbolserver", strlen(ctxMain->pdb.szServer), NULL);
        VMMDLL_VfsList_AddFile(pFileList, L"config_symbolserver_enable", 1, NULL);
        VMMDLL_VfsList_AddFile(pFileList, L"statistics", 1103, NULL);
        VMMDLL_VfsList_AddFile(pFileList, L"config_printf_enable", 1, NULL);
        VMMDLL_VfsList_AddFile(pFileList, L"config_printf_v", 1, NULL);
        VMMDLL_VfsList_AddFile(pFileList, L"config_printf_vv", 1, NULL);
        VMMDLL_VfsList_AddFile(pFileList, L"config_printf_vvv", 1, NULL);
        VMMDLL_VfsList_AddFile(pFileList, L"config_process_show_terminated", 1, NULL);
        VMMDLL_VfsList_AddFile(pFileList, L"native_max_address", 16, NULL);
        VMMDLL_VfsList_AddFile(pFileList, L"statistics_fncall", cbCallStatistics, NULL);
    }
    return TRUE;
}

/*
* Initialization function. The module manager shall call into this function
* when the module shall be initialized. If the module wish to initialize it
* shall call the supplied pfnPluginManager_Register function.
* NB! the module does not have to register itself - for example if the target
* operating system or architecture is unsupported.
* -- pPluginRegInfo
*/
VOID M_Status_Initialize(_Inout_ PVMMDLL_PLUGIN_REGINFO pRI)
{
    if((pRI->magic != VMMDLL_PLUGIN_REGINFO_MAGIC) || (pRI->wVersion != VMMDLL_PLUGIN_REGINFO_VERSION)) { return; }
    // .status module is always valid - no check against pPluginRegInfo->tpMemoryModel, tpSystem
    wcscpy_s(pRI->reg_info.wszPathName, 128, L"\\.status");     // module name
    pRI->reg_info.fRootModule = TRUE;                           // module shows in root directory
    pRI->reg_fn.pfnList = MStatus_List;                         // List function supported
    pRI->reg_fn.pfnRead = MStatus_Read;                         // Read function supported
    pRI->reg_fn.pfnWrite = MStatus_Write;                       // Write function supported
    pRI->pfnPluginManager_Register(pRI);
}
