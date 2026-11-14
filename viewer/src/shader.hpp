#pragma once
#include <glxx/shader.hpp>

struct KeyframeUniforms {
	int mvp = -1;
	int alpha = -1;
	int tex = -1;
};

const Shader& GetKeyframeShader();
const KeyframeUniforms& GetKeyframeUniforms();
const Shader& GetWeatherShader();
