#include "backpack.h"

namespace backpack
{
	std::string g_backpack_container_nodename = "Box";
	std::string g_backpack_container_extentname = "extent";
	int         g_mini_items_per_row = 5;
	float       g_mini_horizontal_spacing = 2;

	void Backpack::Init()
	{
		if (auto form = helper::GetForm(ref_id, g_mod_name)) { object = form->AsReference(); }
		if (auto form = helper::GetForm(wearer_ref_id, g_mod_name)) { wearer = form->AsReference(); }
		if (object) { base = object->GetObjectReference(); object->SetActivationBlocked(true);}
		item_view.clear();
	}

	void Backpack::CalculateMiniLayout()
	{
		auto& ml = mini_layout;

		if (object && object->Is3DLoaded())
		{
			if (auto container =
					object->GetCurrent3D()->GetObjectByName(g_backpack_container_nodename))
			{
				if (auto data =
						container->GetExtraData<NiVectorExtraData>(g_backpack_container_extentname))
				{
					ml.width = data->m_vector[1];
					ml.height = data->m_vector[2];
				}
			}
		}
		ml.item_desired_radius =
			(ml.width / g_mini_items_per_row - g_mini_horizontal_spacing) * 0.5;

		ml.items_per_column =
			std::floor(ml.height / (2 * ml.item_desired_radius + g_mini_horizontal_spacing));

		ml.vertical_spacing =
			(ml.height - ml.items_per_column * 2 * ml.item_desired_radius) / ml.items_per_column;
	}

}
