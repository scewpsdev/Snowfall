#version 460

layout (location = 0) in vec3 v_normal;
layout (location = 1) in vec3 v_color;
layout (location = 2) in vec3 v_localpos;

layout (location = 0) out vec4 out_color;


void main()
{
	if (v_localpos.x > 1 || v_localpos.y > 1 || v_localpos.z > 1)
		discard;

	vec3 normal = v_normal;
	vec3 toLight = normalize(vec3(1, 2, 0.5));
	float d = dot(normal, toLight) * 0.5 + 0.5;
	vec3 diffuse = d * v_color;

	out_color = vec4(diffuse, 1);
}
