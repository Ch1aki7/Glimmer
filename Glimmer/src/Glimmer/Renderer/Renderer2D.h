#pragma once

#include "Glimmer/Renderer/OrthographicCamera.h"
#include "Glimmer/Renderer/Texture.h"
#include "Glimmer/Renderer/Shader.h"
#include "Glimmer/Renderer/Camera.h"

#include "Glimmer/Scene/Components.h"
namespace gl {

	class Renderer2D
	{
	public:
		static void Init();
		static void Shutdown();

		static void BeginScene(const Camera& camera, const glm::mat4& transform);
		static void BeginScene(const OrthographicCamera& camera); // TODO: Remove
		static void BeginScene(const glm::mat4& viewProjection);
		static void EndScene();
		static void Flush();
		static void StartBatch();

		static void SetEntityID(int id);

		// --- 基础绘图接口 (Quads) ---

		// 纯色方块 (Vector2 & Vector3 坐标支持)
		static void DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color);
		static void DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color);

		// 基础 DrawQuad (带平铺和染色)
		static void DrawQuad(const glm::vec2& position, const glm::vec2& size, const Ref<Texture2D>& texture, float tilingFactor = 1.0f, const glm::vec4& tintColor = glm::vec4(1.0f));
		static void DrawQuad(const glm::vec3& position, const glm::vec2& size, const Ref<Texture2D>& texture, float tilingFactor = 1.0f, const glm::vec4& tintColor = glm::vec4(1.0f));

		// transform
		static void DrawQuad(const glm::mat4& transform, const glm::vec4& color);
		static void DrawQuad(const glm::mat4& transform, const Ref<Texture2D>& texture, float tilingFactor = 1.0f, const glm::vec4& tintColor = glm::vec4(1.0f));

		// 带实体 ID 的 DrawQuad（用于鼠标拾取）
		static void DrawQuad(const glm::mat4& transform, const glm::vec4& color, int entityID);
		static void DrawQuad(const glm::mat4& transform, const Ref<Texture2D>& texture, float tilingFactor, const glm::vec4& tintColor, int entityID);

		// 便捷方法：根据 SpriteRendererComponent 自动选纯色/纹理
		static void DrawSprite(
			const glm::mat4& transform,
			const SpriteRendererComponent& src,
			int entityID,
			AssetHandle materialHandle = AssetHandle(0),
			const MaterialOverrides* overrides = nullptr);

		// 旋转 DrawQuad (纯色)
		static void DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float rotation, const glm::vec4& color);
		static void DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotation, const glm::vec4& color);

		// 旋转 DrawQuad (贴图 + 平铺 + 染色)
		static void DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float rotation, const Ref<Texture2D>& texture, float tilingFactor = 1.0f, const glm::vec4& tintColor = glm::vec4(1.0f));
		static void DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotation, const Ref<Texture2D>& texture, float tilingFactor = 1.0f, const glm::vec4& tintColor = glm::vec4(1.0f));

		static void DrawFullscreenQuad(const Ref<Shader>& shader, float depth = 0.0f);
		static void DrawPostProcess(const Ref<Shader>& shader, uint32_t inputTextureID);

		// Stats
		struct Statistics
		{
			uint32_t DrawCalls = 0;
			uint32_t QuadCount = 0;

			uint32_t GetTotalVertexCount() { return QuadCount * 4; }
			uint32_t GetTotalIndexCount() { return QuadCount * 6; }
		};
		static void ResetStats();
		static Statistics GetStats();
	private:
		static void FlushAndReset();
	};

}
