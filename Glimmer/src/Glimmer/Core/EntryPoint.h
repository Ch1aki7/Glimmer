#pragma once

#ifdef GL_PLATFORM_WINDOWS

extern gl::Application* gl::CreateApplication();

int main(int argc, char** argv)
{
    gl::Log::Init();

    GL_INFO("Ready to build something epic?");

	GL_PROFILE_BEGIN_SESSION("Runtime", "GlimmerProfile-Startup.json");
    auto app = gl::CreateApplication();
	GL_PROFILE_END_SESSION();

	GL_PROFILE_BEGIN_SESSION("Runtime", "GlimmerProfile-Runtime.json");
    app->Run();
	GL_PROFILE_END_SESSION();

	GL_PROFILE_BEGIN_SESSION("Runtime", "GlimmerProfile-Shutdown.json");
    delete app;
	GL_PROFILE_END_SESSION();
}

#endif
