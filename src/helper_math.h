#pragma once
#include <algorithm>

namespace helper
{
    using namespace RE;

    constexpr inline float deg2rad(const float d)
    {
        return d * 0.01745329f;
    }

    // get rotation about Z axis while ignoring other rotations
    inline float GetAzimuth(NiMatrix3& rot)
    {
        if (std::abs(rot.entry[2][1]) < 0.9995f)
        {
            return std::atan2(rot.entry[0][1], rot.entry[1][1]);
        }
        else
        {
            return -1.0f * std::atan2(rot.entry[1][0], rot.entry[0][0]);
        }
    }
    // same but X axis
    inline float GetElevation(NiMatrix3& rot)
    {
        return -1.0f * std::asin(std::clamp(rot.entry[2][1], -1.0f, 1.0f));
    }

    void RotateZ(NiPoint3& target, NiMatrix3& rotator);

    // vector stuff from higgs
    inline float VectorLengthSquared(const NiPoint3& vec) { return vec.x * vec.x + vec.y * vec.y + vec.z * vec.z; }
    inline float VectorLength(const NiPoint3& vec) { return sqrtf(VectorLengthSquared(vec)); }
    inline float DotProductSafe(const NiPoint3& vec1, const NiPoint3& vec2) { return std::clamp(vec1.Dot(vec2), -1.f, 1.f); }
    inline NiPoint3 VectorNormalized(const NiPoint3& vec)
    {
        float length = VectorLength(vec);
        return length > 0.0f ? vec / length : NiPoint3();
    }

    NiPoint3 GetPalmVectorWS(NiMatrix3& handRotation, bool isLeft);
    NiPoint3 GetThumbVector(NiMatrix3& handRotation);

    // matrix stuff

    // Gets a rotation matrix from an axis and an angle
    inline NiMatrix3 getRotationAxisAngle(NiPoint3 axis, float theta)
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

    inline void Quat2Mat(NiMatrix3& matrix, NiQuaternion& quaternion)
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

    inline void slerpQuat(float interp, NiQuaternion& q1, NiQuaternion& q2, NiMatrix3& out)
    {
        // Convert mat1 to a quaternion
        float q1w = q1.w;
        float q1x = q1.x;
        float q1y = q1.y;
        float q1z = q1.z;

        // Convert mat2 to a quaternion
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
            float theta_0 = acosf(dot);                             // theta_0 = angle between input vectors
            float theta = theta_0 * interp;                         // theta = angle between q1 and result
            float sin_theta = sinf(theta);                          // compute this value only once
            float sin_theta_0 = sinf(theta_0);                      // compute this value only once
            float s0 = cosf(theta) - dot * sin_theta / sin_theta_0; // == sin(theta_0 - theta) / sin(theta_0)
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
