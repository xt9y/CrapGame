#include "Renderer/Material/Material.hpp"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace Renderer
{
namespace Material
{
namespace
{

std::vector<Resource>& resources()
{
    static std::vector<Resource> values;
    return values;
}

} // namespace

float nsToRoughness(float shininess)
{
    const float ns = std::max(0.0f, shininess);
    return std::max(0.04f, std::min(1.0f, std::sqrt(2.0f / (ns + 2.0f))));
}

float iorToF0(float ior)
{
    const float n = std::max(1.0001f, ior);
    const float ratio = (n - 1.0f) / (n + 1.0f);
    return ratio * ratio;
}

MaterialHandle registerMaterial(Resource resource)
{
    const MaterialHandle handle = static_cast<MaterialHandle>(resources().size());
    resources().push_back(std::move(resource));
    return handle;
}

const Resource *get(MaterialHandle handle)
{
    return handle < resources().size() ? &resources()[handle] : nullptr;
}

std::size_t count()
{
    return resources().size();
}

void clear()
{
    resources().clear();
}

} // namespace Material
} // namespace Renderer
