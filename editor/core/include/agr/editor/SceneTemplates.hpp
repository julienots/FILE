#pragma once
#include <vector>

#include "agr/editor/Project.hpp"
#include "agr/scene/Scene.hpp"

namespace agr::editor {

/// Fills an empty scene with the content of a template. Every template is
/// playable as is: a camera, lighting where relevant and visible content.
void buildSceneTemplate(Scene& scene, SceneTemplate tmpl);
SceneTemplate defaultTemplateFor(ProjectMode mode);
/// Templates offered for a project mode (the default first).
std::vector<SceneTemplate> templatesFor(ProjectMode mode);
/// Every template, for menus and tests.
std::vector<SceneTemplate> allTemplates();
const char* templateName(SceneTemplate tmpl);
/// Project mode a template is made for (2D, 2.5D, 3D).
ProjectMode templateMode(SceneTemplate tmpl);
/// Complete playable games (as opposed to empty bases and showcases).
bool isGameTemplate(SceneTemplate tmpl);
const char* templateDescription(SceneTemplate tmpl);

} // namespace agr::editor
