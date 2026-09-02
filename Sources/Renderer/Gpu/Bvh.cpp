#include "Bvh.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <utility>

namespace Renderer
{
namespace Gpu
{
namespace
{

struct Vec3f
{
    float x,
          y,
          z;
};

Vec3f rotateX (Vec3f value, float angle)
{
    const float sine = std::sin(angle),
                cosine = std::cos(angle);

    return {
        value.x,
        cosine * value.y - sine * value.z,
        sine * value.y + cosine * value.z,
    };
}

Vec3f rotateY (Vec3f value, float angle)
{
    const float sine = std::sin(angle),
                cosine = std::cos(angle);

    return {
        cosine * value.x + sine * value.z,
        value.y,
        -sine * value.x + cosine * value.z,
    };
}

Vec3f rotateZ (Vec3f value, float angle)
{
    const float sine = std::sin(angle),
                cosine = std::cos(angle);

    return {
        cosine * value.x - sine * value.y,
        sine * value.x + cosine * value.y,
        value.z,
    };
}

Vec3f forwardRotate (Vec3f value, const Ecs::Vec3& degrees)
{
    constexpr float PI = 3.14159265358979323846f;
    const float x = degrees.x * PI / 180.0f,
                y = degrees.y * PI / 180.0f,
                z = degrees.z * PI / 180.0f;

    value = rotateZ(value, z);
    value = rotateX(value, x);
    value = rotateY(value, y);
    return value;
}

float component (const float value[3], int axis)
{
    return value[axis];
}

float centroid (const BvhBoundsInput& bounds, int axis)
{
    return (component(bounds.minimum, axis) + component(bounds.maximum, axis)) * 0.5f;
}

struct Builder
{
    const std::vector<BvhBoundsInput>& bounds;
    std::vector<std::uint32_t> order;
    std::vector<BvhNodeGpu> nodes;
    std::size_t leaf_size;

    std::uint32_t buildNode (std::size_t first, std::size_t count)
    {
        const std::uint32_t node_index = static_cast<std::uint32_t>(nodes.size());
        nodes.push_back({});

        float minimum[3] = {
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
        };
        float maximum[3] = {
            -std::numeric_limits<float>::max(),
            -std::numeric_limits<float>::max(),
            -std::numeric_limits<float>::max(),
        };
        float centroid_minimum[3] = {
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
        };
        float centroid_maximum[3] = {
            -std::numeric_limits<float>::max(),
            -std::numeric_limits<float>::max(),
            -std::numeric_limits<float>::max(),
        };

        for (std::size_t offset = 0; offset < count; ++offset)
        {
            const BvhBoundsInput& item = bounds[order[first + offset]];

            for (int axis = 0; axis < 3; ++axis)
            {
                minimum[axis] = std::min(minimum[axis], item.minimum[axis]);
                maximum[axis] = std::max(maximum[axis], item.maximum[axis]);

                const float center = centroid(item, axis);
                centroid_minimum[axis] = std::min(centroid_minimum[axis], center);
                centroid_maximum[axis] = std::max(centroid_maximum[axis], center);
            }
        }

        BvhNodeGpu& node = nodes[node_index];
        for (int axis = 0; axis < 3; ++axis)
        {
            node.bounds_minimum[axis] = minimum[axis];
            node.bounds_maximum[axis] = maximum[axis];
        }

        if (count <= leaf_size)
        {
            node.meta[0] = -1;
            node.meta[1] = -1;
            node.meta[2] = -1;
            node.meta[3] = static_cast<std::int32_t>(count);

            for (std::size_t offset = 0; offset < count; ++offset)
            {
                node.meta[offset] = static_cast<std::int32_t>(
                        bounds[order[first + offset]].primitive_index
                    );
            }
            return node_index;
        }

        int split_axis = 0;
        float split_extent = centroid_maximum[0] - centroid_minimum[0];

        for (int axis = 1; axis < 3; ++axis)
        {
            const float extent = centroid_maximum[axis] - centroid_minimum[axis];
            if (extent > split_extent)
            {
                split_axis = axis;
                split_extent = extent;
            }
        }

        const std::size_t left_count = count / 2u;
        const std::size_t middle = first + left_count;
        const std::size_t end = first + count;

        std::nth_element(
                order.begin() + static_cast<std::ptrdiff_t>(first),
                order.begin() + static_cast<std::ptrdiff_t>(middle),
                order.begin() + static_cast<std::ptrdiff_t>(end),
                [&] (std::uint32_t left, std::uint32_t right)
                {
                    return centroid(bounds[left], split_axis)
                        < centroid(bounds[right], split_axis);
                }
            );

        const std::uint32_t left = buildNode(first, left_count);
        const std::uint32_t right = buildNode(middle, count - left_count);

        BvhNodeGpu& completed = nodes[node_index];
        completed.meta[0] = static_cast<std::int32_t>(left);
        completed.meta[1] = static_cast<std::int32_t>(right);
        completed.meta[2] = -1;
        completed.meta[3] = 0;
        return node_index;
    }
};

} // namespace

BvhBoundsInput primitiveBounds (
            const Ecs::TransformComponent& transform,
            Ecs::MeshType mesh,
            std::uint32_t primitive_index
    )
{
    const float local_x = mesh == Ecs::MeshType::Cube
        ? 0.75f * std::fabs(transform.scale.x)
        : 0.50f * std::fabs(transform.scale.x);
    const float local_y = mesh == Ecs::MeshType::Cube
        ? 0.75f * std::fabs(transform.scale.y)
        : std::max(0.005f, 0.005f * std::fabs(transform.scale.y));
    const float local_z = mesh == Ecs::MeshType::Cube
        ? 0.75f * std::fabs(transform.scale.z)
        : 0.50f * std::fabs(transform.scale.z);

    const Vec3f basis_x = forwardRotate({1.0f, 0.0f, 0.0f}, transform.rotation);
    const Vec3f basis_y = forwardRotate({0.0f, 1.0f, 0.0f}, transform.rotation);
    const Vec3f basis_z = forwardRotate({0.0f, 0.0f, 1.0f}, transform.rotation);

    const Vec3f extent = {
        std::fabs(basis_x.x) * local_x
            + std::fabs(basis_y.x) * local_y
            + std::fabs(basis_z.x) * local_z,
        std::fabs(basis_x.y) * local_x
            + std::fabs(basis_y.y) * local_y
            + std::fabs(basis_z.y) * local_z,
        std::fabs(basis_x.z) * local_x
            + std::fabs(basis_y.z) * local_y
            + std::fabs(basis_z.z) * local_z,
    };

    BvhBoundsInput result;
    result.minimum[0] = transform.position.x - extent.x;
    result.minimum[1] = transform.position.y - extent.y;
    result.minimum[2] = transform.position.z - extent.z;
    result.maximum[0] = transform.position.x + extent.x;
    result.maximum[1] = transform.position.y + extent.y;
    result.maximum[2] = transform.position.z + extent.z;
    result.primitive_index = primitive_index;
    return result;
}

BvhBuild buildBvh (
            const std::vector<BvhBoundsInput>& bounds,
            std::size_t leaf_size
    )
{
    BvhBuild result;

    if (bounds.empty())
    {
        return result;
    }

    Builder builder {
        bounds,
        std::vector<std::uint32_t>(bounds.size()),
        {},
        std::min<std::size_t>(3u, std::max<std::size_t>(1u, leaf_size)),
    };

    std::iota(builder.order.begin(), builder.order.end(), 0u);
    builder.nodes.reserve(bounds.size() * 2u - 1u);
    builder.buildNode(0u, bounds.size());

    result.nodes = std::move(builder.nodes);
    return result;
}

bool refitBvh (
            std::vector<BvhNodeGpu> *nodes,
            const std::vector<BvhBoundsInput>& bounds
    )
{
    if (!nodes || nodes->empty())
    {
        return bounds.empty();
    }

    for (std::size_t reverse = nodes->size(); reverse-- > 0u;)
    {
        BvhNodeGpu& node = (*nodes)[reverse];
        const std::int32_t leaf_count = node.meta[3];

        if (leaf_count > 0)
        {
            if (leaf_count > 3)
            {
                return false;
            }

            float minimum[3] = {
                std::numeric_limits<float>::max(),
                std::numeric_limits<float>::max(),
                std::numeric_limits<float>::max(),
            };
            float maximum[3] = {
                -std::numeric_limits<float>::max(),
                -std::numeric_limits<float>::max(),
                -std::numeric_limits<float>::max(),
            };

            for (std::int32_t slot = 0; slot < leaf_count; ++slot)
            {
                const std::int32_t primitive = node.meta[slot];

                if (primitive < 0
                        || static_cast<std::size_t>(primitive) >= bounds.size())
                {
                    return false;
                }

                const BvhBoundsInput& item = bounds[static_cast<std::size_t>(primitive)];

                for (int axis = 0; axis < 3; ++axis)
                {
                    minimum[axis] = std::min(minimum[axis], item.minimum[axis]);
                    maximum[axis] = std::max(maximum[axis], item.maximum[axis]);
                }
            }

            for (int axis = 0; axis < 3; ++axis)
            {
                node.bounds_minimum[axis] = minimum[axis];
                node.bounds_maximum[axis] = maximum[axis];
            }

            continue;
        }

        const std::int32_t left = node.meta[0],
                           right = node.meta[1];

        if (left < 0 || right < 0
                || static_cast<std::size_t>(left) >= nodes->size()
                || static_cast<std::size_t>(right) >= nodes->size())
        {
            return false;
        }

        const BvhNodeGpu& left_node = (*nodes)[static_cast<std::size_t>(left)];
        const BvhNodeGpu& right_node = (*nodes)[static_cast<std::size_t>(right)];

        for (int axis = 0; axis < 3; ++axis)
        {
            node.bounds_minimum[axis] = std::min(
                    left_node.bounds_minimum[axis],
                    right_node.bounds_minimum[axis]
                );
            node.bounds_maximum[axis] = std::max(
                    left_node.bounds_maximum[axis],
                    right_node.bounds_maximum[axis]
                );
        }
    }

    return true;
}

} // namespace Gpu
} // namespace Renderer
