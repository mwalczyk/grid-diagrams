#pragma once
#pragma once

#include <iostream>
#include <vector>

#include "glad/glad.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include "glm.hpp"
#include "gtx/hash.hpp"

namespace graphics
{

    enum class Attribute
    {
        POSITION,
        NORMAL,
        COLOR,
        TEXTURE
        // CUSTOM_0,
        // CUSTOM_1,
        // ...
    };

    struct AttribInfo
    {
        int8_t get_byte_size() const
        {
            if (data_type == GL_DOUBLE)
            {
                return dimensions * 8;
            }
            else
            {
                // Float
                return dimensions * 4;
            }
        }

        Attribute attrib;
        uint8_t dimensions;
        size_t stride;
        size_t offset;
        uint32_t instance_divisor;
        uint32_t data_type = GL_FLOAT;
    };

    class Vbo
    {

    public:

        Vbo(size_t allocation_size, const void* data = nullptr, GLenum usage = GL_DYNAMIC_STORAGE_BIT) :
            size{ allocation_size },
            usage{ usage }
        {
            std::cout << "Creating buffer object..." << std::endl;
            glCreateBuffers(1, &vbo);
            glNamedBufferStorage(vbo, size, nullptr, usage);
        }

        ~Vbo()
        {
            glDeleteBuffers(1, &vbo);
        }

        void check_size(size_t total_data_bytes)
        {
            if (size < total_data_bytes)
            {
                std::cout << "Reallocating buffer object - old size: " << size << ", new size: " << total_data_bytes << std::endl;
                size = total_data_bytes;
                glCreateBuffers(1, &vbo);
                glNamedBufferStorage(vbo, size, nullptr, usage);
            }
        }

    private:

        uint32_t vbo;
        size_t size;
        GLenum usage;

    };

    class Layout
    {

    public:
        
        Layout() :
            usage{ GL_DYNAMIC_STORAGE_BIT }
        {}

        Layout& interleave(bool interleave = true)
        {
            interleave = interleave;
            return *this;
        }

        Layout& usage(GLenum usage)
        {
            usage = usage;
            return *this;
        }

        Layout& attrib(Attribute attrib, uint8_t dimensions)
        {
            if (dimensions <= 0)
            {
                throw std::runtime_error("Dimensions must be greater than 0");
            }

            // Don't allow duplicate attributes
            for (auto it = attrib_infos.begin(); it != attrib_infos.end(); )
            {
                if (it->attrib == attrib)
                {
                    it = attrib_infos.erase(it);
                }
                else
                {
                    ++it;
                }
            }
            attrib_infos.push_back({ attrib, dimensions, 0, 0, 0 });
            return *this;
        }

        Layout& attrib(const AttribInfo& attrib_info)
        {
            if (attrib_info.dimensions <= 0)
            {
                throw std::runtime_error("Dimensions must be greater than 0");
            }

            // Don't allow duplicate attributes
            for (auto it = attrib_infos.begin(); it != attrib_infos.end(); )
            {
                if (it->attrib == attrib_info.attrib)
                {
                    it = attrib_infos.erase(it);
                }
                else
                {
                    ++it;
                }
            }
            attrib_infos.push_back(attrib_info);
            return *this;
        }

        void clear_attribs()
        {
            attrib_infos.clear();
        }

        void allocate_for(size_t number_of_vertices, Vbo* vbo) const
        {
            auto attrib_infos_copy = attrib_infos;

            // Setup offsets and strides based on interleaved or planar
            size_t total_data_bytes;

            if (interleave) 
            {
                // Calculate the stride between each vertex, which is simply the sum of the sizes of each (interleaved) attrib
                size_t total_stride = 0;
                for (const auto& attrib : attrib_infos_copy)
                {
                    total_stride += attrib.get_byte_size();
                }

                size_t current_offset = 0;
                for (auto& attrib : attrib_infos_copy)
                {
                    attrib.offset = current_offset;
                    attrib.stride = total_stride;
                    current_offset += attrib.get_byte_size();
                }

                total_data_bytes = current_offset * number_of_vertices;
            }
            else 
            { 
                // Non-interleaved data
                size_t current_offset = 0;
                for (auto& attrib : attrib_infos_copy)
                {
                    attrib.offset = current_offset;
                    attrib.stride = attrib.get_byte_size();
                    current_offset += attrib.get_byte_size() * number_of_vertices;
                }

                total_data_bytes = current_offset;
            }

            // Return the resulting buffer layout as some form of `attrib_infos_copy`
            // ...

            // Allocate the buffer
            if (vbo != nullptr)
            {
                vbo->check_size(total_data_bytes);
            }
            else
            {
                *vbo = Vbo(total_data_bytes, nullptr, usage);
            }

        }
        
    private:

        bool interleave;
        GLenum usage;
        std::vector<AttribInfo> attrib_infos;

        friend class Mesh;

    };







    class MeshData {};

    class Mesh
    {
    public:

        Mesh(const MeshData& data, std::vector<Layout>& layouts, const Vbo& ibo) :
            ibo{ ibo }
        {
            // If layouts is empty, pull attributes available from source geometry
            if (layouts.empty())
            {
                throw std::runtime_error("Layouts cannot be empty (for now)");
            }

            // Figure out which attributes are actually going to be used
            std::vector<Attribute> requested_attribs;
            for (const auto& layout : layouts)
            {
                for (const auto& attrib_info : layout.attrib_infos)
                {
                    if (attrib_info.attrib == Attribute::POSITION)
                    {
                        std::cout << "Requested POSITION attribute\n";
                    }
                    else if (attrib_info.attrib == Attribute::COLOR)
                    {
                        std::cout << "Requested COLOR attribute\n";
                    }
                    else if (attrib_info.attrib == Attribute::NORMAL)
                    {
                        std::cout << "Requested NORMAL attribute\n";
                    }
                    else if (attrib_info.attrib == Attribute::TEXTURE)
                    {
                        std::cout << "Requested TEXTURE attribute\n";
                    }
                    requested_attribs.push_back(attrib_info.attrib);
                }
            }

            // Load the geometry data into VBOs
            // ...
            // Get the number of vertices
            // Get the primitive type (TRIANLGES, LINES, etc.)
            // Save these as member vars
            // ...
            size_t number_of_vertices = 10;


            for (const auto& layout : layouts)
            {
                Vbo* vbo = nullptr;

                layout.allocate_for(number_of_vertices, vbo);

                //layouts_with_buffers.push_back({ layout, nullptr });
            }

            // Setup the VAO
            // ...
        }

    private:

        std::vector<std::pair<Layout, Vbo>> layouts_with_buffers;
        Vbo ibo;
       
    };











    struct DrawCommand
    {
        uint32_t mode;              // Probably GL_TRIANGLES
        uint32_t count;             // Number of elements to be rendered
        uint32_t type;              // Probably GL_UNSIGNED_BYTE
        uint32_t indices;           // A pointer to the location where the indices are stored
        uint32_t base_vertex;       // A constant that should be added to each element of `indices` when choosing elements from the enabled vertex arrays
        uint32_t base_instance;     // The base instance for use in fetching instanced vertex attributes
    };



    //class Mesh
    //{

    //public:

    //    Mesh() = default;

    //    Mesh(const std::vector<glm::vec3>& vertices, 
    //         const std::vector<glm::vec3>& colors,
    //         const std::vector<glm::vec3>& normals,
    //         std::vector<uint32_t> indices) :

    //        vertices{ vertices },
    //        colors{ colors },
    //        normals{ normals },
    //        indices{ indices }
    //    {
    //        setup();
    //    }

    //    ~Mesh()
    //    {
    //        // RAII: clean-up OpenGL objects
    //        glDeleteVertexArrays(1, &vao);
    //        glDeleteBuffers(1, &vbo);
    //        glDeleteBuffers(1, &ibo);
    //    }

    //    Mesh& operator=(Mesh&& other) noexcept
    //    {
    //        // Grab the other mesh's OpenGL handles
    //        std::swap(vao, other.vao);
    //        std::swap(vbo, other.vbo);
    //        std::swap(ibo, other.ibo);

    //        vertices = std::move(other.vertices);
    //        indices = std::move(other.indices);

    //        return *this;
    //    }

    //    Mesh& operator=(const Mesh& other) = delete;

    //    void draw(uint32_t mode = GL_TRIANGLES) const
    //    {
    //        glBindVertexArray(vao);

    //        if (!indices.empty())
    //        {
    //            glDrawElements(mode, indices.size(), GL_UNSIGNED_INT, 0);
    //        }
    //        else
    //        {
    //            glDrawArrays(mode, 0, vertices.size());
    //        }

    //        glBindVertexArray(0);
    //    }

    //    void set_vertices(const std::vector<Vertex>& updated_vertices)
    //    {
    //        // Re-allocate the buffer if more space is needed: otherwise, we can simply copy in the new data because
    //        // we already have enough storage
    //        if (vertices.size() < updated_vertices.size())
    //        {
    //            // In DSA, if you need to re-allocate buffer memory, you basically have to reinitialize the 
    //            // entire buffer, per: https://www.reddit.com/r/opengl/comments/aifvjl/glnamedbufferstorage_vs_glbufferdata/
    //            vertices = updated_vertices;
    //            glDeleteVertexArrays(1, &vao);
    //            glDeleteBuffers(1, &vbo);
    //            glDeleteBuffers(1, &ibo);
    //            setup();
    //        }
    //        else
    //        {
    //            glNamedBufferSubData(vbo, 0, sizeof(Vertex) * updated_vertices.size(), updated_vertices.data());
    //            vertices = updated_vertices;
    //        }
    //    }

    //    void set_indices(const std::vector<uint32_t>& updated_indices)
    //    {
    //        if (indices.size() < updated_indices.size())
    //        {
    //            indices = updated_indices;
    //            glDeleteVertexArrays(1, &vao);
    //            glDeleteBuffers(1, &vbo);
    //            glDeleteBuffers(1, &ibo);
    //            setup();
    //        }
    //        else
    //        {
    //            glNamedBufferSubData(vbo, 0, sizeof(Vertex) * updated_indices.size(), updated_indices.data());
    //            indices = updated_indices;
    //        }
    //    }

    //    size_t get_vertex_count() const
    //    {
    //        return vertices.size();
    //    }

    //    size_t get_index_count() const
    //    {
    //        return indices.size();
    //    }

    //    const std::vector<glm::vec3>& get_vertices() const
    //    {
    //        return vertices;
    //    }

    //    const std::vector<uint32_t>& get_indices() const
    //    {
    //        return indices;
    //    }

    //private:

    //    uint32_t vao;
    //    uint32_t vbo;
    //    uint32_t ibo;

    //    // We shouldn't need to hold onto these CPU-side, but for convenience, we keep them here for now
    //    std::vector<glm::vec3> vertices;
    //    std::vector<glm::vec3> colors;
    //    std::vector<glm::vec3> normals;
    //    std::vector<uint32_t> indices;

    //    void setup()
    //    {
    //        // Load data into the vertex buffer
    //        if (vertices.empty())
    //        {
    //            // This should never happen
    //            vbo = 0;
    //        }
    //        else
    //        {
    //            glCreateBuffers(1, &vbo);
    //            glNamedBufferStorage(vbo, sizeof(Vertex) * vertices.size(), &vertices[0], GL_DYNAMIC_STORAGE_BIT);
    //        }

    //        // Load data into the index buffer
    //        if (indices.empty())
    //        {
    //            ibo = 0;
    //        }
    //        else
    //        {
    //            glCreateBuffers(1, &ibo);
    //            glNamedBufferStorage(ibo, sizeof(uint32_t) * indices.size(), &indices[0], GL_DYNAMIC_STORAGE_BIT);
    //        }

    //        // Set up the VAO and attributes
    //        glCreateVertexArrays(1, &vao);

    //        if (vbo)
    //        {
    //            // All vertex attributes will be sourced from a single buffer
    //            glVertexArrayVertexBuffer(vao, 0, vbo, 0, sizeof(Vertex));

    //            glEnableVertexArrayAttrib(vao, 0);
    //            glEnableVertexArrayAttrib(vao, 1);
    //            glEnableVertexArrayAttrib(vao, 2);

    //            glVertexArrayAttribFormat(vao, 0, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, position));
    //            glVertexArrayAttribFormat(vao, 1, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, color));
    //            glVertexArrayAttribFormat(vao, 2, 2, GL_FLOAT, GL_FALSE, offsetof(Vertex, texture_coordinate));

    //            glVertexArrayAttribBinding(vao, 0, 0);
    //            glVertexArrayAttribBinding(vao, 1, 0);
    //            glVertexArrayAttribBinding(vao, 2, 0);
    //        }
    //        if (ibo)
    //        {
    //            glVertexArrayElementBuffer(vao, ibo);
    //        }

    //        // TODO: build draw command
    //        // ...
    //    }
    //};

}