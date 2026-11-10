#pragma once
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "agr/editor/ui/ImGuiRenderer.hpp"
#include "agr/editor/ui/Panels.hpp"
#include "agr/runtime/Application.hpp"

namespace agr::editor {

struct EditorOptions {
    std::string resourcesDir;    ///< fonts/ and templates/ (copied next to the executable)
    std::string projectPath;     ///< Open this project at startup (otherwise: project hub)
    std::string smokeTestDir;    ///< Non-empty: run the scripted end-to-end test in this folder
    std::string hubScreenshot;   ///< Non-empty: capture the project hub to this PNG and quit
};

/// The AGR editor application: project hub, docked panels, menus, toolbar,
/// shortcuts, and the frame structure (scene view render target, then UI).
class EditorApp final : public Application {
public:
    EditorApp(SdlPlatform* platform, EditorOptions options);
    ~EditorApp() override;

    Status onStart(Engine& engine) override;
    void onUpdate(Engine& engine, const FrameTime& time) override;
    void onRender(Engine& engine, const FrameTime& time) override;
    void onShutdown(Engine& engine) override;
    int exitCode() const { return m_exitCode; }

private:
    void drawHub();
    void drawEditor(const FrameTime& time);
    void drawMenuBar();
    void drawToolbar(float height);
    void drawStatusBar(float height);
    void drawModals();
    void handleShortcuts();
    void buildDefaultLayout(unsigned int dockspace);
    void openProject(const std::string& path);
    void createProject();
    void startPlay();
    void stopPlay();
    void saveOrSaveAs();
    void updateWindowTitle();
    void loadSettings();
    void saveSettings();
    void captureNextFrame(std::string path) { m_capturePath = std::move(path); }
    void smokeTest(const FrameTime& time);
    void check(bool condition, const char* what);
    void pushMouse(u32 sdlEventType, ImVec2 pos);
    void pushKey(u32 sdlKey, u32 scancode, u16 mod, bool down);

    SdlPlatform* m_platform;
    EditorOptions m_options;
    Engine* m_engine = nullptr;
    std::unique_ptr<EditorContext> m_ctx;
    std::unique_ptr<EditorShared> m_shared;
    std::unique_ptr<ImGuiRenderer> m_imgui;
    HierarchyPanel m_hierarchy;
    InspectorPanel m_inspector;
    ViewportPanel m_viewport;
    AssetBrowserPanel m_assets;
    ConsolePanel m_console;
    StatsPanel m_stats;
    RecentProjects m_recent;
    std::string m_settingsDir, m_iniPath;
    bool m_layoutBuilt = false;
    bool m_resetLayout = false;
    bool m_showDemo = false;
    bool m_gameViewBeforePlay = false;
    bool m_closeProjectRequested = false;
    std::string m_title;
    std::string m_capturePath;

    // project hub
    struct Wizard {
        ProjectMode mode = ProjectMode::Mode3D;
        std::string name = "Mon Jeu";
        std::string location;
        bool desktop = true;
        bool android = false;
        std::string error;
    } m_wizard;
    std::mutex m_dialogMutex;
    std::string m_dialogFolder;   // result of the native folder dialog
    int m_dialogTarget = 0;       // 1 = wizard location, 2 = open project

    // modals
    std::string m_saveAsName;
    bool m_runPendingAfterSaveAs = false;

    // smoke test
    int m_exitCode = 0;
    u64 m_frame = 0;
    std::vector<std::string> m_failures;
    ImVec2 m_smokeClick{};
    Uuid m_smokeDeleted;
};

} // namespace agr::editor
