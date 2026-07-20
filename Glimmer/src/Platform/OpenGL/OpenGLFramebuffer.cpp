#include "glpch.h"
#include "OpenGLFramebuffer.h"
#include <glad/glad.h>

namespace gl {

	static const uint32_t s_MaxFramebufferSize = 8192;

	// ============================================================
	// 格式映射（文件内静态函数，避免 GLenum 泄漏到头文件）
	// ============================================================

	static bool IsDepthFormat(FramebufferTextureFormat format)
	{
		return format == FramebufferTextureFormat::Depth24Stencil8;
	}

	static GLenum TextureFormatToGL(FramebufferTextureFormat format)
	{
		switch (format)
		{
		case FramebufferTextureFormat::RGBA8:            return GL_RGBA;
		case FramebufferTextureFormat::RED_INTEGER:      return GL_RED_INTEGER;
		case FramebufferTextureFormat::RGBA16F:           return GL_RGBA;
		case FramebufferTextureFormat::Depth24Stencil8:   return GL_DEPTH_STENCIL;
		}
		return 0;
	}

	static GLenum TextureFormatToInternal(FramebufferTextureFormat format)
	{
		switch (format)
		{
		case FramebufferTextureFormat::RGBA8:            return GL_RGBA8;
		case FramebufferTextureFormat::RED_INTEGER:      return GL_R32I;
		case FramebufferTextureFormat::RGBA16F:           return GL_RGBA16F;
		case FramebufferTextureFormat::Depth24Stencil8:   return GL_DEPTH24_STENCIL8;
		}
		return 0;
	}

	static GLenum TextureFormatToDataType(FramebufferTextureFormat format)
	{
		switch (format)
		{
		case FramebufferTextureFormat::RGBA8:            return GL_UNSIGNED_BYTE;
		case FramebufferTextureFormat::RED_INTEGER:      return GL_INT;
		case FramebufferTextureFormat::RGBA16F:           return GL_FLOAT;
		case FramebufferTextureFormat::Depth24Stencil8:   return GL_UNSIGNED_INT_24_8;
		}
		return 0;
	}


	// ============================================================
	// 构造 / 析构
	// ============================================================

	OpenGLFramebuffer::OpenGLFramebuffer(const FramebufferSpecification& spec)
		: m_Specification(spec)
	{
		// 确保至少一个附件
		if (m_Specification.Attachments.empty())
			m_Specification.Attachments.push_back({ FramebufferTextureFormat::RGBA8 });

		Invalidate();
	}

	OpenGLFramebuffer::~OpenGLFramebuffer()
	{
		glDeleteFramebuffers(1, &m_RendererID);
		for (auto& a : m_ColorAttachments)
			glDeleteTextures(1, &a.RendererID);
		glDeleteTextures(1, &m_DepthAttachment.RendererID);
		if (m_MSAAColorRBO) glDeleteRenderbuffers(1, &m_MSAAColorRBO);
		if (m_MSAADepthRBO) glDeleteRenderbuffers(1, &m_MSAADepthRBO);
	}

	// ============================================================
	// Invalidate — 首次创建 / 完全重建
	// ============================================================

	void OpenGLFramebuffer::Invalidate()
	{
		// 清理旧资源
		if (m_RendererID)
		{
			glDeleteFramebuffers(1, &m_RendererID);
			for (auto& a : m_ColorAttachments)
				glDeleteTextures(1, &a.RendererID);
			glDeleteTextures(1, &m_DepthAttachment.RendererID);
			if (m_MSAAColorRBO) { glDeleteRenderbuffers(1, &m_MSAAColorRBO); m_MSAAColorRBO = 0; }
			if (m_MSAADepthRBO) { glDeleteRenderbuffers(1, &m_MSAADepthRBO); m_MSAADepthRBO = 0; }
		}

		glCreateFramebuffers(1, &m_RendererID);

		uint32_t w = m_Specification.Width;
		uint32_t h = m_Specification.Height;
		uint32_t samples = m_Specification.Samples;
		bool multisample = samples > 1;

		// --- 颜色附件 ---
		m_ColorAttachments.clear();
		m_ColorAttachments.resize(m_Specification.Attachments.size());

		for (size_t i = 0; i < m_Specification.Attachments.size(); i++)
		{
			auto& att = m_ColorAttachments[i];
			att.Format = m_Specification.Attachments[i].Format;

			if (IsDepthFormat(att.Format))
				continue; // 深度附件另外处理

			if (multisample)
			{
				// MSAA: 用 Renderbuffer
				glCreateRenderbuffers(1, &m_MSAAColorRBO);
				glNamedRenderbufferStorageMultisample(m_MSAAColorRBO, samples,
					TextureFormatToInternal(att.Format), w, h);
				glNamedFramebufferRenderbuffer(m_RendererID, GL_COLOR_ATTACHMENT0 + (GLenum)i,
					GL_RENDERBUFFER, m_MSAAColorRBO);
				att.RendererID = m_MSAAColorRBO; // 暂存 RBO ID（多附件时需改进）
			}
			else
			{
				glCreateTextures(GL_TEXTURE_2D, 1, &att.RendererID);
				glBindTexture(GL_TEXTURE_2D, att.RendererID);
				glTexImage2D(GL_TEXTURE_2D, 0, TextureFormatToInternal(att.Format),
					w, h, 0, TextureFormatToGL(att.Format),
					TextureFormatToDataType(att.Format), nullptr);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

				glNamedFramebufferTexture(m_RendererID, GL_COLOR_ATTACHMENT0 + (GLenum)i,
					att.RendererID, 0);
			}
		}

		// --- 深度/模板附件 ---
		bool hasDepth = false;
		for (auto& attSpec : m_Specification.Attachments)
		{
			if (IsDepthFormat(attSpec.Format))
			{
				hasDepth = true;
				m_DepthAttachment.Format = attSpec.Format;

				if (multisample)
				{
					glCreateRenderbuffers(1, &m_MSAADepthRBO);
					glNamedRenderbufferStorageMultisample(m_MSAADepthRBO, samples,
						GL_DEPTH24_STENCIL8, w, h);
					glNamedFramebufferRenderbuffer(m_RendererID, GL_DEPTH_STENCIL_ATTACHMENT,
						GL_RENDERBUFFER, m_MSAADepthRBO);
					m_DepthAttachment.RendererID = m_MSAADepthRBO;
				}
				else
				{
					glCreateTextures(GL_TEXTURE_2D, 1, &m_DepthAttachment.RendererID);
					glBindTexture(GL_TEXTURE_2D, m_DepthAttachment.RendererID);
					glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH24_STENCIL8, w, h);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

					glNamedFramebufferTexture(m_RendererID, GL_DEPTH_STENCIL_ATTACHMENT,
						m_DepthAttachment.RendererID, 0);
				}
				break;
			}
		}

		// 如果超规中没有指定深度附件，默认创建一个
		if (!hasDepth)
		{
			m_DepthAttachment.Format = FramebufferTextureFormat::Depth24Stencil8;

			if (multisample)
			{
				glCreateRenderbuffers(1, &m_MSAADepthRBO);
				glNamedRenderbufferStorageMultisample(m_MSAADepthRBO, samples,
					GL_DEPTH24_STENCIL8, w, h);
				glNamedFramebufferRenderbuffer(m_RendererID, GL_DEPTH_STENCIL_ATTACHMENT,
					GL_RENDERBUFFER, m_MSAADepthRBO);
				m_DepthAttachment.RendererID = m_MSAADepthRBO;
			}
			else
			{
				glCreateTextures(GL_TEXTURE_2D, 1, &m_DepthAttachment.RendererID);
				glBindTexture(GL_TEXTURE_2D, m_DepthAttachment.RendererID);
				glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH24_STENCIL8, w, h);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				glNamedFramebufferTexture(m_RendererID, GL_DEPTH_STENCIL_ATTACHMENT,
					m_DepthAttachment.RendererID, 0);
			}
		}

		// 设置多附件绘制目标
		if (!multisample)
		{
			std::vector<GLenum> drawBuffers;
			for (size_t i = 0; i < m_ColorAttachments.size(); i++)
				if (!IsDepthFormat(m_ColorAttachments[i].Format))
					drawBuffers.push_back(GL_COLOR_ATTACHMENT0 + (GLenum)i);
			if (!drawBuffers.empty())
				glNamedFramebufferDrawBuffers(m_RendererID, (GLsizei)drawBuffers.size(), drawBuffers.data());
		}

		// 完整性检查
		GL_CORE_ASSERT(
			glCheckNamedFramebufferStatus(m_RendererID, GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
			"Framebuffer is incomplete!");
	}

	// ============================================================
	// Resize — 原地更新纹理尺寸（不销毁纹理 ID）
	// ============================================================

	void OpenGLFramebuffer::ResizeAttachments()
	{
		uint32_t w = m_Specification.Width;
		uint32_t h = m_Specification.Height;
		uint32_t samples = m_Specification.Samples;
		bool multisample = samples > 1;

		for (size_t i = 0; i < m_ColorAttachments.size(); i++)
		{
			auto& att = m_ColorAttachments[i];
			if (IsDepthFormat(att.Format)) continue;

			if (multisample)
			{
				glNamedRenderbufferStorageMultisample(att.RendererID, samples,
					TextureFormatToInternal(att.Format), w, h);
			}
			else
			{
				glBindTexture(GL_TEXTURE_2D, att.RendererID);
				glTexImage2D(GL_TEXTURE_2D, 0, TextureFormatToInternal(att.Format),
					w, h, 0, TextureFormatToGL(att.Format),
					TextureFormatToDataType(att.Format), nullptr);
			}
		}

		if (m_DepthAttachment.RendererID)
		{
			if (multisample)
			{
				glNamedRenderbufferStorageMultisample(m_DepthAttachment.RendererID, samples,
					GL_DEPTH24_STENCIL8, w, h);
			}
			else
			{
				glDeleteTextures(1, &m_DepthAttachment.RendererID);
				glCreateTextures(GL_TEXTURE_2D, 1, &m_DepthAttachment.RendererID);
				glBindTexture(GL_TEXTURE_2D, m_DepthAttachment.RendererID);
				glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH24_STENCIL8, w, h);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				glNamedFramebufferTexture(m_RendererID, GL_DEPTH_STENCIL_ATTACHMENT,
					m_DepthAttachment.RendererID, 0);
			}
		}
	}

	void OpenGLFramebuffer::Resize(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0 || width > s_MaxFramebufferSize || height > s_MaxFramebufferSize)
		{
			GL_CORE_WARN("Framebuffer resize rejected: {0}x{1}", width, height);
			return;
		}

		m_Specification.Width = width;
		m_Specification.Height = height;
		ResizeAttachments();
	}

	// ============================================================
	// Bind / Unbind
	// ============================================================

	void OpenGLFramebuffer::Bind()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);
		glViewport(0, 0, m_Specification.Width, m_Specification.Height);
	}

	void OpenGLFramebuffer::Unbind()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	uint32_t OpenGLFramebuffer::GetColorAttachmentRendererID(uint32_t index) const
	{
		if (index < m_ColorAttachments.size())
			return m_ColorAttachments[index].RendererID;
		return 0;
	}

}
