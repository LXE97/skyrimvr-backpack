#pragma once
#include "VR/PapyrusVRAPI.h"
#include "helper_math.h"
#include "xbyak/xbyak.h"

namespace vrinput
{
	constexpr const char* kLeftHandNodeName = "NPC L Hand [LHnd]";
	constexpr const char* kRightHandNodeName = "NPC R Hand [RHnd]";
	constexpr const char* kControllerNodeName[2] = { kRightHandNodeName, kLeftHandNodeName };

	constexpr std::array all_buttons{
		vr::k_EButton_System,
		vr::k_EButton_Knuckles_B,
		vr::k_EButton_Grip,
		vr::k_EButton_A,
		vr::k_EButton_SteamVR_Touchpad,  // this is the joystick for index  check oculus
		vr::k_EButton_SteamVR_Trigger,
	};

	/* opencomposite automatically generates dpad events for joystick movement, but SteamVR doesn't,
	* so these are treated separately and generated from joystick axes internally */
	constexpr std::array dpad{
		vr::k_EButton_DPad_Left,
		vr::k_EButton_DPad_Up,
		vr::k_EButton_DPad_Right,
		vr::k_EButton_DPad_Down,
	};

	enum class Hand
	{
		kRight = 0,
		kLeft,
		kBoth
	};
	enum class ActionType
	{
		kPress = 0,  // dpad(joystick) events are always kPress
		kTouch
	};
	enum class ButtonState
	{
		kButtonUp = 0,
		kButtonDown
	};

	struct ModInputEvent
	{
		Hand            device;
		ActionType      touch_or_press;
		ButtonState     button_state;
		vr::EVRButtonId button_ID;

		bool operator==(const ModInputEvent& a_rhs)
		{
			return (device == a_rhs.device) && (touch_or_press == a_rhs.touch_or_press) &&
				(button_state == a_rhs.button_state);
		}
	};

	/* returns: true if input that triggered the event should be blocked
	* Decision to block controller input to the game is made inside the callback, because we don't
	* want to block it if e.g. a menu is open or the action bound to this input is not applicable.
	* If the press is blocked, then we don't need to block the release
	*/
	typedef bool (*InputCallbackFunc)(const ModInputEvent& e);

	void StartBlockingAll();
	void StopBlockingAll();
	bool isBlockingAll();

	void StartSmoothing();
	void StopSmoothing();

	/* returns the state of the specified button's action type */
	ButtonState GetButtonState(
		vr::EVRButtonId a_button_ID, Hand a_hand, ActionType a_touch_or_press);

	const float                   GetTrigger(Hand a);
	const vr::VRControllerAxis_t& GetJoystick(Hand a);

	inline Hand GetOtherHand(Hand a)
	{
		return a == Hand::kRight ? Hand::kLeft : (a == Hand::kLeft ? Hand::kRight : Hand::kBoth);
	}

	inline RE::NiAVObject* GetHandNode(Hand a_hand, bool a_first_person)
	{
		if (auto pc3d = RE::PlayerCharacter::GetSingleton()->Get3D(a_first_person))
		{
			return pc3d->GetObjectByName(
				a_hand == Hand::kRight ? kRightHandNodeName : kLeftHandNodeName);
		}
		return nullptr;
	}

	/* Adds a function to the list of callbacks for a specific button. The callback will be triggered
	* on press and release.
	*/
	void AddCallback(const InputCallbackFunc a_callback, const vr::EVRButtonId a_button_ID,
		const Hand a_hand, const ActionType a_touch_or_press);
	void RemoveCallback(const InputCallbackFunc a_callback, const vr::EVRButtonId a_button_ID,
		const Hand a_hand, const ActionType a_touch_or_press);

	/* Adds a timed callback that triggers if the button is held for the specific duration. 
	* Callbacks added while the button is held down will not take effect until it's pressed again.
	* a_duration: duration in milliseconds 
	*/
	void AddHoldCallback(const InputCallbackFunc a_callback,
		const std::chrono::milliseconds a_duration, const vr::EVRButtonId a_button_ID,
		const Hand a_hand, const ActionType a_touch_or_press);

	/* Removes all timed callbacks from the button that match the flags (i.e. duration is not considered)
	*/
	void RemoveHoldCallback(const InputCallbackFunc a_callback, const vr::EVRButtonId a_button_ID,
		const Hand a_hand, const ActionType a_touch_or_press);

	// Emulated input functions-- To emulate a button press, 2 events must be sent (touch and press)

	/* Sets an override on the button state that gets sent to Skyrim. Other mods will not see this */
	void SetFakeButtonState(const ModInputEvent a_event);

	/* Clears an override, otherwise fake button state persists forever. Does nothing if no override exists */
	void ClearFakeButtonState(const ModInputEvent a_event);

	void ClearAllFake();

	/* Sets a momentary button state, the button will be returned to its true state in the next update.
	*/
	void SendFakeInputEvent(const ModInputEvent a_event);

	void InitControllerHooks();

	void Vibrate(bool isLeft, std::vector<uint16_t>* keyframes, float a_power = 1.f);

	/* This needs to be fed to OVRHookManager::RegisterControllerStateCB() */
	bool ControllerInputCallback(vr::TrackedDeviceIndex_t unControllerDeviceIndex,
		const vr::VRControllerState_t* pControllerState, uint32_t unControllerStateSize,
		vr::VRControllerState_t* pOutputControllerState);

	vr::EVRCompositorError ControllerPoseCallback(VR_ARRAY_COUNT(unRenderPoseArrayCount)
													  vr::TrackedDevicePose_t* pRenderPoseArray,
		uint32_t                                                      unRenderPoseArrayCount,
		VR_ARRAY_COUNT(unGamePoseArrayCount) vr::TrackedDevicePose_t* pGamePoseArray,
		uint32_t                                                      unGamePoseArrayCount);

	extern vr::TrackedDeviceIndex_t g_leftcontroller;
	extern vr::TrackedDeviceIndex_t g_rightcontroller;
	extern float                    adjustable;
	extern vr::IVRSystem*           g_IVRSystem;

}
