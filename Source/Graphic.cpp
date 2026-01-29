#include "Graphic.hpp"
#include "gx2/enum.h"
#include "gx2/shaders.h"
#include "gx2/surface.h"

#include <cstddef>
#include <gx2/mem.h>
#include <gx2/utils.h>
#include <gx2/swap.h>
#include <gx2r/buffer.h>
#include <gx2r/surface.h>
#include <gfd.h>
#include <memory/mappedmemory.h>
#include <format>
#include <stdexcept>

namespace Graphic
{
    Shader::Shader(void const * file)
    {
        initPixel(file);
        initVertex(file);
    }
    
    Shader::~Shader()
    {
        if (vertexShader) MEMFreeToMappedMemory(vertexShader);
        if (pixelShader) MEMFreeToMappedMemory(pixelShader);
        if (fetchShader) MEMFreeToMappedMemory(fetchShader);

        GX2RDestroyBufferEx(&uniformBuffer, GX2R_RESOURCE_BIND_UNIFORM_BLOCK | GX2R_RESOURCE_USAGE_CPU_WRITE | GX2R_RESOURCE_USAGE_GPU_READ);
    }
    
    void Shader::addAttribute(std::string const & name, uint32_t offset, AttributeFormat format, EndianSwapMode swap)
    {
        GX2AttribStream stream;
        stream.location = getAttributeLocation(name);
        stream.buffer = 0;
        stream.offset = offset;
        stream.format = static_cast<GX2AttribFormat>(format);
        stream.type = GX2_ATTRIB_INDEX_PER_VERTEX;
        stream.aluDivisor = 0;
        stream.mask = getAttributeMask(format);
        stream.endianSwap = static_cast<GX2EndianSwapMode>(swap);
    
        attribute.add(stream, name, offset, format);
    }

    void Shader::initUniform(size_t bufferSize)
    {
        GX2RBuffer buffer{};
        buffer.flags = GX2R_RESOURCE_BIND_UNIFORM_BLOCK | GX2R_RESOURCE_USAGE_CPU_WRITE | GX2R_RESOURCE_USAGE_GPU_READ;
        buffer.elemSize = bufferSize;
        buffer.elemCount = 1;
        GX2RCreateBuffer(&buffer);

        void * lockedBuffer = GX2RLockBufferEx(&buffer, GX2R_RESOURCE_BIND_UNIFORM_BLOCK);
        GX2Invalidate(GX2_INVALIDATE_MODE_CPU | GX2_INVALIDATE_MODE_UNIFORM_BLOCK, lockedBuffer, buffer.elemSize * buffer.elemCount);
        GX2RUnlockBufferEx(&buffer, GX2R_RESOURCE_BIND_UNIFORM_BLOCK);

        uniformBuffer = buffer;
    }
    
    void Shader::initFetch()
    {
        fetchShader = reinterpret_cast<GX2FetchShader*>(MEMAllocFromMappedMemoryEx(sizeof(GX2FetchShader), 64));
    
        uint32_t size = GX2CalcFetchShaderSizeEx(attribute.stream().size(), GX2_FETCH_SHADER_TESSELLATION_NONE, GX2_TESSELLATION_MODE_DISCRETE);
        void * program = MEMAllocFromMappedMemoryForGX2Ex(size, GX2_SHADER_PROGRAM_ALIGNMENT);
    
        GX2InitFetchShaderEx(fetchShader, reinterpret_cast<uint8_t*>(program), attribute.stream().size(), attribute.stream().data(), GX2_FETCH_SHADER_TESSELLATION_NONE, GX2_TESSELLATION_MODE_DISCRETE);
    
        GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, program, size);
    }
    
    void Shader::beginFrame()
    {
        currentOffset = 0;
    }

    void Shader::use()
    {
        GX2SetShaderMode(GX2_SHADER_MODE_UNIFORM_BLOCK);
        GX2SetVertexShader(vertexShader);
        GX2SetPixelShader(pixelShader);
        GX2SetFetchShader(fetchShader);
    }

    void Shader::updateVertexUniform(std::string const & name, std::span<std::byte> data)
    {
        uint32_t mask = GX2_UNIFORM_BLOCK_ALIGNMENT - 1;
        uint32_t alignedSize = (data.size() + mask) & ~mask;

        GX2RBuffer * buffer = &uniformBuffer;

        void* lockedBuffer = GX2RLockBufferEx(buffer, GX2R_RESOURCE_BIND_UNIFORM_BLOCK);
        void* dst = static_cast<uint8_t*>(lockedBuffer) + currentOffset;

        GX2Invalidate(GX2_INVALIDATE_MODE_CPU | GX2_INVALIDATE_MODE_UNIFORM_BLOCK, dst, alignedSize);
        memcpy(dst, data.data(), data.size());
        GX2RUnlockBufferEx(buffer, GX2R_RESOURCE_BIND_UNIFORM_BLOCK);

        int32_t location = getVertexUniformLocation(name.c_str());
        GX2RSetVertexUniformBlock(const_cast<GX2RBuffer *>(buffer), location, currentOffset);
        currentOffset += alignedSize;
    }

    void Shader::updatePixelUniform(std::string const & name, std::span<std::byte> data)
    {
        uint32_t mask = GX2_UNIFORM_BLOCK_ALIGNMENT - 1;
        uint32_t alignedSize = (data.size() + mask) & ~mask;

        GX2RBuffer * buffer = &uniformBuffer;

        void* lockedBuffer = GX2RLockBufferEx(buffer, GX2R_RESOURCE_BIND_UNIFORM_BLOCK);
        void* dst = static_cast<uint8_t*>(lockedBuffer) + currentOffset;

        GX2Invalidate(GX2_INVALIDATE_MODE_CPU | GX2_INVALIDATE_MODE_UNIFORM_BLOCK, dst, alignedSize);
        memcpy(dst, data.data(), data.size());
        GX2RUnlockBufferEx(buffer, GX2R_RESOURCE_BIND_UNIFORM_BLOCK);

        int32_t location = getPixelUniformLocation(name.c_str());
        GX2RSetPixelUniformBlock(const_cast<GX2RBuffer *>(buffer), location, currentOffset);
        currentOffset += alignedSize;
    }
    
    void Shader::initPixel(void const * file)
    {
        uint32_t headerSize = GFDGetPixelShaderHeaderSize(0, file);
        if (!headerSize)
        {
            throw std::invalid_argument(std::format("{}: headerSize == 0", __FUNCTION__));
        }
    
        uint32_t programSize = GFDGetPixelShaderProgramSize(0, file);
        if (!programSize)
        {
            throw std::invalid_argument(std::format("{}: programSize == 0", __FUNCTION__));
        }
    
        GX2PixelShader * shader = reinterpret_cast<GX2PixelShader*>(MEMAllocFromMappedMemoryEx(headerSize, 64));
        if (!shader)
        {
            throw std::invalid_argument(std::format("{}: MEMAllocFromMappedMemoryEx({}, 64) failed", __FUNCTION__, headerSize));
        }
    
        void * program = MEMAllocFromMappedMemoryForGX2Ex(programSize, GX2_SHADER_PROGRAM_ALIGNMENT);
        if (!program)
        {
            if(shader) MEMFreeToMappedMemory(shader);
            throw std::invalid_argument(std::format("{}: MEMAllocFromMappedMemoryForGX2Ex failed", __FUNCTION__));
        }
    
        if (!GFDGetPixelShader(shader, program, 0, file))
        {
            if(shader) MEMFreeToMappedMemory(shader);
            if(program) MEMFreeToMappedMemory(program);
            GX2RUnlockBufferEx(&shader->gx2rBuffer, GX2R_RESOURCE_DISABLE_CPU_INVALIDATE | GX2R_RESOURCE_DISABLE_GPU_INVALIDATE);
            throw std::invalid_argument(std::format("{}: GFDGetPixelShader failed", __FUNCTION__));
        }
        pixelShader = shader;
        pixelShader->size = programSize;
    
        GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, pixelShader->program, pixelShader->size);
    }
    
    void Shader::initVertex(void const * file)
    {
        uint32_t headerSize = GFDGetVertexShaderHeaderSize(0, file);
        if (!headerSize)
        {
            throw std::invalid_argument(std::format("{}: headerSize == 0", __FUNCTION__));
        }
    
        uint32_t programSize = GFDGetVertexShaderProgramSize(0, file);
        if (!programSize)
        {
            throw std::invalid_argument(std::format("{}: programSize == 0", __FUNCTION__));
        }
    
        GX2VertexShader * shader = reinterpret_cast<GX2VertexShader*>(MEMAllocFromMappedMemoryEx(headerSize, 64));
        if (!shader)
        {
            throw std::invalid_argument(std::format("{}: MEMAllocFromMappedMemoryEx({}, 64) failed", __FUNCTION__, headerSize));
        }
    
        void * program = MEMAllocFromMappedMemoryForGX2Ex(programSize, GX2_SHADER_PROGRAM_ALIGNMENT);
        if (!program)
        {
            if(shader) MEMFreeToMappedMemory(shader);
            throw std::invalid_argument(std::format("{}: MEMAllocFromMappedMemoryForGX2Ex failed", __FUNCTION__));
        }
    
        if (!GFDGetVertexShader(shader, program, 0, file))
        {
            if(shader) MEMFreeToMappedMemory(shader);
            if(program) MEMFreeToMappedMemory(program);
            GX2RUnlockBufferEx(&shader->gx2rBuffer, GX2R_RESOURCE_DISABLE_CPU_INVALIDATE | GX2R_RESOURCE_DISABLE_GPU_INVALIDATE);
            throw std::invalid_argument(std::format("{}: GFDGetvertexShader failed", __FUNCTION__));
        }
        vertexShader = shader;
        vertexShader->size = programSize;
    
        GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, vertexShader->program, vertexShader->size);
    }
    
    int Shader::getAttributeLocation(const std::string& name)
    {
        for (uint32_t i = 0; i < vertexShader->attribVarCount; ++i)
        {
            if (vertexShader->attribVars[i].name == name)
            {
                return vertexShader->attribVars[i].location;
            }
        }
        throw std::invalid_argument(std::format("Attribute name is invalid: {}", name));
    }
    
    uint32_t Shader::getAttributeMask(AttributeFormat format)
    {
        switch (format)
        {
            case AttributeFormat::SNorm8x1:
            case AttributeFormat::UNorm8x1:
            case AttributeFormat::SInt8x1:
            case AttributeFormat::UInt8x1:
            case AttributeFormat::Float32x1:
            {
                return GX2_SEL_MASK(GX2_SQ_SEL_X, GX2_SQ_SEL_0, GX2_SQ_SEL_0, GX2_SQ_SEL_1);
            }
            case AttributeFormat::SNorm8x2:
            case AttributeFormat::UNorm8x2:
            case AttributeFormat::SInt8x2:
            case AttributeFormat::UInt8x2:
            case AttributeFormat::Float32x2:
            {
                return GX2_SEL_MASK(GX2_SQ_SEL_X, GX2_SQ_SEL_Y, GX2_SQ_SEL_0, GX2_SQ_SEL_1);
            }
            case AttributeFormat::Float32x3:
            {
                return GX2_SEL_MASK(GX2_SQ_SEL_X, GX2_SQ_SEL_Y, GX2_SQ_SEL_Z, GX2_SQ_SEL_1);
            }
            case AttributeFormat::SNorm8x4:
            case AttributeFormat::UNorm8x4:
            case AttributeFormat::SInt8x4:
            case AttributeFormat::UInt8x4:
            case AttributeFormat::Float32x4:
            {
                return GX2_SEL_MASK(GX2_SQ_SEL_X, GX2_SQ_SEL_Y, GX2_SQ_SEL_Z, GX2_SQ_SEL_W);
            }
            default:
            {
                return GX2_SEL_MASK(GX2_SQ_SEL_0, GX2_SQ_SEL_0, GX2_SQ_SEL_0, GX2_SQ_SEL_1);
            }
        }
    }

    int32_t Shader::getVertexUniformLocation(std::string const & name)
    {
        for (uint32_t i = 0; i < vertexShader->uniformBlockCount; i++)
        {
            if(vertexShader->uniformBlocks[i].name == name)
            {
                return vertexShader->uniformBlocks[i].offset;
            }
        }
        throw std::invalid_argument(std::format("VertexUniform name: {} is invalid", name));
    }

    int32_t Shader::getPixelUniformLocation(std::string const & name)
    {
        for (uint32_t i = 0; i < pixelShader->uniformBlockCount; i++)
        {
            if(pixelShader->uniformBlocks[i].name == name)
            {
                return pixelShader->uniformBlocks[i].offset;
            }
        }
        throw std::invalid_argument(std::format("PixelUniform name: {} is invalid", name));
    }
    
    void Shader::Attribute::add(GX2AttribStream const & stream, std::string const & name, uint32_t offset, AttributeFormat format)
    {
        _stream.push_back(stream);
        _name.push_back(name);
        _offset.push_back(offset);
        _format.push_back(format);
    }
    
    std::vector<GX2AttribStream>& Shader::Attribute::stream()
    {
        return _stream;
    }
    
    std::vector<std::string>& Shader::Attribute::name()
    {
        return _name;
    }
    
    std::vector<uint32_t>& Shader::Attribute::offset()
    {
        return _offset;
    }
    
    std::vector<Shader::AttributeFormat>& Shader::Attribute::format()
    {
        return _format;
    }

    ColorBuffer::ColorBuffer(uint32_t width, uint32_t height)
    {
        buffer = new GX2ColorBuffer();
        memset(buffer, 0, sizeof(GX2ColorBuffer));

        buffer->surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
        buffer->surface.use = GX2_SURFACE_USE_COLOR_BUFFER;
        buffer->surface.width = width;
        buffer->surface.height = height;
        buffer->surface.depth = 1;
        buffer->surface.mipLevels = 1;
        buffer->surface.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
        buffer->surface.aa = GX2_AA_MODE1X;
        buffer->surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;

        GX2RCreateSurface
        (
            &buffer->surface,
            GX2R_RESOURCE_BIND_COLOR_BUFFER |
            GX2R_RESOURCE_USAGE_GPU_WRITE |
            GX2R_RESOURCE_USAGE_GPU_READ
        );
        GX2InitColorBufferRegs(buffer);
    }

    ColorBuffer::~ColorBuffer()
    {
        if (buffer)
        {
            if (buffer->surface.image)
            {
                GX2RDestroySurfaceEx
                (
                    &buffer->surface,
                    GX2R_RESOURCE_BIND_COLOR_BUFFER |
                    GX2R_RESOURCE_USAGE_GPU_WRITE |
                    GX2R_RESOURCE_USAGE_GPU_READ
                );
                buffer->surface.image = nullptr;
            }

            delete buffer;
        }
    }

    void ColorBuffer::use()
    {
        GX2SetColorBuffer(buffer, GX2_RENDER_TARGET_0);
    }

    void ColorBuffer::swap(Target target)
    {
        if(target == Target::TV) GX2CopyColorBufferToScanBuffer(buffer, GX2_SCAN_TARGET_TV);
        if(target == Target::DRC) GX2CopyColorBufferToScanBuffer(buffer, GX2_SCAN_TARGET_DRC);
    }
}