#include "shader.hpp"

// clang-format off
constexpr std::string_view kf_vert_src = "#version 450\n"
"layout(location = 0) in      uint  a_texid;\n"
"layout(location = 1) in highp vec2  a_uv;\n"
"layout(location = 2) in highp vec3  a_xyz;\n"
"layout(location = 3) in highp vec4  a_fog;\n"
"layout(location = 4) in highp float a_grad;\n"
"uniform highp mat4  u_mvp;\n"
"uniform highp float u_grad_enable;\n"
"out     highp vec4  v_fog;\n"
"out     highp vec2  v_uv;\n"
"out     highp float v_grad;\n"
"flat out      uint  v_texid;\n"

"void main(void) {\n"
"    v_fog = a_fog;\n"
"    v_uv  = a_uv;\n"
"    v_grad = u_grad_enable > 0.5 ? a_grad : -1.0;\n"
"    v_texid = a_texid;\n"
"    gl_Position = u_mvp * vec4(a_xyz, 1.0);\n"
"}";

constexpr std::string_view kf_frag_src = "#version 450\n"
"in      highp vec4           v_fog;\n"
"in      highp vec2           v_uv;\n"
"in      highp float          v_grad;\n"
"flat in       uint           v_texid;\n"
"uniform highp sampler2DArray u_tex;\n"
"uniform highp sampler2DArray u_gradient;\n"
"uniform highp float          u_alpha;\n"
"uniform highp float          u_grad_index;\n"
"out     highp vec4           fragColor;\n"

"void main(void) {\n"
"    highp vec4 sample_col;\n"
"    if (v_texid >= 0xFFFF0000u) {\n"
"        sample_col = vec4(1.0);\n"
"    } else {\n"
"        highp ivec3 texsize = textureSize(u_tex, 0);\n"
"        highp vec3 texcoord = vec3(v_uv.x / float(texsize.x), v_uv.y / float(texsize.y), float(v_texid));\n"
"        sample_col = texture(u_tex, texcoord);\n"
"    }\n"
"    highp vec4 base = sample_col * v_fog;\n"
"    if (v_grad >= 0.0) {\n"
"        highp float lum = dot(base.rgb, vec3(0.223499998, 0.631299973, 0.0588000007));\n"
"        highp vec3 ramp = texture(u_gradient, vec3(lum, v_grad, 0.0)).rgb;\n"
"        base.rgb = mix(base.rgb, ramp, u_grad_index);\n"
"    }\n"
"    fragColor = base;\n"
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

// clang-format off
constexpr std::string_view weather_vert_src = "#version 450\n"
"layout(location = 0) in highp vec2 a_ndc;\n"
"out      highp vec2 v_ndc;\n"
"void main(void) {\n"
"    v_ndc = a_ndc;\n"
"    gl_Position = vec4(a_ndc, 0.0, 1.0);\n"
"}";

constexpr std::string_view weather_frag_src = "#version 450\n"
"in      highp vec2  v_ndc;\n"
"uniform highp vec2  u_ndc_min;\n"
"uniform highp vec2  u_ndc_max;\n"
"uniform highp float u_amount;\n"
"uniform highp float u_time;\n"
"out     highp vec4  fragColor;\n"

"float hash(highp vec2 p) {\n"
"    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);\n"
"}\n"

"void main(void) {\n"
"    if (any(lessThan(v_ndc, u_ndc_min)) || any(greaterThan(v_ndc, u_ndc_max)))\n"
"        discard;\n"
"    highp vec2 uv = v_ndc * 0.5 + 0.5;\n"
"    highp vec2 p = vec2(uv.x * (16.0 / 9.0), uv.y);\n"
"    highp float a = 0.0;\n"
"    for (int i = 0; i < 3; ++i) {\n"
"        highp float fi = float(i);\n"
"        highp vec2 q = p * vec2(80.0 + fi * 26.0, 16.0 + fi * 7.0);\n"
"        q.x += p.y * (10.0 + fi * 6.0);\n"
"        q.y -= u_time * (22.0 + fi * 14.0);\n"
"        highp float col = floor(q.x);\n"
"        highp float rnd = hash(vec2(col, fi * 17.0 + 1.0));\n"
"        highp float y = fract(q.y + rnd);\n"
"        highp float streak = smoothstep(0.0, 0.18, y) * smoothstep(1.0, 0.55, y);\n"
"        highp float sparse = step(0.78, hash(vec2(col, floor(q.y * 0.09) + fi * 31.0)));\n"
"        a += streak * sparse * (0.32 - fi * 0.08);\n"
"    }\n"
"    a *= u_amount * 1.6;\n"
"    highp vec3 tint = mix(vec3(0.58, 0.68, 0.78), vec3(0.92, 0.95, 1.0), uv.y);\n"
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
