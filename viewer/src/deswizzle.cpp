#include "deswizzle.hpp"

#include <eltolinde.hpp>

#include <glad/glad.h>

#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace {

// Pass 1: Tegra-X1 block deswizzle (validated against swizzle.py). BCn writes a
// block-contiguous stream; NONE writes pixel-linear RGBA.
const char* kDeswizzle = R"GLSL(#version 450
layout(local_size_x = 64) in;
layout(std430, binding = 0) readonly buffer In { uint src[]; };
layout(std430, binding = 1) writeonly buffer Out { uint dst[]; };
uniform uint u_w, u_h, u_hb, u_unit, u_bpp, u_rgba;
uint pow2_round_up(uint x){ x-=1u; x|=x>>1u; x|=x>>2u; x|=x>>4u; x|=x>>8u; x|=x>>16u; return x+1u; }
uint gob(uint x, uint y, uint iw, uint bpp, uint bh) {
    uint iwg = (iw*bpp + 63u)/64u;
    uint a = (y/(8u*bh))*512u*bh*iwg + (x*bpp/64u)*512u*bh + ((y%(8u*bh))/8u)*512u;
    uint xx = x*bpp;
    return a + ((xx%64u)/32u)*256u + ((y%8u)/2u)*64u + ((xx%32u)/16u)*32u + (y%2u)*16u + (xx%16u);
}
void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= u_w*u_hb) return;
    uint y = idx / u_w, x = idx % u_w;
    uint bh = min(16u, pow2_round_up(u_hb/8u));
    uint i = gob(x, y, u_w, u_bpp, bh) / u_bpp;
    if (y >= u_h) return;
    if (u_rgba == 1u) {
        uint wpx = u_w * 4u;
        for (uint r = 0u; r < 4u; ++r) {
            uint dp = (y*4u + r)*wpx + x*4u;
            uint s = i*16u + r*4u;
            for (uint c = 0u; c < 4u; ++c) dst[dp + c] = src[s + c];
        }
    } else {
        uint d = (y*u_w + x)*u_unit, s = i*u_unit;
        for (uint k = 0u; k < u_unit; ++k) dst[d + k] = src[s + k];
    }
}
)GLSL";

// Pass 2: decode the (deswizzled) native BCn texture into the RGBA8 array slice.
const char* kExpand = R"GLSL(#version 450
layout(local_size_x = 8, local_size_y = 8) in;
uniform sampler2D u_src;
layout(rgba8, binding = 0) writeonly uniform image2DArray u_dst;
uniform int u_layer;
void main() {
    ivec2 p = ivec2(gl_GlobalInvocationID.xy);
    ivec2 sz = textureSize(u_src, 0);
    if (p.x >= sz.x || p.y >= sz.y) return;
    imageStore(u_dst, ivec3(p, u_layer), texelFetch(u_src, p, 0));
}
)GLSL";

GLuint compile(GLenum stage, const char* src, const char* name) {
	GLuint s = glCreateShader(stage);
	glShaderSource(s, 1, &src, nullptr);
	glCompileShader(s);
	GLint ok = 0;
	glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char log[4096];
		glGetShaderInfoLog(s, sizeof(log), nullptr, log);
		throw std::runtime_error(std::string("deswizzle ") + name + " shader: " + log);
	}
	GLuint p = glCreateProgram();
	glAttachShader(p, s);
	glLinkProgram(p);
	glGetProgramiv(p, GL_LINK_STATUS, &ok);
	if (!ok) {
		char log[4096];
		glGetProgramInfoLog(p, sizeof(log), nullptr, log);
		throw std::runtime_error(std::string("deswizzle ") + name + " program: " + log);
	}
	glDeleteShader(s);
	return p;
}

GLenum gl_internal(FTX::Compression c) {
	switch (c) {
	case FTX::Compression::BC7_UNORM:
	case FTX::Compression::BC7_SRGB:
		return GL_COMPRESSED_RGBA_BPTC_UNORM;
	case FTX::Compression::BC3_UNORM:
	case FTX::Compression::BC3_SRGB:
		return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
	case FTX::Compression::BC4_UNORM:
	case FTX::Compression::BC4_SNORM:
		return GL_COMPRESSED_RED_RGTC1;
	default:
		return 0;
	}
}

void set1u(GLuint prog, const char* name, unsigned v) { glUniform1ui(glGetUniformLocation(prog, name), v); }

} // namespace

Deswizzler::Deswizzler() {
	_deswizzle = compile(GL_COMPUTE_SHADER, kDeswizzle, "deswizzle");
	_expand = compile(GL_COMPUTE_SHADER, kExpand, "expand");
	glGenBuffers(1, &_in);
	glGenBuffers(1, &_out);
}

Deswizzler::~Deswizzler() {
	if (_tmp)
		glDeleteTextures(1, &_tmp);
	if (_in)
		glDeleteBuffers(1, &_in);
	if (_out)
		glDeleteBuffers(1, &_out);
	if (_deswizzle)
		glDeleteProgram(_deswizzle);
	if (_expand)
		glDeleteProgram(_expand);
}

void Deswizzler::ensure_buffers(size_t bytes) {
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, _in);
	glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(bytes), nullptr, GL_STREAM_DRAW);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, _out);
	glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(bytes), nullptr, GL_STREAM_DRAW);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void Deswizzler::upload_slice(
	unsigned array_tex,
	unsigned layer,
	unsigned width,
	unsigned height,
	FTX::Compression comp,
	std::span<const uint8_t> payload) {
	const bool rgba = FTX::block_bytes(comp) == 0;
	const unsigned bytes_per_block = rgba ? 64u : FTX::block_bytes(comp);
	const unsigned uints_per_block = bytes_per_block / 4u;
	const unsigned bpp = rgba ? 16u : FTX::block_bytes(comp); // GOB bpp (16 for BC7/NONE, 8 for BC3/BC4)

	const unsigned wb = width / 4;
	const unsigned himg = height / 4;
	const unsigned nsrc = static_cast<unsigned>(payload.size() / bytes_per_block);
	const unsigned hb = nsrc / wb;
	const unsigned total = wb * hb;

	ensure_buffers(payload.size());

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, _in);
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, static_cast<GLsizeiptr>(payload.size()), payload.data());
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, _in);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, _out);

	glUseProgram(_deswizzle);
	set1u(_deswizzle, "u_w", wb);
	set1u(_deswizzle, "u_h", himg);
	set1u(_deswizzle, "u_hb", hb);
	set1u(_deswizzle, "u_unit", uints_per_block);
	set1u(_deswizzle, "u_bpp", bpp);
	set1u(_deswizzle, "u_rgba", rgba ? 1u : 0u);
	glDispatchCompute((total + 63u) / 64u, 1, 1);
	glMemoryBarrier(GL_ALL_BARRIER_BITS);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);

	if (rgba) {
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, _out);
		glBindTexture(GL_TEXTURE_2D_ARRAY, array_tex);
		glTexSubImage3D(
			GL_TEXTURE_2D_ARRAY,
			0,
			0,
			0,
			static_cast<GLint>(layer),
			width,
			height,
			1,
			GL_RGBA,
			GL_UNSIGNED_BYTE,
			static_cast<const void*>(nullptr));
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
		glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
		return;
	}

	// Upload the deswizzled block stream as a native compressed 2D texture,
	// then decode it into the array slice.
	const GLsizei image_bytes = static_cast<GLsizei>(wb * himg * bytes_per_block);
	if (!_tmp)
		glGenTextures(1, &_tmp);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, _out);
	glBindTexture(GL_TEXTURE_2D, _tmp);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glCompressedTexImage2D(
		GL_TEXTURE_2D,
		0,
		gl_internal(comp),
		static_cast<GLsizei>(width),
		static_cast<GLsizei>(height),
		0,
		image_bytes,
		static_cast<const void*>(nullptr));
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

	glUseProgram(_expand);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, _tmp);
	glUniform1i(glGetUniformLocation(_expand, "u_src"), 0);
	glUniform1i(glGetUniformLocation(_expand, "u_layer"), static_cast<GLint>(layer));
	// Bind the whole array (layered) so the shader's image2DArray + z=layer is
	// valid; binding a single layer (layered=GL_FALSE) needs image2D instead.
	glBindImageTexture(0, array_tex, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA8);
	glDispatchCompute((width + 7u) / 8u, (height + 7u) / 8u, 1);
	glMemoryBarrier(GL_ALL_BARRIER_BITS);
	glBindImageTexture(0, 0, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA8);
	glBindTexture(GL_TEXTURE_2D, 0);
}
