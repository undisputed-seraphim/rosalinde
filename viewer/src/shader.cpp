#include "shader.hpp"

// clang-format off
constexpr std::string_view kf_vert_src = "#version 450\n"
"in            uint  a_texid;\n"
"in      highp vec2  a_uv;\n"
"in      highp vec3  a_xyz;\n"
"in      highp vec4  a_fog;\n"
"uniform highp mat4  u_mvp;\n"
"out     highp vec4  v_fog;\n"
"out     highp vec2  v_uv;\n"
"flat out      uint  v_texid;\n"

"void main(void) {\n"
"    v_fog = a_fog;\n"
"    v_uv  = a_uv;\n"
"    v_texid = a_texid;\n"
"    gl_Position = u_mvp * vec4(a_xyz, 1.0);\n"
"}";

constexpr std::string_view kf_frag_src = "#version 450\n"
"in      highp vec4           v_fog;\n"
"in      highp vec2           v_uv;\n"
"flat in       uint           v_texid;\n"
"uniform highp sampler2DArray u_tex;\n"
"uniform highp float          u_alpha;\n"
"out     highp vec4           fragColor;\n"

"void main(void) {\n"
"    if (v_texid >= 0xFFFF0000u) {\n"
"        fragColor = v_fog;\n"
"    } else {\n"
"        highp ivec3 texsize = textureSize(u_tex, 0);\n"
"        highp vec3 texcoord = vec3(v_uv.x / float(texsize.x), v_uv.y / float(texsize.y), float(v_texid));\n"
"        fragColor = texture(u_tex, texcoord) * v_fog;\n"
"    }\n"
"    fragColor.a *= u_alpha;\n"
"}";
// clang-format on

const Shader& GetKeyframeShader() {
	static Shader shader;
	if (!shader) {
		shader.Compile(kf_vert_src, kf_frag_src);
	}
	return shader;
}

const KeyframeUniforms& GetKeyframeUniforms() {
	static const KeyframeUniforms uniforms = [] {
		const Shader& shader = GetKeyframeShader();
		return KeyframeUniforms{
			shader.GetUniformLocation("u_mvp"),
			shader.GetUniformLocation("u_alpha"),
			shader.GetUniformLocation("u_tex")};
	}();
	return uniforms;
}

// clang-format off
// The engine's rain/snow NVN programs are vertex shaders that wrap a particle
// field around g_WeatherCenter by g_WeatherRange, move it with
// g_WeatherMoveTotal, and extrude thin quads (g_RainWH) through g_RainMtx. The
// viewer keeps the same model but generates the particle field on the screen
// plane: each instance is one thin streak, seeded by gl_InstanceID.
constexpr std::string_view weather_vert_src = "#version 450\n"
"layout(location = 0) in highp vec2 a_corner;\n"
"out      highp vec2  v_local;\n"
"out      highp vec2  v_ndc;\n"
"out      highp float v_alpha;\n"
"uniform highp float u_time;\n"

"float hash11(highp float p) {\n"
"    p = fract(p * 0.1031);\n"
"    p *= p + 33.33;\n"
"    p *= p + p;\n"
"    return fract(p);\n"
"}\n"

"void main(void) {\n"
"    highp float id = float(gl_InstanceID) + 1.0;\n"
"    highp float r0 = hash11(id * 1.13 + 0.7);\n"
"    highp float r1 = hash11(id * 2.71 + 1.9);\n"
"    highp float r2 = hash11(id * 4.37 + 3.3);\n"
"    highp float r3 = hash11(id * 7.91 + 5.1);\n"
"    highp float speed = mix(0.55, 1.25, r0);\n"
"    highp float y = fract(r1 - u_time * speed);\n"
"    highp float x = r2;\n"
"    highp float w = mix(0.0006, 0.0016, r3);\n"
"    highp float h = mix(0.018, 0.055, r0);\n"
"    highp vec2 off = (a_corner - 0.5) * vec2(w, h);\n"
"    off.x += off.y * mix(0.20, 0.45, r2);\n"
"    highp vec2 uv = vec2(x, y) + off;\n"
"    v_local = a_corner;\n"
"    v_ndc = uv * 2.0 - 1.0;\n"
"    v_alpha = mix(0.30, 0.95, hash11(id * 11.7 + 9.1));\n"
"    v_alpha *= smoothstep(0.0, 0.06, y) * smoothstep(1.0, 0.94, y);\n"
"    gl_Position = vec4(v_ndc, 0.0, 1.0);\n"
"}";

constexpr std::string_view weather_frag_src = "#version 450\n"
"in      highp vec2  v_local;\n"
"in      highp vec2  v_ndc;\n"
"in      highp float v_alpha;\n"
"uniform highp vec2  u_ndc_min;\n"
"uniform highp vec2  u_ndc_max;\n"
"uniform highp float u_amount;\n"
"out     highp vec4  fragColor;\n"

"void main(void) {\n"
"    if (any(lessThan(v_ndc, u_ndc_min)) || any(greaterThan(v_ndc, u_ndc_max)))\n"
"        discard;\n"
"    highp float dx = abs(v_local.x * 2.0 - 1.0);\n"
"    highp float line = smoothstep(1.0, 0.0, dx);\n"
"    highp float ends = smoothstep(0.0, 0.12, v_local.y) * smoothstep(1.0, 0.88, v_local.y);\n"
"    highp float a = line * ends * v_alpha * u_amount;\n"
"    highp vec3 tint = mix(vec3(0.58, 0.68, 0.78), vec3(0.92, 0.95, 1.0), v_ndc.y * 0.5 + 0.5);\n"
"    fragColor = vec4(tint, clamp(a, 0.0, 0.55));\n"
"}";
// clang-format on

const Shader& GetWeatherShader() {
	static Shader shader;
	if (!shader) {
		shader.Compile(weather_vert_src, weather_frag_src);
	}
	return shader;
}
