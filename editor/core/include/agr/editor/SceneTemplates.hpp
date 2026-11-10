#pragma once
#include "agr/editor/Project.hpp"
#include "agr/scene/Scene.hpp"

namespace agr::editor {

/// Fills an empty scene with the content of a template. Every template is
/// playable as is: a camera, lighting where relevant and visible content.
void buildSceneTemplate(Scene& scene, SceneTemplate tmpl);
SceneTemplate defaultTemplateFor(ProjectMode mode);
const char* templateName(SceneTemplate tmpl);
const char* templateDescription(SceneTemplate tmpl);

} // namespace agr::editor
