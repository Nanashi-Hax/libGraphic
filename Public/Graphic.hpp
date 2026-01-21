#pragma once

#include <gx2/shaders.h>
#include <string>
#include <vector>

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
    
        Shader(void const * file);
        ~Shader();
    
        // Initialize
        void addAttribute(std::string const & name, uint32_t offset, AttributeFormat format);
    
        // After setting attribute
        void initFetch();
    
        // Use
        void use();
    
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
        GX2VertexShader * vertexShader;
        GX2PixelShader * pixelShader;
        GX2FetchShader * fetchShader;
    
        int getAttributeLocation(std::string const & name);
        uint32_t getAttributeMask(AttributeFormat format);
    };
}