#pragma once

#include "Glimmer/Renderer/OrthographicCamera.h"
#include "Glimmer/Renderer/Texture.h"
#include "Glimmer/Renderer/Shader.h"

namespace gl {

	class Renderer2D
	{
	public:
		static void Init();
		static void Shutdown();

		static void BeginScene(const OrthographicCamera& camera);
		static void EndScene();

		// --- 基础绘图接口 (Quads) ---

		// 纯色方块 (Vector2 & Vector3 坐标支持)
		static void DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color);
		static void DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color);

		// 贴图方块
		static void DrawQuad(const glm::vec2& position, const glm::vec2& size, const Ref<Texture2D>& texture);
		static void DrawQuad(const glm::vec3& position, const glm::vec2& size, const Ref<Texture2D>& texture);

		static void DrawFullscreenQuad(const Ref<Shader>& shader, float depth = 0.0f);

	};

}
