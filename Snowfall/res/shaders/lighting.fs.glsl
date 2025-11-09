#version 460

layout (location = 0) in vec2 v_texcoord;

layout (location = 0) out vec4 out_color;

layout(set = 2, binding = 0) uniform sampler2D s_position;
layout(set = 2, binding = 1) uniform sampler2D s_normal;
layout(set = 2, binding = 2) uniform sampler2D s_color;


void main()
{
	vec4 positionW = texture(s_position, v_texcoord);
	if (positionW.w < 0.5)
		discard;

	vec3 position = positionW.xyz;
	vec3 normal = texture(s_normal, v_texcoord).rgb;
	vec3 color = texture(s_color, v_texcoord).rgb;

	vec3 toLight = normalize(vec3(1, 2, 0.5));
	float d = dot(normal, toLight) * 0.5 + 0.5;
	vec3 diffuse = d * color;
	
	out_color = vec4(diffuse, 1);
}
