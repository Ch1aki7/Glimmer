#pragma once
#include "Glimmer/Core/Layer.h"


namespace gl {
    class ImGuiLayer : public Layer {
    public:
        ImGuiLayer();
        ~ImGuiLayer();

        virtual void OnAttach() override;
        virtual void OnDetach() override;
        virtual void OnUpdate(Timestep ts) override;
        virtual void OnEvent(Event& event) override;

        void Begin(); // 每帧开始前呼叫
        void End();   // 每帧结束后呼叫

		void BlockEvents(bool block) { m_BlockEvents = block; }

    private:
		bool m_BlockEvents = true;
        float m_Time = 0.0f;
    };
}
