#pragma once
// API reference generated from the engine itself: every registered component with its fields
// (type, range, flags, choices, description) and the Lua API (agr.* modules and entity methods)
// read from a live script engine. `agr_editor --api-reference docs/API.md` regenerates it.
#include <string>

namespace agr {
class ScriptEngine;
}

namespace agr::editor {

/// Markdown reference; `scripts` may be null (components only).
std::string apiReferenceMarkdown(ScriptEngine* scripts);

} // namespace agr::editor
