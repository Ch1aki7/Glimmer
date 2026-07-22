#include "glpch.h"
#include "RenderPass.h"
#include "Glimmer/Renderer/RenderCommand.h"

namespace gl {

	RenderPassSpecification* RenderPass::s_Active = nullptr;

	void RenderPass::Begin(const RenderPassSpecification& spec)
	{
		static thread_local RenderPassSpecification current;
		current = spec;
		s_Active = &current;

		spec.Target->Bind();
		if (spec.ClearColor)
		{
			RenderCommand::SetClearColor(spec.ClearColorValue);
			RenderCommand::Clear();
		}
	}

	void RenderPass::End()
	{
		s_Active->Target->Unbind();
		s_Active = nullptr;
	}

}
