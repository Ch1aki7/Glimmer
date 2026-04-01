#pragma once
#include "Glimmer/Core/Layer.h"

#include "Glimmer/Events/ApplicationEvent.h"
#include "Glimmer/Events/KeyEvent.h"
#include "Glimmer/Events/MouseEvent.h"

namespace gl {
    class ImGuiLayer : public Layer {
    public:
        ImGuiLayer();
        ~ImGuiLayer();

        virtual void OnAttach() override;
        virtual void OnDetach() override;
        virtual void OnUpdate(Timestep ts) override;
        virtual void OnEvent(Event& event) override;
        virtual void OnImGuiRender() override;

        void Begin(); // 每帧开始前呼叫
        void End();   // 每帧结束后呼叫
    private:
        bool OnMouseButtonPressedEvent(MouseButtonPressedEvent& e);
        bool OnMouseButtonReleasedEvent(MouseButtonReleasedEvent& e);
        bool OnMouseMovedEvent(MouseMovedEvent& e);
        bool OnMouseScrolledEvent(MouseScrolledEvent& e);
        //bool OnKeyPressedEvent(KeyPressedEvent& e);
        //bool OnKeyReleasedEvent(KeyReleasedEvent& e);
        bool OnKeyTypedEvent(KeyTypedEvent& e);
        bool OnWindowResizeEvent(WindowResizeEvent& e);

    private:
        float m_Time = 0.0f;
    };
}
