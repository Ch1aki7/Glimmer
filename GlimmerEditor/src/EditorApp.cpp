#include <Glimmer.h>
#include "Glimmer/Core/EntryPoint.h"
#include "EditorLayer.h"

class GlimmerEditor : public gl::Application {
public:
	GlimmerEditor():Application("Glimmer Editor") {
		PushLayer(new gl::EditorLayer());
	}
};

gl::Application* gl::CreateApplication() {
	return new GlimmerEditor();
}
