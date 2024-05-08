#include "hooks.h"

namespace hooks
{

	void PostWandUpdateHook() { backpackvr::OnUpdate(); }

	// thanks to FlyingParticle for this code
	uintptr_t postWandUpdateHookedFuncAddr = 0;
	// A call shortly after the Higgs call shortly after the wand nodes are updated as part of Main::Draw()
	auto postWandUpdateHookLoc = RelocAddr<uintptr_t>(0x13233C7);
	auto postWandUpdateHookedFunc = RelocAddr<uintptr_t>(0xDCF900);

	void Install()
	{
		postWandUpdateHookedFuncAddr = postWandUpdateHookedFunc.GetUIntPtr();
		{
			struct Code : Xbyak::CodeGenerator
			{
				Code()
				{
					Xbyak::Label jumpBack;

					// Original code
					mov(rax, postWandUpdateHookedFuncAddr);
					call(rax);

					push(rax);
					// Need to keep the stack 16 byte aligned, and an additional 0x20 bytes for scratch space
					sub(rsp, 0x38);
					movsd(ptr[rsp + 0x20], xmm0);

					// Call our hook
					mov(rax, (uintptr_t)PostWandUpdateHook);
					call(rax);

					movsd(xmm0, ptr[rsp + 0x20]);
					add(rsp, 0x38);
					pop(rax);

					// Jump back to whence we came (+ the size of the initial branch instruction)
					jmp(ptr[rip + jumpBack]);

					L(jumpBack);
					dq(postWandUpdateHookLoc.GetUIntPtr() + 5);
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