// Application.h
#include "Events/Event.h"
#include "Window.h"
#include "Glimmer/Events/ApplicationEvent.h"
#include "Glimmer/LayerStack.h"
#include "Glimmer/ImGui/ImGuiLayer.h"
namespace gl { // 属于 Glimmer 引擎的命名空间
    class Application {
    public:
        Application();
        virtual ~Application();
        void Run();

        void OnEvent(Event& e); // 处理事件的中心枢纽

        void PushLayer(Layer* layer);
        void PushOverlay(Layer* overlay);

        inline static Application& Get() { return *s_Instance; }
        inline Window& GetWindow() { return *m_Window; }        
    private:
        bool OnWindowClose(WindowCloseEvent& e); // 专门处理关闭的逻辑
        std::unique_ptr<Window> m_Window; // 引擎持有的窗口指针
        bool m_Running = true;

        LayerStack m_LayerStack;
        static Application* s_Instance;
        ImGuiLayer* m_ImGuiLayer; // 【新增】：保存 ImGui 层的指针
    };
    // 提供给外部创建应用的接口
    Application* CreateApplication();
}