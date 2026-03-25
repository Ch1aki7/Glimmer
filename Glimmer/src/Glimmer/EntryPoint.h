#pragma once

#ifdef GL_PLATFORM_WINDOWS

extern gl::Application* gl::CreateApplication();

int main(int argc, char** argv)
{
    auto app = gl::CreateApplication();

    app->Run();

    delete app;
}

#endif