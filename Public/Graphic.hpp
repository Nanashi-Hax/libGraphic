#pragma once

#include "gx2/surface.h"
#include "gx2r/buffer.h"
#include <gx2/shaders.h>
#include <gx2/texture.h>
#include <string>
#include <vector>
#include <span>

namespace Graphic
{
    class Shader
    {
    public:
        enum class AttributeFormat
        {
            SNorm8x1 = GX2_ATTRIB_FORMAT_SNORM_8,
            SNorm8x2 = GX2_ATTRIB_FORMAT_SNORM_8_8,
            SNorm8x4 = GX2_ATTRIB_FORMAT_SNORM_8_8_8_8,

            UNorm8x1 = GX2_ATTRIB_FORMAT_UNORM_8,
            UNorm8x2 = GX2_ATTRIB_FORMAT_UNORM_8_8,
            UNorm8x4 = GX2_ATTRIB_FORMAT_UNORM_8_8_8_8,

            SInt8x1 = GX2_ATTRIB_FORMAT_SINT_8,
            SInt8x2 = GX2_ATTRIB_FORMAT_SINT_8_8,
            SInt8x4 = GX2_ATTRIB_FORMAT_SINT_8_8_8_8,

            UInt8x1 = GX2_ATTRIB_FORMAT_UINT_8,
            UInt8x2 = GX2_ATTRIB_FORMAT_UINT_8_8,
            UInt8x4 = GX2_ATTRIB_FORMAT_UINT_8_8_8_8,

            Float32x1 = GX2_ATTRIB_FORMAT_FLOAT_32,
            Float32x2 = GX2_ATTRIB_FORMAT_FLOAT_32_32,
            Float32x3 = GX2_ATTRIB_FORMAT_FLOAT_32_32_32,
            Float32x4 = GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32
        };

        enum class EndianSwapMode
        {
            None = GX2_ENDIAN_SWAP_NONE,
            Swap8In16 = GX2_ENDIAN_SWAP_8_IN_16,
            Swap8In32 = GX2_ENDIAN_SWAP_8_IN_32,
            Default = GX2_ENDIAN_SWAP_DEFAULT,
        };

        Shader(void const * file);
        ~Shader();

        // Initialize
        void addAttribute(std::string const & name, uint32_t offset, AttributeFormat format, EndianSwapMode swap = EndianSwapMode::Swap8In32);

        // Allocate uniform buffer
        void initUniform(size_t bufferSize);

        // After setting attribute
        void initFetch();

        // Be sure to call it at the beginning of the frame
        void beginFrame();

        // Use
        void use();

        // After using shader
        // Assumes that data has already been endian converted
        void updateVertexUniform(std::string const & name, std::span<std::byte> data);
        void updatePixelUniform(std::string const & name, std::span<std::byte> data);

    private:
        class Attribute
        {
        public:
            void add(GX2AttribStream const & stream, std::string const & name, uint32_t offset, AttributeFormat format);
            std::vector<GX2AttribStream>& stream();
            std::vector<std::string>& name();
            std::vector<uint32_t>& offset();
            std::vector<AttributeFormat>& format();

        private:
            std::vector<GX2AttribStream> _stream;
            std::vector<std::string> _name;
            std::vector<uint32_t> _offset;
            std::vector<AttributeFormat> _format;
        };

        void initVertex(void const * file);
        void initPixel(void const * file);

        Attribute attribute;

        GX2RBuffer uniformBuffer;
        size_t currentOffset;

        GX2VertexShader * vertexShader;
        GX2PixelShader * pixelShader;
        GX2FetchShader * fetchShader;

        int getAttributeLocation(std::string const & name);
        uint32_t getAttributeMask(AttributeFormat format);

        int32_t getVertexUniformLocation(std::string const & name);
        int32_t getPixelUniformLocation(std::string const & name);
    };

    class ColorBuffer
    {
    public:
        enum class Target
        {
            TV,
            DRC
        };

        ColorBuffer(uint32_t width, uint32_t height);
        ~ColorBuffer();
        void use();
        void swap(Target target);

    private:
        GX2ColorBuffer * buffer;
    };
}