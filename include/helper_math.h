#pragma once
#include "helper_game.h"

#include <algorithm>
#include <numbers>

namespace helper
{
	using namespace RE;

	const bool TestBoxCollision(NiTransform &a_origin, NiPoint3 &a_extent, NiPoint3 &a_collider);
	
	RE::NiColor HSV_to_RGB(float h, float s, float v);

	RE::NiPoint3 RGBtoHSV(NiColor rgb);

	RE::NiPoint3 RGBtoHSV(NiColorA rgb);

	float deg2rad(const float d);

	float GetAzimuth(NiMatrix3& rot);

	float GetElevation(NiMatrix3& rot);

	float VectorLengthSquared(const NiPoint3& vec);

	float VectorLength(const NiPoint3& vec);

	float DotProductSafe(const NiPoint3& vec1, const NiPoint3& vec2);

	NiPoint3 VectorNormalized(const NiPoint3& vec);

	NiMatrix3 getRotationAxisAngle(NiPoint3& axis, float theta);

	NiMatrix3 RotateBetweenVectors(const NiPoint3& src, const NiPoint3& dest);

	void FaceCamera(RE::NiAVObject* a_target, bool a_x = false, bool a_y = false, bool a_z = true,
		RE::NiPoint3 a_target_normal = { 1.0, 0.0, 0.0 });

	NiMatrix3 slerpMatrixAdaptive(NiMatrix3 mat1, NiMatrix3 mat2);

	void Quat2Mat(NiMatrix3& matrix, NiQuaternion& quaternion);

	void slerpQuat(float interp, NiQuaternion& q1, NiQuaternion& q2, NiMatrix3& out);

}
