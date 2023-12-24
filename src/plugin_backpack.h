#pragma once

#include "higgsinterface001.h"
#include "vrikinterface001.h"
#include "RE/N/NiRTTI.h"
#include "SKSE/Impl/Stubs.h"
#include "VR/PapyrusVRAPI.h"
#include "VR/VRManagerAPI.h"
#include "VR/OpenVRUtils.h"
#include "VRInteractionSphere.h"

#include "mod_input.h"
#include "mod_eventSink.hpp"

namespace backpack
{
    extern SKSE::detail::SKSETaskInterface* g_task;
    extern OpenVRHookManagerAPI* g_OVRHookManager;
    extern PapyrusVR::VRManagerAPI* g_VRManager;
    extern vr::TrackedDeviceIndex_t g_l_controller;
    extern vr::TrackedDeviceIndex_t g_r_controller;

    void StartMod();
    void GameLoad();
    void PreGameLoad();

    bool onDEBUGBtnPressA();
    bool onDEBUGBtnReleaseB();
    bool onDEBUGBtnPressB();
    void onEquipEvent(const RE::TESEquipEvent* event);
    void onOverlap(const vrinput::OverlapEvent& e);

    // low level input
    bool ControllerInput_CB(vr::TrackedDeviceIndex_t unControllerDeviceIndex, const vr::VRControllerState_t* pControllerState, uint32_t unControllerStateSize, vr::VRControllerState_t* pOutputControllerState);
    void RegisterVRInputCallback();
}
