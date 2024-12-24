#include "shader.hpp"

// clang-format off
constexpr std::string_view kf_vert_src = "# version 300 es\n"
"in      highp vec4  a_fog;\n"
"in      highp vec3  a_xyz;\n"
"in      highp vec2  a_uv;\n"
"uniform highp mat4  u_model;\n"
"uniform highp mat4  u_view;\n"
"uniform highp mat4  u_proj;\n"
"out     highp vec4  v_fog;\n"
"out     highp vec2  v_uv;\n"

"void main(void) {\n"
"    v_fog = a_fog;\n"
"    v_uv  = a_uv;\n"
"    gl_Position = u_proj * u_view * u_model * vec4(a_xyz, 1.0);\n"
"}";

constexpr std::string_view kf_frag_src = "# version 300 es\n"
"in      highp vec4  v_fog;\n"
"in      highp vec2  v_uv;\n"
"uniform sampler2D   u_tex;\n"
"out     highp vec4  fragColor;\n"

"void main(void) {\n"
"    fragColor = texture(u_tex, v_uv) * v_fog;\n"
"}";
// clang-format on

const Shader& GetKeyframeShader() {
	static Shader shader;
	if (!shader) {
		shader.Compile(kf_vert_src, kf_frag_src);
	}
	return shader;
}
