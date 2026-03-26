// Application.h
#include "Events/Event.h"
#include "Window.h"
#include "Glimmer/Events/ApplicationEvent.h"
namespace gl { // 属于 Glimmer 引擎的命名空间
    class Application {
    public:
        Application();
        virtual ~Application();
        void Run();

        void OnEvent(Event& e); // 处理事件的中心枢纽
    private:
        bool OnWindowClose(WindowCloseEvent& e); // 专门处理关闭的逻辑
        std::unique_ptr<Window> m_Window; // 引擎持有的窗口指针
        bool m_Running = true;
    };
    // 提供给外部创建应用的接口
    Application* CreateApplication();
}