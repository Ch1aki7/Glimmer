#include <Glimmer.h>
#include "Glimmer/Core/EntryPoint.h"
#include "EditorLayer.h"

class GlimmerEditor : public gl::Application {
public:
	GlimmerEditor() :Application("Glimmer Editor - Cyou Branch") {
		PushLayer(new gl::EditorLayer());
	}
};

gl::Application* gl::CreateApplication() {
	return new GlimmerEditor();
}
