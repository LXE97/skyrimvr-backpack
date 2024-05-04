#include "helper_math.h"

namespace helper
{
	void RotateZ(RE::NiPoint3& target, RE::NiMatrix3& rotator)
	{
		float zangle = helper::GetAzimuth(rotator);
		float cosz = cos(zangle);
		float sinz = sin(zangle);
		target = { cosz * target[0] + sinz * target[1], cosz * target[1] - sinz * target[0],
			target[2] };
	}

		RE::NiPoint3 LinearInterp(const RE::NiPoint3& v1, const RE::NiPoint3& v2, float interp)
	{
		RE::NiPoint3 result;

		result.x = v1.x + interp * (v2.x - v1.x);
		result.y = v1.y + interp * (v2.y - v1.y);
		result.z = v1.z + interp * (v2.z - v1.z);

		return result;
	}
}
