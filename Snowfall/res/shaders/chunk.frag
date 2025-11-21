#version 460

layout (location = 0) in vec3 v_position;
layout (location = 1) in vec3 v_normal;
layout (location = 2) in vec3 v_localpos;
layout (location = 3) in flat uint v_color;

layout (location = 0) out vec4 out_position;
layout (location = 1) out vec3 out_normal;
layout (location = 2) out vec3 out_color;

layout(set = 2, binding = 0) uniform sampler2D s_palette;


vec3 getColorFromID(uint id)
{
	//return id == 1 ? vec3(0.3, 0.3, 0.3) : vec3(0, 1, 0);
	float u = float(id % 4) + 0.5;
	float v = float(id / 4) + 0.5;
	return texture(s_palette, vec2(u, v) / 4.0).rgb;
	//uint rgba = hash(id);
	//uint r = rgba & 0xFF;
	//uint g = (rgba & 0xFF00) >> 8;
	//uint b = (rgba & 0xFF0000) >> 16;
	//return vec3(r / 255.0, g / 255.0, b / 255.0);
}

void main()
{
	if (v_localpos.x > 1 || v_localpos.y > 1 || v_localpos.z > 1)
		discard;

	out_position = vec4(v_position, 1);
	out_normal = v_normal;
	out_color = getColorFromID(v_color);
}
