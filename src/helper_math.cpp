#include "helper_math.h"

namespace helper
{
	using namespace RE;

	const bool TestBoxCollision(NiTransform& a_origin, NiPoint3& a_extent, NiPoint3& a_collider)
	{
		// get vector to test point and transform to box space
		auto diff = a_collider - a_origin.translate;
		diff = a_origin.rotate * diff;

		// test components
		return diff.x > 0 && diff.y > 0 && diff.z > 0 && diff.x < a_extent.x &&
			diff.y < a_extent.y && diff.z < a_extent.z;
	}

	void RotateZ(RE::NiPoint3& target, RE::NiMatrix3& rotator)
	{
		float zangle = helper::GetAzimuth(rotator);
		float cosz = cos(zangle);
		float sinz = sin(zangle);
		target = { cosz * target[0] + sinz * target[1], cosz * target[1] - sinz * target[0],
			target[2] };
	}

	RE::NiTransform WorldToLocal(RE::NiTransform& a_parent, RE::NiTransform& a_child)
	{
		NiTransform result;
		result.translate = a_parent.rotate * (a_child.translate - a_parent.translate);
		result.rotate = a_parent.rotate.Transpose() * a_child.rotate;
		return result;
	}

	RE::NiPoint3 LinearInterp(const RE::NiPoint3& v1, const RE::NiPoint3& v2, float interp)
	{
		RE::NiPoint3 result;

		result.x = v1.x + interp * (v2.x - v1.x);
		result.y = v1.y + interp * (v2.y - v1.y);
		result.z = v1.z + interp * (v2.z - v1.z);

		return result;
	}

	RE::NiColor HSV_to_RGB(float h, float s, float v)
	{
		h = std::fmod(h, 1.0f);
		s = std::clamp(s, 0.0f, 1.0f);
		v = std::clamp(v, 0.0f, 1.0f);

		int   hi = static_cast<int>(std::floor(h * 6.0)) % 6;
		float f = h * 6.0 - std::floor(h * 6.0);
		float p = v * (1.0 - s);
		float q = v * (1.0 - f * s);
		float t = v * (1.0 - (1.0 - f) * s);

		switch (hi)
		{
		case 0:
			return { v, t, p };
		case 1:
			return { q, v, p };
		case 2:
			return { p, v, t };
		case 3:
			return { p, q, v };
		case 4:
			return { t, p, v };
		default:
			return { v, p, q };
		}
	}

	RE::NiPoint3 RGBtoHSV(NiColor rgb)
	{
		float r = rgb.red;
		float g = rgb.green;
		float b = rgb.blue;
		float cmax = std::max({ r, g, b });
		float cmin = std::min({ r, g, b });
		float delta = cmax - cmin;

		// Calculate hue
		float h = 0.0;
		if (delta != 0.0)
		{
			if (cmax == r) { h = std::fmod((g - b) / delta, 6.0); }
			else if (cmax == g) { h = (b - r) / delta + 2.0; }
			else
			{  // cmax == b
				h = (r - g) / delta + 4.0;
			}
			h /= 6.0;
			if (h < 0.0) { h += 1.0; }
		}

		// Calculate saturation
		float s = (cmax != 0.0) ? delta / cmax : 0.0;

		// Calculate value
		float v = cmax;

		return { h, s, v };
	}

	RE::NiPoint3 RGBtoHSV(NiColorA rgb)
	{
		float r = rgb.red;
		float g = rgb.green;
		float b = rgb.blue;
		float cmax = std::max({ r, g, b });
		float cmin = std::min({ r, g, b });
		float delta = cmax - cmin;

		// Calculate hue
		float h = 0.0;
		if (delta != 0.0)
		{
			if (cmax == r) { h = std::fmod((g - b) / delta, 6.0); }
			else if (cmax == g) { h = (b - r) / delta + 2.0; }
			else
			{  // cmax == b
				h = (r - g) / delta + 4.0;
			}
			h /= 6.0;
			if (h < 0.0) { h += 1.0; }
		}

		// Calculate saturation
		float s = (cmax != 0.0) ? delta / cmax : 0.0;

		// Calculate value
		float v = cmax;

		return { h, s, v };
	}

	constexpr float deg2radC(const float d) { return d * 0.01745329f; }

	float deg2rad(const float d) { return d * 0.01745329f; }

	// get rotation about Z axis while ignoring other rotations
	float GetAzimuth(NiMatrix3& rot)
	{
		if (std::abs(rot.entry[2][1]) < 0.9995f)
		{
			return std::atan2(rot.entry[0][1], rot.entry[1][1]);
		}
		else { return -1.0f * std::atan2(rot.entry[1][0], rot.entry[0][0]); }
	}
	// same but X axis
	float GetElevation(NiMatrix3& rot)
	{
		return -1.0f * std::asin(std::clamp(rot.entry[2][1], -1.0f, 1.0f));
	}

	void RotateZ(NiPoint3& target, NiMatrix3& rotator);

	// vector stuff from higgs
	float VectorLengthSquared(const NiPoint3& vec)
	{
		return vec.x * vec.x + vec.y * vec.y + vec.z * vec.z;
	}
	float VectorLength(const NiPoint3& vec) { return sqrtf(VectorLengthSquared(vec)); }
	float DotProductSafe(const NiPoint3& vec1, const NiPoint3& vec2)
	{
		return std::clamp(vec1.Dot(vec2), -1.f, 1.f);
	}
	NiPoint3 VectorNormalized(const NiPoint3& vec)
	{
		float length = VectorLength(vec);
		return length > 0.0f ? vec / length : NiPoint3();
	}

	RE::NiPoint3 LinearInterp(const RE::NiPoint3& v1, const RE::NiPoint3& v2, float interp);

	// matrix stuff

	// Gets a rotation matrix from an axis and an angle
	NiMatrix3 getRotationAxisAngle(NiPoint3& axis, float theta)
	{
		NiMatrix3 result;
		// This math was found online http://www.euclideanspace.com/maths/geometry/rotations/conversions/angleToMatrix/
		double c = std::cosf(theta);
		double s = std::sinf(theta);
		double t = 1.0 - c;
		result.entry[0][0] = c + axis.x * axis.x * t;
		result.entry[1][1] = c + axis.y * axis.y * t;
		result.entry[2][2] = c + axis.z * axis.z * t;
		double tmp1 = axis.x * axis.y * t;
		double tmp2 = axis.z * s;
		result.entry[1][0] = tmp1 + tmp2;
		result.entry[0][1] = tmp1 - tmp2;
		tmp1 = axis.x * axis.z * t;
		tmp2 = axis.y * s;
		result.entry[2][0] = tmp1 - tmp2;
		result.entry[0][2] = tmp1 + tmp2;
		tmp1 = axis.y * axis.z * t;
		tmp2 = axis.x * s;
		result.entry[2][1] = tmp1 + tmp2;
		result.entry[1][2] = tmp1 - tmp2;
		return result;
	}

	NiMatrix3 RotateBetweenVectors(const NiPoint3& src, const NiPoint3& dest)
	{
		RE::NiPoint3 axis = VectorNormalized(src.Cross(VectorNormalized(dest)));

		auto angle =
			std::acos(helper::DotProductSafe(VectorNormalized(src), VectorNormalized(dest)));

		if (axis.Length() < 0.0005f)
		{  // Handle the case where the vectors are colinear
			axis = src.UnitCross(RE::NiPoint3(std::rand() / RAND_MAX, std::rand() / RAND_MAX, 0));
		}  // also need to handle case where the angle is extremely small?
		//if (angle < 0.01 || angle > 3.13) angle = 0.f;

		return helper::getRotationAxisAngle(axis, angle);
	}

	void FaceCamera(
		RE::NiAVObject* a_target, bool a_x, bool a_y, bool a_z, RE::NiPoint3 a_target_normal)
	{
		if (a_target)
		{ /*
			auto camera = RE::PlayerCharacter::GetSingleton()
							  ->Get3D(true)
							  //->GetObjectByName("NPC R Hand [RHnd]")
							  ->GetObjectByName("NPC Head [Head]")
							  ->world;*/

			RE::PlayerCamera* camobj = RE::PlayerCamera::GetSingleton();

			if (auto camera = camobj->cameraRoot.get())
			{
				constexpr float tolerance = 25.f * 25.f;

				auto vector_to_camera = camera->world.translate - a_target->world.translate;

				if ((vector_to_camera.x * vector_to_camera.x +
						vector_to_camera.y * vector_to_camera.y) > tolerance)
				{
					auto norm = VectorNormalized(vector_to_camera);

					auto          zrot = atan2(norm.y, norm.x);
					auto          cosz = std::cosf(zrot);
					auto          sinz = std::sinf(zrot);
					RE::NiMatrix3 rotz = { { cosz, -1 * sinz, 0 }, { sinz, cosz, 0 }, { 0, 0, 1 } };

					a_target->local.rotate = a_target->parent->world.rotate.Transpose() * rotz;
					//}
				}
			}
		}
	}

	// Interpolate between two rotation matrices using quaternion math (Prog's code)
	NiMatrix3 slerpMatrixAdaptive(NiMatrix3 mat1, NiMatrix3 mat2)
	{
		// Convert mat1 to a quaternion
		float q1w =
			sqrt(std::max(0.0f, 1 + mat1.entry[0][0] + mat1.entry[1][1] + mat1.entry[2][2])) / 2;
		float q1x =
			sqrt(std::max(0.0f, 1 + mat1.entry[0][0] - mat1.entry[1][1] - mat1.entry[2][2])) / 2;
		float q1y =
			sqrt(std::max(0.0f, 1 - mat1.entry[0][0] + mat1.entry[1][1] - mat1.entry[2][2])) / 2;
		float q1z =
			sqrt(std::max(0.0f, 1 - mat1.entry[0][0] - mat1.entry[1][1] + mat1.entry[2][2])) / 2;
		q1x = _copysign(q1x, mat1.entry[2][1] - mat1.entry[1][2]);
		q1y = _copysign(q1y, mat1.entry[0][2] - mat1.entry[2][0]);
		q1z = _copysign(q1z, mat1.entry[1][0] - mat1.entry[0][1]);

		// Convert mat2 to a quaternion
		float q2w =
			sqrt(std::max(0.0f, 1 + mat2.entry[0][0] + mat2.entry[1][1] + mat2.entry[2][2])) / 2;
		float q2x =
			sqrt(std::max(0.0f, 1 + mat2.entry[0][0] - mat2.entry[1][1] - mat2.entry[2][2])) / 2;
		float q2y =
			sqrt(std::max(0.0f, 1 - mat2.entry[0][0] + mat2.entry[1][1] - mat2.entry[2][2])) / 2;
		float q2z =
			sqrt(std::max(0.0f, 1 - mat2.entry[0][0] - mat2.entry[1][1] + mat2.entry[2][2])) / 2;
		q2x = _copysign(q2x, mat2.entry[2][1] - mat2.entry[1][2]);
		q2y = _copysign(q2y, mat2.entry[0][2] - mat2.entry[2][0]);
		q2z = _copysign(q2z, mat2.entry[1][0] - mat2.entry[0][1]);

		// Take the dot product, inverting q2 if it is negative
		double dot = q1w * q2w + q1x * q2x + q1y * q2y + q1z * q2z;
		if (dot < 0.0f)
		{
			q2w = -q2w;
			q2x = -q2x;
			q2y = -q2y;
			q2z = -q2z;
			dot = -dot;
		}

		float interp = 0.2f;

		float q3w, q3x, q3y, q3z;
		if (dot > 0.9996f) { return mat1; }
		else if (dot > 0.999f) { interp = 0.02f; }
		else if (dot > 0.99f) { interp = 0.05f; }

		float interp_adjusted = 1 - dot * dot + interp;
		interp_adjusted = std::clamp<float>(interp_adjusted, interp, 1.0f);

		float theta_0 = acosf(dot);         // theta_0 = angle between input vectors
		float theta = theta_0 * interp;     // theta = angle between q1 and result
		float sin_theta = sinf(theta);      // compute this value only once
		float sin_theta_0 = sinf(theta_0);  // compute this value only once
		float s0 =
			cosf(theta) - dot * sin_theta / sin_theta_0;  // == sin(theta_0 - theta) / sin(theta_0)
		float s1 = sin_theta / sin_theta_0;
		q3w = (s0 * q1w) + (s1 * q2w);
		q3x = (s0 * q1x) + (s1 * q2x);
		q3y = (s0 * q1y) + (s1 * q2y);
		q3z = (s0 * q1z) + (s1 * q2z);

		// Convert the new quaternion back to a matrix
		NiMatrix3 result;
		result.entry[0][0] = 1 - (2 * q3y * q3y) - (2 * q3z * q3z);
		result.entry[0][1] = (2 * q3x * q3y) - (2 * q3z * q3w);
		result.entry[0][2] = (2 * q3x * q3z) + (2 * q3y * q3w);
		result.entry[1][0] = (2 * q3x * q3y) + (2 * q3z * q3w);
		result.entry[1][1] = 1 - (2 * q3x * q3x) - (2 * q3z * q3z);
		result.entry[1][2] = (2 * q3y * q3z) - (2 * q3x * q3w);
		result.entry[2][0] = (2 * q3x * q3z) - (2 * q3y * q3w);
		result.entry[2][1] = (2 * q3y * q3z) + (2 * q3x * q3w);
		result.entry[2][2] = 1 - (2 * q3x * q3x) - (2 * q3y * q3y);
		return result;
	}

	void Quat2Mat(NiMatrix3& matrix, NiQuaternion& quaternion)
	{
		float xx = quaternion.x * quaternion.x;
		float xy = quaternion.x * quaternion.y;
		float xz = quaternion.x * quaternion.z;
		float xw = quaternion.x * quaternion.w;

		float yy = quaternion.y * quaternion.y;
		float yz = quaternion.y * quaternion.z;
		float yw = quaternion.y * quaternion.w;

		float zz = quaternion.z * quaternion.z;
		float zw = quaternion.z * quaternion.w;

		matrix.entry[0][0] = 1 - 2 * (yy + zz);
		matrix.entry[0][1] = 2 * (xy - zw);
		matrix.entry[0][2] = 2 * (xz + yw);

		matrix.entry[1][0] = 2 * (xy + zw);
		matrix.entry[1][1] = 1 - 2 * (xx + zz);
		matrix.entry[1][2] = 2 * (yz - xw);

		matrix.entry[2][0] = 2 * (xz - yw);
		matrix.entry[2][1] = 2 * (yz + xw);
		matrix.entry[2][2] = 1 - 2 * (xx + yy);
	}

	void slerpQuat(float interp, NiQuaternion& q1, NiQuaternion& q2, NiMatrix3& out)
	{
		float q1w = q1.w;
		float q1x = q1.x;
		float q1y = q1.y;
		float q1z = q1.z;

		float q2w = q2.w;
		float q2x = q2.x;
		float q2y = q2.y;
		float q2z = q2.z;

		// Take the dot product, inverting q2 if it is negative
		double dot = q1w * q2w + q1x * q2x + q1y * q2y + q1z * q2z;
		if (dot < 0.0f)
		{
			q2w = -q2w;
			q2x = -q2x;
			q2y = -q2y;
			q2z = -q2z;
			dot = -dot;
		}

		// Linearly interpolate and normalize if the dot product is too close to 1
		float q3w, q3x, q3y, q3z;
		if (dot > 0.9995)
		{
			q3w = q1w + interp * (q2w - q1w);
			q3x = q1x + interp * (q2x - q1x);
			q3y = q1y + interp * (q2y - q1y);
			q3z = q1z + interp * (q2z - q1z);
			float length = sqrtf(q3w * q3w + q3x + q3x + q3y * q3y + q3z * q3z);
			q3w /= length;
			q3x /= length;
			q3y /= length;
			q3z /= length;

			// Otherwise do a spherical linear interpolation normally
		}
		else
		{
			float theta_0 = acosf(dot);         // theta_0 = angle between input vectors
			float theta = theta_0 * interp;     // theta = angle between q1 and result
			float sin_theta = sinf(theta);      // compute this value only once
			float sin_theta_0 = sinf(theta_0);  // compute this value only once
			float s0 = cosf(theta) -
				dot * sin_theta / sin_theta_0;  // == sin(theta_0 - theta) / sin(theta_0)
			float s1 = sin_theta / sin_theta_0;
			q3w = (s0 * q1w) + (s1 * q2w);
			q3x = (s0 * q1x) + (s1 * q2x);
			q3y = (s0 * q1y) + (s1 * q2y);
			q3z = (s0 * q1z) + (s1 * q2z);
		}

		// Convert the new quaternion back to a matrix
		out.entry[0][0] = 1 - (2 * q3y * q3y) - (2 * q3z * q3z);
		out.entry[0][1] = (2 * q3x * q3y) - (2 * q3z * q3w);
		out.entry[0][2] = (2 * q3x * q3z) + (2 * q3y * q3w);
		out.entry[1][0] = (2 * q3x * q3y) + (2 * q3z * q3w);
		out.entry[1][1] = 1 - (2 * q3x * q3x) - (2 * q3z * q3z);
		out.entry[1][2] = (2 * q3y * q3z) - (2 * q3x * q3w);
		out.entry[2][0] = (2 * q3x * q3z) - (2 * q3y * q3w);
		out.entry[2][1] = (2 * q3y * q3z) + (2 * q3x * q3w);
		out.entry[2][2] = 1 - (2 * q3x * q3x) - (2 * q3y * q3y);
	}
}
