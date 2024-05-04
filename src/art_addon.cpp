#include "art_addon.h"

#include "helper_game.h"
#include "helper_math.h"

#include <codecvt>
#include <filesystem>
#include <locale>
#include <string>

namespace art_addon
{
	using namespace RE;

	std::shared_ptr<ArtAddon> ArtAddon::Make(const char* a_model_path, TESObjectREFR* a_target,
		NiAVObject* a_attach_node, NiTransform& a_local, std::function<void(ArtAddon*)> a_callback)
	{
		auto manager = ArtAddonManager::GetSingleton();
		auto art_object = manager->GetArtForm(a_model_path);
		int  id = manager->GetNextId();

		/** Using the duration parameter of the BSTempEffect as an ID because anything < 0 has the
		 * same effect. */
		if (art_object && a_attach_node && a_target && a_target->IsHandleValid() &&
			a_target->ApplyArtObject(art_object, (float)id))
		{
			auto new_obj = std::shared_ptr<ArtAddon>(new ArtAddon);
			new_obj->art_object = art_object;
			new_obj->local = a_local;
			new_obj->attach_node = a_attach_node;
			new_obj->target = a_target;
			if (a_callback) { new_obj->callback = a_callback; }

			manager->new_objects.emplace(id, new_obj);
			return new_obj;
		}
		else
		{
			SKSE::log::error("Art addon failed: invalid target or model path");
			return nullptr;
		}
	}

	void RemoveCollisionNodes(NiAVObject* a_node)
	{
		if (a_node != nullptr)
		{
			if (a_node->collisionObject)
			{
				SKSE::log::trace("Removing collision from {}", a_node->name.c_str());
				a_node->collisionObject = nullptr;
			}

			if (auto ninode = a_node->AsNode())
			{
				for (auto c : ninode->GetChildren()) { RemoveCollisionNodes(c.get()); }
			}
		}
	}

	/** clone the created NiNode and then delete the ModelReferenceEffect and the original NiNode
	 *  using the ProcessList. */
	void ArtAddonManager::Update()
	{
		if (!new_objects.empty())
		{
			std::scoped_lock lock(objects_lock);
			if (const auto processLists = ProcessLists::GetSingleton())
			{
				processLists->ForEachModelEffect([this](ModelReferenceEffect& a_modelEffect) {
					if (a_modelEffect.Get3D() && a_modelEffect.lifetime < -1.0f)
					{
						int id = a_modelEffect.lifetime;
						if (new_objects.contains(id))
						{
							if (auto addon = new_objects[id].lock())
							{
								// the id is not unique to this mod but the ArtObject is
								if (addon->art_object == a_modelEffect.artObject)
								{
									addon->root3D = a_modelEffect.Get3D()->Clone();
									addon->attach_node->AsNode()->AttachChild(addon->root3D);
									a_modelEffect.lifetime = 0;
									addon->root3D->local = std::move(addon->local);

									// .nifs with collision will not be drawn when they're attached to an actor
									RemoveCollisionNodes(addon->root3D);

									if (addon->callback) { addon->callback(addon.get()); }
								}
							}
							else
							{  // check if it's one of our ArtObjects
								auto it = std::find_if(artobject_cache.begin(),
									artobject_cache.end(), [&a_modelEffect](const auto& pair) {
										return pair.second == a_modelEffect.artObject;
									});
								if (it != artobject_cache.end())
								{  // the artAddon was deleted before initialization finished
									a_modelEffect.lifetime = 0;
									SKSE::log::trace("deleting MRE {} (orphaned)", id);
								}
							}
							// finished with this ArtAddon, no longer need to track it
							new_objects.erase(id);
						}
					}
					return BSContainer::ForEachResult::kContinue;
				});
			}
		}
	}

	BGSArtObject* ArtAddonManager::GetArtForm(const char* a_model_path)
	{
		if (!artobject_cache.contains(a_model_path))
		{
			if (base_artobject)
			{
				if (auto dupe = base_artobject->CreateDuplicateForm(false, nullptr))
				{
					auto temp = dupe->As<BGSArtObject>();
					temp->SetModel(a_model_path);
					artobject_cache[a_model_path] = temp;
					return temp;
				}
				else { SKSE::log::error("error creating form for: {}", a_model_path); }
			}
			else { SKSE::log::error("base ArtObject not found"); }

			return nullptr;
		}
		else { return artobject_cache[a_model_path]; }
	}

	int ArtAddonManager::GetNextId() { return next_id--; }

	ArtAddonManager::ArtAddonManager()
	{
		constexpr FormID kBaseArtobjectId = 0x9405f;

		base_artobject = TESForm::LookupByID(kBaseArtobjectId)->As<BGSArtObject>();
	}

	NifChar::NifChar(char a_ascii, RE::NiAVObject* a_parent, RE::NiTransform& a_local) :
		ascii(a_ascii)
	{
		artaddon = ArtAddon::Make(kFontModelPath, PlayerCharacter::GetSingleton()->AsReference(),
			a_parent, a_local, std::bind(&NifChar::OnModelCreation, this));
	}

	void NifChar::OnModelCreation()
	{
		if (auto shader = helper::GetShaderProperty(artaddon->Get3D(), kNodeName))
		{
			auto oldmat = shader->material;
			auto newmat = oldmat->Create();
			newmat->CopyMembers(oldmat);
			shader->material = newmat;
			newmat->IncRef();
			oldmat->DecRef();

			auto temp = AsciiToXY(ascii);

			newmat->texCoordOffset[0].x = temp.x;
			newmat->texCoordOffset[0].y = temp.y;
			newmat->texCoordOffset[1].x = temp.x;
			newmat->texCoordOffset[1].y = temp.y;
		}
	}

	AddonTextBox::AddonTextBox(const char* a_string, const float a_spacing,
		RE::NiAVObject* a_attach_to, RE::NiTransform& a_world) :
		string(a_string),
		spacing(a_spacing),
		world(a_world)
	{
		RE::NiTransform t;
		root =
			ArtAddon::Make(NifChar::kFontModelPath, PlayerCharacter::GetSingleton()->AsReference(),
				a_attach_to, t, std::bind(&AddonTextBox::MakeString, this));
	}

	void AddonTextBox::MakeString()
	{
		if (auto root_node = root->Get3D())
		{
			NiTransform t;
			for (int i = 0; string[i] != '\0'; i++)
			{
				characters.push_back(std::make_unique<NifChar>(string[i], root_node, t));
				t.translate.y += NifChar::kCharacterWidth + spacing;
			}
			NiUpdateData ctx;
			root_node->Update(ctx);

			helper::FaceCamera(root_node, true, true, true);
			root_node->local.translate =
				root_node->parent->world.rotate.Transpose() * world.translate;
		}
	}

	void DebugCreateSkeletonNodesAttached(std::vector<std::string>& a_nodenames,
		std::vector<ArtAddonPtr> a_art, bool a_first_person, int a_color_hex)
	{
		RE::NiTransform temp;
		auto            pc = RE::PlayerCharacter::GetSingleton();
		for (auto str : a_nodenames)
		{
			if (auto pnode = pc->Get3D(a_first_person)->GetObjectByName(str))
			{
				a_art.push_back(ArtAddon::Make("wearable/HelperSphere.nif", pc->AsReference(),
					pnode, temp, [a_color_hex](ArtAddon* a) {
						helper::SetGlowColor(a->Get3D(), a_color_hex);
						SKSE::log::trace("artaddon created on {}", a->GetParent()->name.c_str());
					}));
			}
		}
	}

	void DebugCreateSkeletonNodesFloating(std::vector<std::string>& a_nodenames,
		std::vector<ArtAddonPtr> a_art, bool a_first_person, int a_color_hex)
	{
		RE::NiTransform temp;
		auto            pc = RE::PlayerCharacter::GetSingleton();
		for (auto str : a_nodenames)
		{
			a_art.push_back(ArtAddon::Make("wearable/HelperSphere.nif", pc->AsReference(),
				pc->Get3D(a_first_person), temp, [a_color_hex](ArtAddon* a) {
					helper::SetGlowColor(a->Get3D(), a_color_hex);
					SKSE::log::trace("artaddon created on {}", a->GetParent()->name.c_str());
				}));
		}
	}

	void DebugUpdateSkeletonNodes(
		std::vector<std::string>& a_nodenames, std::vector<ArtAddonPtr> a_art, bool a_first_person)
	{
		if (a_nodenames.size() == a_art.size())
		{
			if (auto p3d = RE::PlayerCharacter::GetSingleton()->Get3D(a_first_person))
			{
				for (int i = 0; i < a_art.size(); i++)
				{
					if (auto pnode = p3d->GetObjectByName(a_nodenames[i]))
					{
						helper::SetWorldPosition(
							a_art[i]->Get3D(), a_art[i]->GetParent(), pnode->world.translate);
					}
				}
			}
		}
	}

	void DebugGetVRNodes(std::vector<NiPointer<NiNode>>* out)
	{
		if (auto pc = PlayerCharacter::GetSingleton()->GetVRNodeData())
		{
#define vr(X) out->push_back(pc->X);

			

				vr(RightWandNode)                 /* 4F8 */

				vr(ArrowSnapNode)                 /* 5D8 */

		}
	}

	void DebugGetVRNodeStrings(std::vector<std::string>* out)
	{
		if (auto pc = PlayerCharacter::GetSingleton()->GetVRNodeData())
		{
#define vr(X) out->push_back("#X");

			vr(FollowNode)                        /* 3F8 */
				vr(FollowOffset)                  /* 400 */
				vr(HeightOffsetNode)              /* 408 */
				vr(SnapWalkOffsetNode)            /* 410 */
				vr(RoomNode)                      /* 418 */
				vr(BlackSphere)                   /* 420 */
				vr(uiNode)                        /* 428 */
				vr(UIPointerNode)                 /* 438 */
				vr(DialogueUINode)                /* 448 */
				vr(TeleportDestinationPreview)    /* 450 */
				vr(TeleportDestinationFail)       /* 458 */
				vr(TeleportSprintPreview)         /* 460 */
				vr(SpellOrigin)                   /* 468 */
				vr(SpellDestination)              /* 470 */
				vr(ArrowOrigin)                   /* 478 */
				vr(ArrowDestination)              /* 480 */
				vr(QuestMarker)                   /* 488 */
				vr(LeftWandNode)                  /* 490 */
				vr(LeftValveIndexControllerNode)  /* 4A0 */
				vr(unkNode4A8)                    /* 4A8 */
				vr(LeftWeaponOffsetNode)          /* 4B0 */
				vr(LeftCrossbowOffsetNode)        /* 4B8 */
				vr(LeftMeleeWeaponOffsetNode)     /* 4C0 */
				vr(LeftStaffWeaponOffsetNode)     /* 4C8 */
				vr(LeftShieldOffsetNode)          /* 4D0 */
				vr(RightShieldOffsetNode)         /* 4D8 */
				vr(SecondaryMagicOffsetNode)      /* 4E0 */
				vr(SecondaryMagicAimNode)         /* 4E8 */
				vr(SecondaryStaffMagicOffsetNode) /* 4F0 */
				vr(RightWandNode)                 /* 4F8 */
				vr(RightWandShakeNode)            /* 500 */
				vr(RightValveIndexControllerNode) /* 508 */
				vr(unkNode510)                    /* 510 */
				vr(RightWeaponOffsetNode)         /* 518 */
				vr(RightCrossbowOffsetNode)       /* 520 */
				vr(RightMeleeWeaponOffsetNode)    /* 528 */
				vr(RightStaffWeaponOffsetNode)    /* 530 */
				vr(PrimaryMagicOffsetNode)        /* 538 */
				vr(PrimaryMagicAimNode)           /* 540 */
				vr(PrimaryStaffMagicOffsetNode)   /* 548 */
				vr(LastSyncPos)                   /* 578 */
				vr(UprightHmdNode)                /* 580 */
				vr(NPCLHnd)                       /* 590 */
				vr(NPCRHnd)                       /* 598 */
				vr(NPCLClv)                       /* 5A0 */
				vr(NPCRClv)                       /* 5A8 */
				vr(BowAimNode)                    /* 5C8 */
				vr(BowRotationNode)               /* 5D0 */
				vr(ArrowSnapNode)                 /* 5D8 */
				vr(ArrowHoldOffsetNode)           /* 5F8 */
				vr(ArrowHoldNode)                 /* 600 */
		}
	}

	void DebugDrawVRNodes(std::vector<NiPointer<NiNode>>& nodes, std::vector<ArtAddonPtr>* draw)
	{
		if (auto pc = RE::PlayerCharacter::GetSingleton()->Get3D(false))
		{
			RE::NiTransform temp;
			for (auto n : nodes)
			{
				draw->push_back(ArtAddon::Make("wearable/HelperSphere.nif",
					RE::PlayerCharacter::GetSingleton()->AsReference(), pc, temp));
			}
		}
		else { SKSE::log::trace("DebugDrawVRNodes failed no player 3d"); }
	}

	void DebugUpdateVRNodes(std::vector<NiPointer<NiNode>>& nodes, std::vector<ArtAddonPtr>& draw)
	{
		if (auto pc =
				RE::PlayerCharacter::GetSingleton()->Get3D(false) && nodes.size() == draw.size())
		{
			RE::NiTransform temp;
			for (int i = 0; i < nodes.size(); i++)
			{
				helper::SetWorldPosition(
					draw[i]->Get3D(), draw[i]->GetParent(), nodes[i]->world.translate);
			}
		}
	}

	void DebugShowVRNodeNames(std::vector<NiPointer<NiNode>>&                      nodes,
		std::vector<std::string>&                                                  names,
		std::unordered_map<std::string, std::unique_ptr<art_addon::AddonTextBox>>& text)
	{
		RE::PlayerCamera* camera = RE::PlayerCamera::GetSingleton();

		/*
		if (camera->cameraRoot->children.size() == 0) return;
		for (auto& entry : camera->cameraRoot->children)
		{
			auto asCamera = skyrim_cast<RE::NiCamera*>(entry.get());
			if (asCamera) camnode = NiPointer<RE::NiCamera>(asCamera);
		}*/
		if (RE::PlayerCharacter::GetSingleton()->Get3D(false))
		{
			if (auto camnode = camera->cameraRoot.get())
			{
				if (nodes.size() == names.size())
				{
					for (size_t i = 0; i < nodes.size(); i++)
					{
						auto dist = camnode->world.translate - nodes[i]->world.translate;
						if (dist.Length() < 8.f && !text[names[i]])
						{
							NiTransform t = nodes[i]->world;
							text[names[i]] = std::make_unique<AddonTextBox>(names[i].c_str(), 0.f,
								RE::PlayerCharacter::GetSingleton()->Get3D(false), t);
								SKSE::log::trace("create text box!");
						}
						else
						{
							if (text[names[i]]) { text[names[i]].reset(); }
						}
					}
				}
			}
		}
	}
}
