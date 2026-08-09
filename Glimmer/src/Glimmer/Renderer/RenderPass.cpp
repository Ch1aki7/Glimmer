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
		else if (spec.ClearDepth)
			RenderCommand::ClearDepth();
	}

	void RenderPass::End()
	{
		s_Active->Target->Unbind();
		s_Active = nullptr;
	}

	void RenderPass::RebindCurrentTarget()
	{
		if (s_Active && s_Active->Target)
			s_Active->Target->Bind();
	}

}
