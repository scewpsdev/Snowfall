#version 460

layout (location = 0) in vec3 v_position;
layout (location = 1) in vec3 v_normal;
layout (location = 2) in vec3 v_color;
layout (location = 3) in vec3 v_localpos;

layout (location = 0) out vec4 out_position;
layout (location = 1) out vec3 out_normal;
layout (location = 2) out vec3 out_color;


void main()
{
	if (v_localpos.x > 1 || v_localpos.y > 1 || v_localpos.z > 1)
		discard;

	out_position = vec4(v_position, 1);
	out_normal = v_normal;
	out_color = v_color;
}
