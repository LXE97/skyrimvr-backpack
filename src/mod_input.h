#pragma once
#include "VR/PapyrusVRAPI.h"

namespace vrinput
{
    enum whichController { Left = true, Right = false };
    enum touchOrPress { Touch = true, Press = false };
    enum buttonupOrButtondown { ButtonDown = true, ButtonUp = false };

    constexpr std::array all_buttons{
        vr::k_EButton_System,
        vr::k_EButton_ApplicationMenu, // aka B button
        vr::k_EButton_Grip,
        vr::k_EButton_DPad_Left,
        vr::k_EButton_DPad_Up,
        vr::k_EButton_DPad_Right,
        vr::k_EButton_DPad_Down,
        vr::k_EButton_A,
        vr::k_EButton_ProximitySensor,
        vr::k_EButton_SteamVR_Touchpad,
        vr::k_EButton_SteamVR_Trigger
    };

    // returns: true if input that triggered the event should be blocked
    // Decision to block controller input to the game is made inside the callback, because we don't want to block it
    // if e.g. a menu is open or the action bound to this input is not applicable.
    // If the press is blocked, then we don't need to block the release
    typedef bool (*InputCallbackFunc)();

    void AddCallback(const vr::EVRButtonId button, InputCallbackFunc callback, bool isLeft, bool onTouch, bool onButtonDown);
    void RemoveCallback(const vr::EVRButtonId button, InputCallbackFunc callback, bool isLeft, bool onTouch, bool onButtonDown);

    // Must be called by the ControllerStateCB registered to the OpenVR Hook Manager when a change in button states is detected
    void processButtonChanges(uint64_t changedMask, uint64_t currentState, bool isLeft, bool touch, vr::VRControllerState_t* out);
}
