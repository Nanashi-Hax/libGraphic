#include "Graphic.hpp"
#include "gx2/enum.h"
#include "gx2/shaders.h"

#include <gx2/mem.h>
#include <gx2/utils.h>
#include <gfd.h>
#include <memory/mappedmemory.h>
#include <format>
#include <stdexcept>

#include <whb/log.h>

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
}

void Shader::addAttribute(std::string const & name, uint32_t offset, AttributeFormat format)
{
    GX2AttribStream stream;
    stream.location = getAttributeLocation(name);
    stream.buffer = 0;
    stream.offset = offset;
    stream.format = static_cast<GX2AttribFormat>(format);
    stream.type = GX2_ATTRIB_INDEX_PER_VERTEX;
    stream.aluDivisor = 0;
    stream.mask = getAttributeMask(format);
    stream.endianSwap = GX2_ENDIAN_SWAP_DEFAULT;

    attribute.add(stream, name, offset, format);
}

void Shader::initFetch()
{
    fetchShader = reinterpret_cast<GX2FetchShader*>(MEMAllocFromMappedMemoryEx(sizeof(GX2FetchShader), 64));

    uint32_t size = GX2CalcFetchShaderSizeEx(attribute.stream().size(), GX2_FETCH_SHADER_TESSELLATION_NONE, GX2_TESSELLATION_MODE_DISCRETE);
    void * program = MEMAllocFromMappedMemoryForGX2Ex(size, GX2_SHADER_PROGRAM_ALIGNMENT);

    GX2InitFetchShaderEx(fetchShader, reinterpret_cast<uint8_t*>(program), attribute.stream().size(), attribute.stream().data(), GX2_FETCH_SHADER_TESSELLATION_NONE, GX2_TESSELLATION_MODE_DISCRETE);

    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, program, size);
}

void Shader::use()
{
    GX2SetVertexShader(vertexShader);
    GX2SetPixelShader(pixelShader);
    GX2SetFetchShader(fetchShader);
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