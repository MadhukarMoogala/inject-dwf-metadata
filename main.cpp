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

class TstMMReactor* g_pMMReactor = nullptr;
class MyPublishReactor* g_pPubReactor = nullptr;

class TstMMReactor : public AcDMMReactor {
public:
    void OnBeginSheet(AcDMMSheetReactorInfo* pInfo) override {
        AcDMMEPlotProperty prop(_T("LayoutId"), pInfo->UniqueLayoutId());
        prop.SetCategory(_T("APS"));
        AcDMMEPlotPropertyVec properties;
        properties.append(prop);
        pInfo->AddPageProperties(properties);
    }

    void OnBeginEntity(AcDMMEntityReactorInfo* pInfo) override {
        AcDbEntity* pEntity = pInfo->entity();
        if (!pEntity) return;

        AcDbObjectId objId = pEntity->objectId();
        AcDbObjectIdArray objIdArray = pInfo->getEntityBlockRefPath();
        AcDMMEPlotProperties props;
        int nProps = 0;

        if (AcDbBlockReference* pBlkRef = AcDbBlockReference::cast(pEntity)) {
            AcDbBlockTableRecordPointer pBTR(pBlkRef->blockTableRecord(), AcDb::kForRead);
            if (eOkVerify(pBTR.openStatus())) {
                const ACHAR* blkName = nullptr;
                if (pBTR->getName(blkName) == Acad::eOk && blkName) {
                    AcDMMEPlotProperty prop(L"BlockName", blkName);
                    prop.SetCategory(L"Geometry");
                    props.AddProperty(&prop); nProps++;
                }
            }
        }

        AcDbLayerTableRecordPointer pLayer(pEntity->layerId(), AcDb::kForRead);
        if (eOkVerify(pLayer.openStatus())) {
            const ACHAR* layerName = nullptr;
            if (pLayer->getName(layerName) == Acad::eOk && layerName) {
                AcDMMEPlotProperty prop(L"Layer", layerName);
                prop.SetCategory(L"Display");
                props.AddProperty(&prop); nProps++;
            }
        }

        AcDbExtents ext;
        if (pEntity->getGeomExtents(ext) == Acad::eOk) {
            double area = (ext.maxPoint().x - ext.minPoint().x) *
                (ext.maxPoint().y - ext.minPoint().y);
            AcDMMEPlotProperty prop(L"ApproxArea", std::to_wstring(area).c_str());
            prop.SetCategory(L"Geometry");
            props.AddProperty(&prop); nProps++;
        }

        AcDbHandle handle;
        pEntity->getAcDbHandle(handle);
        TCHAR handleStr[20];
        handle.getIntoAsciiBuffer(handleStr);
        AcDMMEPlotProperty propHandle(L"EntityHandle", handleStr);
        propHandle.SetCategory(L"Debug");
        props.AddProperty(&propHandle); nProps++;

        if (nProps == 0) return;

        const wchar_t* wsUnique = pInfo->UniqueEntityId();
        AcDMMWideString wsPropId = L"ACAD-";
        wsPropId += wsUnique;
        delete[] wsUnique;

        props.SetId(wsPropId);
        pInfo->AddProperties(&props);

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

        if (pNode) {
            AcDMMStringVec idVec;
            idVec.append(wsPropId);
            pInfo->AddPropertiesIds(&idVec, *(AcDMMNode*)pNode);
        }

        if (bNewNode && pNode)
            delete pNode;
    }

    void OnEndEntity(AcDMMEntityReactorInfo* pInfo) override {
        pInfo->GetPlotLogger()->logMessage(_T("[MMReactor] Finished entity."));
    }

    void OnEndSheet(AcDMMSheetReactorInfo* pInfo) override {
        AcString msg;
        msg.format(_T("[MMReactor] Finished: %s"), pInfo->UniqueLayoutId());
        pInfo->GetPlotLogger()->logMessage(msg);

        AcDMMResourceVec resVec;
        resVec.append(AcDMMResourceInfo(L"Help file", L"text/html", L"c:\\help.html"));
        pInfo->AddPageResources(resVec);
    }
};

class MyPublishReactor : public AcPublishReactor {
public:
    void OnAboutToBeginPublishing(AcPublishBeginJobInfo* pInfo) override {
        acutPrintf(L"\n[PublishReactor] About to begin publishing...");
        if (!acrxServiceIsRegistered(EPLOT_SERVICENAME))
            acrxLoadModule(EPLOT_ARXNAME, false, false);

        if (HINSTANCE hDmm = ::GetModuleHandle(EPLOT_ARXNAME)) {
            using ADD_DMM_REACTOR = void(*)(AcDMMReactor*);
            if (auto pAddDMM = (ADD_DMM_REACTOR)GetProcAddress(hDmm, "AcGlobAddDMMReactor")) {
                g_pMMReactor = new TstMMReactor();
                pAddDMM(g_pMMReactor);
                acutPrintf(L"\n[Register] AcDMMReactor registered.");
            }
        }
    }

    void CleanupDMMReactor() {
        if (g_pMMReactor) {
            if (HINSTANCE hDmm = ::GetModuleHandle(EPLOT_ARXNAME)) {
                using REMOVE_DMM_REACTOR = void(*)(AcDMMReactor*);
                if (auto pRemDMM = (REMOVE_DMM_REACTOR)GetProcAddress(hDmm, "AcGlobRemoveDMMReactor"))
                    pRemDMM(g_pMMReactor);
            }
            delete g_pMMReactor;
            g_pMMReactor = nullptr;
        }
    }

    void OnEndPublish(AcPublishReactorInfo*) override {
        acutPrintf(L"\n[PublishReactor] Publishing ended.");
        CleanupDMMReactor();
    }

    void OnCancelledOrFailedPublishing(AcPublishReactorInfo*) override {
        acutPrintf(L"\n[PublishReactor] Publishing cancelled/failed.");
        CleanupDMMReactor();
    }
};

void RegisterReactors() {
    if (!acrxServiceIsRegistered(PUBLISH_DLL))
        acrxLoadModule(PUBLISH_DLL, false, false);

    if (HINSTANCE hPublish = ::GetModuleHandle(PUBLISH_DLL)) {
        using ADD_PUB_REACTOR = void(*)(AcPublishReactor*);
        if (auto pAddPubReactor = (ADD_PUB_REACTOR)GetProcAddress(hPublish, "AcGlobAddPublishReactor")) {
            g_pPubReactor = new MyPublishReactor();
            pAddPubReactor(g_pPubReactor);
            acutPrintf(L"\n[Register] AcPublishReactor registered.");
        }
    }
}

void UnregisterReactors() {
    if (g_pPubReactor) {
        if (HINSTANCE hPublish = ::GetModuleHandle(PUBLISH_DLL)) {
            using REMOVE_PUB_REACTOR = void(*)(AcPublishReactor*);
            if (auto pRemPub = (REMOVE_PUB_REACTOR)GetProcAddress(hPublish, "AcGlobRemovePublishReactor"))
                pRemPub(g_pPubReactor);
        }
        delete g_pPubReactor;
        g_pPubReactor = nullptr;
    }
}

extern "C" AcRx::AppRetCode acrxEntryPoint(AcRx::AppMsgCode msg, void* appId) {
    switch (msg) {
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
