#include "hooks.h"

namespace hooks
{

	void PostWandUpdateHook() { backpackvr::PostWandUpdate(); }

	// thanks to FlyingParticle for this code
	// A call shortly after the Higgs call shortly after the wand nodes are updated as part of Main::Draw()
	auto postWandUpdateHookLoc = RelocAddr<uintptr_t>(0x13233C7);
	auto postWandUpdateHookedFunc = RelocAddr<uintptr_t>(0xDCF900);

	void Install()
	{
		{
			struct Code : Xbyak::CodeGenerator
			{
				Code()
				{
                    Xbyak::Label hookLabel;
					Xbyak::Label hookedFuncLabel;
                    Xbyak::Label retnLabel;

					// Original code
					call(ptr[rip + hookedFuncLabel]);

					push(rax);
					// Need to keep the stack 16 byte aligned, and an additional 0x20 bytes for scratch space
					sub(rsp, 0x38);
					movsd(ptr[rsp + 0x20], xmm0);

					// Call our hook
					call(ptr[rip + hookLabel]);

					movsd(xmm0, ptr[rsp + 0x20]);
					add(rsp, 0x38);
					pop(rax);

					jmp(ptr[rip + retnLabel]);

                    L(hookLabel), dq(reinterpret_cast<uintptr_t>(&PostWandUpdateHook));
					L(hookedFuncLabel), dq(postWandUpdateHookedFunc);
                    L(retnLabel), dq(postWandUpdateHookLoc + 5);
				}
			};
			Code code;
			code.ready();

			auto& trampoline = SKSE::GetTrampoline();

			trampoline.write_branch<5>(
				postWandUpdateHookLoc.GetUIntPtr(), trampoline.allocate(code));

			SKSE::log::trace("Post Wand Update hook complete");
		}
	}
}