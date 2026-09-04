#include "Renderer/Gpu/Bvh.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <set>
#include <vector>

using namespace Renderer::Gpu;

namespace
{
BvhBoundsInput box(float x0,float y0,float z0,float x1,float y1,float z1,std::uint32_t id)
{
    BvhBoundsInput b={};
    b.minimum[0]=x0;b.minimum[1]=y0;b.minimum[2]=z0;
    b.maximum[0]=x1;b.maximum[1]=y1;b.maximum[2]=z1;
    b.primitive_index=id;
    return b;
}

bool contains(const BvhNodeGpu& node,const BvhBoundsInput& bounds)
{
    for(int axis=0;axis<3;++axis)
    {
        if(node.bounds_minimum[axis]>bounds.minimum[axis]+1.0e-6f
                ||node.bounds_maximum[axis]<bounds.maximum[axis]-1.0e-6f)
            return false;
    }
    return true;
}
} // namespace

int main()
{
    std::vector<BvhBoundsInput> bounds;
    for(std::uint32_t i=0;i<24u;++i)
    {
        const float cluster=i<12u?0.0f:40.0f;
        const float x=cluster+static_cast<float>(i%6u)*0.7f;
        const float y=static_cast<float>((i/6u)%2u)*0.4f;
        const float z=static_cast<float>(i%3u)*0.5f;
        bounds.push_back(box(x,y,z,x+0.3f,y+0.3f,z+0.3f,i));
    }

    const BvhBuild first=buildBvhSah(bounds,3u,16u);
    const BvhBuild second=buildBvhSah(bounds,3u,16u);
    assert(!first.nodes.empty());
    assert(first.nodes.size()==second.nodes.size());
    assert(std::equal(first.nodes.begin(),first.nodes.end(),second.nodes.begin(),
        [](const BvhNodeGpu& left,const BvhNodeGpu& right)
        {
            for(int i=0;i<4;++i)if(left.meta[i]!=right.meta[i])return false;
            for(int i=0;i<3;++i)
                if(left.bounds_minimum[i]!=right.bounds_minimum[i]
                        ||left.bounds_maximum[i]!=right.bounds_maximum[i])return false;
            return true;
        }));

    std::set<std::uint32_t> seen;
    for(const BvhNodeGpu& node:first.nodes)
    {
        if(node.meta[3]<=0)continue;
        assert(node.meta[3]<=3);
        for(int slot=0;slot<node.meta[3];++slot)
        {
            assert(node.meta[slot]>=0);
            const std::uint32_t id=static_cast<std::uint32_t>(node.meta[slot]);
            assert(id<bounds.size());
            assert(contains(node,bounds[id]));
            assert(seen.insert(id).second);
        }
    }
    assert(seen.size()==bounds.size());
    for(const BvhBoundsInput& item:bounds)assert(contains(first.nodes[0],item));

    assert(first.nodes[0].meta[3]==0);
    const BvhNodeGpu& left=first.nodes[static_cast<std::size_t>(first.nodes[0].meta[0])];
    const BvhNodeGpu& right=first.nodes[static_cast<std::size_t>(first.nodes[0].meta[1])];
    assert(left.bounds_maximum[0]<right.bounds_minimum[0]
            ||right.bounds_maximum[0]<left.bounds_minimum[0]);
    return 0;
}
