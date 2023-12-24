#include "helper_math.h"

namespace helper
{
    void RotateZ(RE::NiPoint3& target, RE::NiMatrix3& rotator)
    {
        float zangle = helper::GetAzimuth(rotator);
        float cosz = cos(zangle);
        float sinz = sin(zangle);
        target = { cosz * target[0] + sinz * target[1],
                  cosz * target[1] - sinz * target[0],
                  target[2] };
    }
}
