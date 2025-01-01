#include "shader.hpp"

// clang-format off
constexpr std::string_view kf_vert_src = "# version 300 es\n"
"in             int  a_texid;\n"
"in      highp vec2  a_uv;\n"
"in      highp vec3  a_xyz;\n"
"in      highp vec4  a_fog;\n"
"uniform highp mat4  u_mvp;\n"
"out     highp vec4  v_fog;\n"
"out     highp vec2  v_uv;\n"
"flat out       int  v_texid;\n"

"void main(void) {\n"
"    v_fog = a_fog;\n"
"    v_uv  = a_uv;\n"
"    v_texid = a_texid;\n"
"    gl_Position = u_mvp * vec4(a_xyz, 1.0);\n"
"}";

constexpr std::string_view kf_frag_src = "# version 300 es\n"
"in      highp vec4  v_fog;\n"
"in      highp vec2  v_uv;\n"
"flat in        int  v_texid;\n"
"uniform sampler2DArray u_tex;\n"
"out     highp vec4  fragColor;\n"
"        highp ivec3 texsize;\n"
"        highp vec3  texcoord;\n"

"void main(void) {\n"
"    texsize = textureSize(u_tex, 0);\n"
"    texcoord = vec3(v_uv.x / float(texsize.x), v_uv.y / float(texsize.y), float(v_texid));\n"
// x-axis of texture must be shifted by 2 to avoid artifacts.
"    fragColor = textureOffset(u_tex, texcoord, ivec2(2, 0)) * v_fog;\n"
"}";
// clang-format on

const Shader& GetKeyframeShader() {
	static Shader shader;
	if (!shader) {
		shader.Compile(kf_vert_src, kf_frag_src);
	}
	return shader;
}
