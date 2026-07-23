#pragma once

#include <string>

namespace gl {

	struct ShaderReloadResult {
		bool Attempted = false;
		bool Success = false;
		std::string Message;
	};

}
