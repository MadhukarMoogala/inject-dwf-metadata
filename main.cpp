//////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2025 Autodesk, Inc.  All rights reserved.
//
//  Use of this software is subject to the terms of the Autodesk license 
//  agreement provided at the time of installation or download, or which 
//  otherwise accompanies this software in either electronic or hard copy form.   
//
//////////////////////////////////////////////////////////////////////////////
//
#if defined(_DEBUG) && !defined(AC_FULL_DEBUG)
#error _DEBUG should not be defined except in internal Adesk debug builds
#endif
#include <Windows.h>
#include <string.h>
#include <aced.h>
#include <dbents.h>
#include <dbsymtb.h>
#include <dbgroup.h>
#include <dbapserv.h>
#include "tchar.h"
#include "acdmmapi.h"
#include "rxregsvc.h"
#include "acpublishreactors.h"
#include <dbobjptr.h>
#include <eoktest.h> 
#include "acpublishuireactors.h"


#define EPLOT_SERVICENAME _T("AcEPlotX")
#define EPLOT_ARXNAME     _T("AcEPlotX.crx")
#define PUBLISH_DLL       _T("AcPublish.crx")
#define PUBLISH_UI_DLL    _T("AcPublish.arx")

class TstMMReactor* g_pMMReactor = nullptr;
class MyPublishReactor* g_pPubReactor = nullptr;
class MyPublishUIReactor* g_pPubUIReactor = nullptr;

class TstMMReactor : public AcDMMReactor
{
public:
    TstMMReactor() : AcDMMReactor() {}

    void OnBeginSheet(AcDMMSheetReactorInfo* pInfo) override {
        AcDMMEPlotProperty prop(_T("LayoutId"), pInfo->UniqueLayoutId());
        prop.SetCategory(_T("APS"));
        AcDMMEPlotPropertyVec properties;
        properties.append(prop);
        pInfo->AddPageProperties(properties);
    }

    void OnBeginEntity(AcDMMEntityReactorInfo* pInfo) override {
        AcDbEntity* pEntity = pInfo->entity();
        assert(pEntity != nullptr);
        if (!pEntity) return;

        AcDbObjectId objId = pEntity->objectId();
        AcDbObjectIdArray objIdArray = pInfo->getEntityBlockRefPath();

        AcDbBlockReference* pBlkRef = pEntity->isKindOf(AcDbBlockReference::desc())
            ? AcDbBlockReference::cast(pEntity) : nullptr;

        AcDMMEPlotProperties props;
        int nProps = 0;

        // Add Block Name
        if (pBlkRef != nullptr) {
            AcDbObjectId btrId = pBlkRef->blockTableRecord();
            AcDbBlockTableRecordPointer pBTR(btrId, AcDb::kForRead);
            if (eOkVerify(pBTR.openStatus())) {
                const ACHAR* blkName = nullptr;
                if (pBTR->getName(blkName) == Acad::eOk && blkName) {
                    AcDMMEPlotProperty prop(L"BlockName", blkName);
                    prop.SetCategory(L"Geometry");
                    props.AddProperty(&prop); nProps++;
                }
            }
        }

        // Add Layer
        AcDbLayerTableRecordPointer pLayer(pEntity->layerId(), AcDb::kForRead);
        if (eOkVerify(pLayer.openStatus())) {
            const ACHAR* layerName = nullptr;
            if (pLayer->getName(layerName) == Acad::eOk && layerName) {
                AcDMMEPlotProperty prop(L"Layer", layerName);
                prop.SetCategory(L"Display");
                props.AddProperty(&prop); nProps++;
            }
        }

        // Add Area (approximate extents)
        AcDbExtents ext;
        if (pEntity->getGeomExtents(ext) == Acad::eOk) {
            double area = (ext.maxPoint().x - ext.minPoint().x) *
                (ext.maxPoint().y - ext.minPoint().y);
            AcDMMEPlotProperty prop(L"ApproxArea", std::to_wstring(area).c_str());
            prop.SetCategory(L"Geometry");
            props.AddProperty(&prop); nProps++;
        }

        // Add Handle
        AcDbHandle handle;
        pEntity->getAcDbHandle(handle);
        TCHAR handleStr[20];
        handle.getIntoAsciiBuffer(handleStr);
        AcDMMEPlotProperty propHandle(L"EntityHandle", handleStr);
        propHandle.SetCategory(L"Debug");
        props.AddProperty(&propHandle); nProps++;

        if (nProps == 0) return;

        // Compose Unique Property ID
        const wchar_t* wsUnique = pInfo->UniqueEntityId();
        AcDMMWideString wsPropId = L"ACAD-";
        wsPropId += wsUnique;
        delete[] wsUnique;

        props.SetId(wsPropId);
        pInfo->AddProperties(&props);

        // Assign to Node
        int nodeId = 0;
        const AcDMMNode* pNode = nullptr;
        bool bNewNode = false;

        if (!pInfo->GetEntityNode(objId, objIdArray, nodeId)) {
            nodeId = pInfo->GetNextAvailableNodeId();
            pNode = new AcDMMNode(nodeId, L"");
            bNewNode = true;
            pInfo->AddNodeToMap(objId, objIdArray, nodeId);
        }
        else {
            pNode = pInfo->GetNode(nodeId);
        }

        if (pNode != nullptr) {
            AcDMMStringVec idVec;
            idVec.append(wsPropId);
            pInfo->AddPropertiesIds(&idVec, *(AcDMMNode*)pNode);
        }

        if (bNewNode && pNode != nullptr) {
            delete pNode;
        }
    }


    void OnEndEntity(AcDMMEntityReactorInfo* pInfo) override {
        pInfo->GetPlotLogger()->logMessage(_T("[MMReactor] Finished entity."));
    }

    void OnEndSheet(AcDMMSheetReactorInfo* pInfo) override {
        AcString msg;
        msg.format(_T("[MMReactor] Finished: %s"), pInfo->UniqueLayoutId());
        pInfo->GetPlotLogger()->logMessage(msg);
        AcDMMResourceInfo resInfo(L"Help file", L"text/html", L"c:\\help.html");
        AcDMMResourceVec resVec;
        resVec.append(resInfo);
        pInfo->AddPageResources(resVec);
    }
};

class MyPublishReactor : public AcPublishReactor
{
public:
    void OnAboutToBeginPublishing(AcPublishBeginJobInfo* pInfo) override {
        acutPrintf(L"\n[PublishReactor] About to begin publishing...");
        if (!acrxServiceIsRegistered(EPLOT_SERVICENAME))
            acrxLoadModule(EPLOT_ARXNAME, false, false);

        HINSTANCE hDmm = ::GetModuleHandle(EPLOT_ARXNAME);
        if (hDmm) {
            using ADD_DMM_REACTOR = void(*)(AcDMMReactor*);
            ADD_DMM_REACTOR pAddDMM = (ADD_DMM_REACTOR)GetProcAddress(hDmm, "AcGlobAddDMMReactor");
            if (pAddDMM) {
                g_pMMReactor = new TstMMReactor();
                pAddDMM(g_pMMReactor);
                acutPrintf(L"\n[Register] AcDMMReactor registered.");
            }
        }
       
       
    }

    void OnEndPublish(AcPublishReactorInfo* pInfo) override {
        acutPrintf(L"\n[PublishReactor] Publishing ended.");
        if (g_pMMReactor) {
            HINSTANCE hDmm = ::GetModuleHandle(EPLOT_ARXNAME);
            if (hDmm) {
                using REMOVE_DMM_REACTOR = void(*)(AcDMMReactor*);
                REMOVE_DMM_REACTOR pRemDMM = (REMOVE_DMM_REACTOR)GetProcAddress(hDmm, "AcGlobRemoveDMMReactor");
                if (pRemDMM) pRemDMM(g_pMMReactor);
            }
            delete g_pMMReactor;
            g_pMMReactor = nullptr;
        }
    }   

    void OnCancelledOrFailedPublishing(AcPublishReactorInfo* pInfo) override {
        acutPrintf(L"\n[PublishReactor] Publishing cancelled/failed.");
        if (g_pMMReactor) {
            HINSTANCE hDmm = ::GetModuleHandle(EPLOT_ARXNAME);
            if (hDmm) {
                using REMOVE_DMM_REACTOR = void(*)(AcDMMReactor*);
                REMOVE_DMM_REACTOR pRemDMM = (REMOVE_DMM_REACTOR)GetProcAddress(hDmm, "AcGlobRemoveDMMReactor");
                if (pRemDMM) pRemDMM(g_pMMReactor);
            }
            delete g_pMMReactor;
            g_pMMReactor = nullptr;
        }
        
    }
};

class MyPublishUIReactor : public AcPublishUIReactor
{
public:
    MyPublishUIReactor() {}

    void OnInitPublishOptionsDialog(IUnknown** pUnk, AcPublishUIReactorInfo* pInfo) override {
        acutPrintf(L"\n[UIReactor] Init Publish Options Dialog.");

        // You can QueryInterface on *pUnk to get IPropertyManager2
        // Add your own UI controls or metadata settings here if needed
        // For example: store user settings in pInfo->GetUnrecognizedDSDData() or similar
    }

    void OnInitExportOptionsDialog(IUnknown** pUnk, AcPublishUIReactorInfo* pInfo) override {
        acutPrintf(L"\n[UIReactor] Init Export to DWF/PDF Options Dialog.");

        // For quick export UI (like in Publish to DWF or PDF)
        // Registry data is stored in: HKEY_CURRENT_USER\...\Dialogs\AcQuickPublishOpts
    }

    void OnInitAutoPublishSettingsDialog(IUnknown** pUnk, AcPublishUIReactorInfo* pInfo) override {
        acutPrintf(L"\n[UIReactor] Init Auto Publish Settings Dialog.");

        // Auto publish is typically used for background publishing on save
        // Registry data stored in: HKEY_CURRENT_USER\...\Dialogs\AcAutoPublishOpts
    }

    void PersistenceRegistryForExportOptionsDialog(const ACHAR* lpszRegRoot, bool bIsReadRegistry) override {
        acutPrintf(L"\n[UIReactor] %s Export Options to Registry: %s",
            bIsReadRegistry ? _T("Reading") : _T("Saving"), lpszRegRoot);

        // If you added settings in the dialog, persist them here manually
    }

    void PersistenceRegistryForAutoPublishDialog(const ACHAR* lpszRegRoot, bool bIsReadRegistry) override {
        acutPrintf(L"\n[UIReactor] %s Auto Publish Options to Registry: %s",
            bIsReadRegistry ? _T("Reading") : _T("Saving"), lpszRegRoot);

        // Handle custom user options for AutoPublish UI
    }

    void PersistenceRegistryForPublishOptionsDialog(const ACHAR* lpszRegRoot, bool bIsReadRegistry) override {
        acutPrintf(L"\n[UIReactor] %s Main Publish Options to Registry: %s",
            bIsReadRegistry ? _T("Reading") : _T("Saving"), lpszRegRoot);

        // Optional: override if you persist options from the full publish dialog
    }

    ~MyPublishUIReactor() override {}
};



void RegisterReactors()
{
    // Load & Register Publish Core Reactor
    if (!acrxServiceIsRegistered(PUBLISH_DLL))
        acrxLoadModule(PUBLISH_DLL, false, false);

    HINSTANCE hPublish = ::GetModuleHandle(PUBLISH_DLL);
    if (!hPublish) {
        acutPrintf(L"\n[ERROR] Failed to load AcPublish.crx");
        return;
    }

    using ADD_PUB_REACTOR = void(*)(AcPublishReactor*);
    ADD_PUB_REACTOR pAddPubReactor = (ADD_PUB_REACTOR)GetProcAddress(hPublish, "AcGlobAddPublishReactor");
    if (pAddPubReactor) {
        g_pPubReactor = new MyPublishReactor();
        pAddPubReactor(g_pPubReactor);
        acutPrintf(L"\n[Register] AcPublishReactor registered.");
    }
    

    // Load & Register Publish UI Reactor
    acrxLoadModule(PUBLISH_UI_DLL, false, false);
    HINSTANCE hPubUI = ::GetModuleHandle(PUBLISH_UI_DLL);

    if (hPubUI) {
        using ADD_PUB_UI_REACTOR = void(*)(AcPublishUIReactor*);
        ADD_PUB_UI_REACTOR pAddPubUI = (ADD_PUB_UI_REACTOR)GetProcAddress(hPubUI, "AcGlobAddPublishUIReactor");
        if (pAddPubUI) {
            g_pPubUIReactor = new MyPublishUIReactor();
            pAddPubUI(g_pPubUIReactor);
            acutPrintf(L"\n[Register] AcPublishUIReactor registered.");
        }
    }
    
   
}

void UnregisterReactors()
{
   
    // Publish Reactor
    if (g_pPubReactor) {
        HINSTANCE hPublish = ::GetModuleHandle(PUBLISH_DLL);
        if (hPublish) {
            using REMOVE_PUB_REACTOR = void(*)(AcPublishReactor*);
            REMOVE_PUB_REACTOR pRemPub = (REMOVE_PUB_REACTOR)GetProcAddress(hPublish, "AcGlobRemovePublishReactor");
            if (pRemPub) pRemPub(g_pPubReactor);
        }
        delete g_pPubReactor;
        g_pPubReactor = nullptr;
    }

    // Publish UI Reactor
    if (g_pPubUIReactor) {
        HINSTANCE hPubUI = ::GetModuleHandle(PUBLISH_UI_DLL);
        if (hPubUI) {
            using REMOVE_PUB_UI_REACTOR = void(*)(AcPublishUIReactor*);
            REMOVE_PUB_UI_REACTOR pRemPubUI = (REMOVE_PUB_UI_REACTOR)GetProcAddress(hPubUI, "AcGlobRemovePublishUIReactor");
            if (pRemPubUI) pRemPubUI(g_pPubUIReactor);
        }
        delete g_pPubUIReactor;
        g_pPubUIReactor = nullptr;
    }
}

extern "C" AcRx::AppRetCode
acrxEntryPoint(AcRx::AppMsgCode msg, void* appId)
{
    switch (msg)
    {
    case AcRx::kInitAppMsg:
        acrxUnlockApplication(appId);
        acrxRegisterAppMDIAware(appId);
        RegisterReactors();       
        break;

    case AcRx::kUnloadAppMsg:
        UnregisterReactors();         
        break;
    }

    return AcRx::kRetOK;
}
