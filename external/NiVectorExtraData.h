#pragma once

#include "RE/N/NiExtraData.h"

namespace RE
{

	// 28
	class NiVectorExtraData : public NiExtraData
	{
	public:
		NiVectorExtraData();
		~NiVectorExtraData();

		float m_vector[4];  // 18
	};
}