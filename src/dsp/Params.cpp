#include "dsp/Params.h"

namespace k303 {

std::optional<ParamId> paramFromKey(std::string_view key)
{
    for (const auto &info : kParams) {
        if (info.key == key)
            return info.id;
    }
    return std::nullopt;
}

} // namespace k303
