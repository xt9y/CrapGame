#ifndef CRAPGAME_RENDERER_GPU_GBUFFERGPU_HPP
#define CRAPGAME_RENDERER_GPU_GBUFFERGPU_HPP

#include "Ecs/Ecs.hpp"
#include "Renderer/Math/Math.hpp"
#include "Renderer/Mesh/Mesh.hpp"

#include <lwcgl/lwcgl.h>
#include <lwcgl/glmodern.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Renderer { namespace Gpu {

class GBufferGpu
{
public:
    bool init(std::string* error=nullptr);
    bool resize(int width,int height,std::string* error=nullptr);
    bool updateScene(const Ecs::World& world,std::string* error=nullptr);
    bool draw(const Math::Mat4& view,const Math::Mat4& projection,std::string* error=nullptr);
    bool render(const Ecs::World& world,const Math::Mat4& view,const Math::Mat4& projection,std::string* error=nullptr);
    void shutdown();

    bool ready() const { return program_!=0 && framebuffer_!=0; }
    int width() const { return width_; }
    int height() const { return height_; }
    GLuint positionDepthTexture() const { return position_depth_; }
    GLuint normalRoughnessTexture() const { return normal_roughness_; }
    GLuint albedoMetallicTexture() const { return albedo_metallic_; }
    GLuint emissiveTexture() const { return emissive_; }
    GLuint depthTexture() const { return depth_; }

private:
    struct MeshGpu { GLuint vao=0, vertex_buffer=0, index_buffer=0; GLsizei index_count=0; };
    struct InstanceGpu { float model[16]; float normal_matrix[16]; float albedo_metallic[4]; float emissive_roughness[4]; };
    struct Batch
    {
        MeshGpu mesh;
        GLuint instance_buffer=0;
        std::size_t instance_capacity=0;
        std::vector<InstanceGpu> instances;
        std::vector<InstanceGpu> uploaded_instances;
    };

    bool createMesh(Ecs::MeshType type,MeshGpu* mesh,std::string* error);
    bool createMesh(const Mesh::MeshData& source,MeshGpu* mesh,std::string* error);
    Batch* loadedBatch(std::uint32_t handle,std::string* error);
    bool uploadBatch(Batch* batch,std::string* error);
    bool createAttachments(std::string* error);
    void destroyAttachments();
    void destroyMesh(MeshGpu* mesh);
    void destroyBatch(Batch* batch);

    GLuint program_=0, framebuffer_=0;
    GLuint position_depth_=0, normal_roughness_=0, albedo_metallic_=0, emissive_=0, depth_=0;
    GLint view_location_=-1, projection_location_=-1;
    Batch cubes_, planes_;
    std::vector<Batch> loaded_batches_;
    int width_=0, height_=0;
};

} }
#endif
