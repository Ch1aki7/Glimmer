#pragma once

#ifdef GL_PLATFORM_WINDOWS

extern gl::Application* gl::CreateApplication();

int main(int argc, char** argv)
{
    gl::Log::Init();

    GL_INFO("Ready to build something epic?");

    auto app = gl::CreateApplication();

    app->Run();

    delete app;
}

#endif
