#type vertex
#version 330 core
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;
out vec2 v_TexCoord;
void main() {
	v_TexCoord = a_TexCoord;
	gl_Position = vec4(a_Position, 1.0); // 直接占满 NDC 空间
}

#type fragment
#version 330 core
layout(location = 0) out vec4 color;
in vec2 v_TexCoord;
uniform sampler2D u_Texture; // 传入的 Framebuffer 贴图

void main() {
	vec4 texColor = texture(u_Texture, v_TexCoord);
	// 使用工业标准权重计算灰度
	float gray = dot(texColor.rgb, vec3(0.299, 0.587, 0.114));
	color = vec4(vec3(gray), texColor.a);
}
