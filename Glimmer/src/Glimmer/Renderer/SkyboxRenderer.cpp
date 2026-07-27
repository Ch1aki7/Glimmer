#include "glpch.h"
#include "SkyboxRenderer.h"

#include "Glimmer/Renderer/Buffer.h"
#include "Glimmer/Renderer/RenderCommand.h"
#include "Glimmer/Renderer/Shader.h"
#include "Glimmer/Renderer/TextureCube.h"
#include "Glimmer/Renderer/VertexArray.h"

namespace gl {

    namespace {

        Ref<VertexArray> s_SkyboxVertexArray;

    }

    void SkyboxRenderer::Init()
    {
		float vertices[] = {
            -1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
             1.0f, -1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f
        };

        uint32_t indices[] = {
            0, 2, 1, 0, 3, 2,
            1, 6, 5, 1, 2, 6,
            5, 7, 4, 5, 6, 7,
            4, 3, 0, 4, 7, 3,
            3, 6, 2, 3, 7, 6,
            4, 1, 5, 4, 0, 1
        };

        s_SkyboxVertexArray = VertexArray::Create();
        Ref<VertexBuffer> vertexBuffer = VertexBuffer::Create(
			vertices, sizeof(vertices));
        vertexBuffer->SetLayout({
            { ShaderDataType::Float3, "a_Position" }
        });
        s_SkyboxVertexArray->AddVertexBuffer(vertexBuffer);
        s_SkyboxVertexArray->SetIndexBuffer(
			IndexBuffer::Create(indices, 36));
    }

    void SkyboxRenderer::Shutdown()
    {
        s_SkyboxVertexArray.reset();
    }

    void SkyboxRenderer::Draw(
        const Ref<TextureCube>& cubemap,
        const Ref<Shader>& shader,
        const glm::mat4& view,
        const glm::mat4& projection,
        float intensity)
    {
        if (!cubemap || !shader || !s_SkyboxVertexArray)
            return;

        RenderCommand::SetDepthFunction(DepthFunction::LessEqual);
        shader->Bind();
        shader->UploadUniformMat4("u_View", view);
        shader->UploadUniformMat4("u_Projection", projection);
        shader->UploadUniformFloat("u_Intensity", glm::max(intensity, 0.0f));
        cubemap->Bind(0);
        shader->UploadUniformInt("u_Skybox", 0);
        RenderCommand::DrawIndexed(s_SkyboxVertexArray, 36);
        RenderCommand::SetDepthFunction(DepthFunction::Less);
    }

}
