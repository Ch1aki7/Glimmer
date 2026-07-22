#version 450 core

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(rgba8, binding = 0) uniform image2D u_Output;

void main()
{
	ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);

	// 根据像素位置生成渐变颜色
	float r = float(pixel.x) / float(gl_NumWorkGroups.x * gl_WorkGroupSize.x);
	float g = float(pixel.y) / float(gl_NumWorkGroups.y * gl_WorkGroupSize.y);
	float b = 0.5;
	float a = 1.0;

	vec4 color = vec4(r, g, b, a);
	imageStore(u_Output, pixel, color);
}
