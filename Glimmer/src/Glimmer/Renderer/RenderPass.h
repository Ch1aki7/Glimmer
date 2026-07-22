#pragma once
#include "Glimmer/Core/Core.h"
#include "Glimmer/Renderer/Framebuffer.h"
#include <glm/glm.hpp>

namespace gl {

	// 单个渲染 Pass 的规格
	struct RenderPassSpecification
	{
		Ref<Framebuffer> Target;
		bool ClearColor = true;
		bool ClearDepth = true;
		glm::vec4 ClearColorValue = { 0.1f, 0.1f, 0.1f, 1.0f };
	};

	// 渲染 Pass — 封装 Begin/Clear/End 生命周期
	class RenderPass {
	public:
		// 开始一个 Pass：绑定 FBO + 清屏
		static void Begin(const RenderPassSpecification& spec);

		// 结束当前 Pass
		static void End();

		// 获取当前活跃 Pass 的规格
		static const RenderPassSpecification& GetCurrent() { return *s_Active; }

	private:
		static RenderPassSpecification* s_Active;
	};

}
