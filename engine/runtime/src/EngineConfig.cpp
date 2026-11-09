#include "agr/runtime/EngineConfig.hpp"

#include <nlohmann/json.hpp>

namespace agr {

using nlohmann::json;

Result<EngineConfig> EngineConfig::fromJson(std::string_view text) {
    json j = json::parse(text, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return makeError(Error::Code::ParseError, "engine config is not a JSON object");
    EngineConfig c;
    try {
        c.appName = j.value("appName", c.appName);
        c.organization = j.value("organization", c.organization);
        c.headless = j.value("headless", c.headless);
        c.fixedTimestep = j.value("fixedTimestep", c.fixedTimestep);
        c.maxFixedStepsPerFrame = j.value("maxFixedStepsPerFrame", c.maxFixedStepsPerFrame);
        c.maxFrameDelta = j.value("maxFrameDelta", c.maxFrameDelta);
        c.targetFps = j.value("targetFps", c.targetFps);
        c.workerThreads = j.value("workerThreads", c.workerThreads);
        c.maxFrames = j.value("maxFrames", c.maxFrames);
        c.assetsDirectory = j.value("assetsDirectory", c.assetsDirectory);
        c.userDirectory = j.value("userDirectory", c.userDirectory);
        c.pluginsDirectory = j.value("pluginsDirectory", c.pluginsDirectory);
        c.logFile = j.value("logFile", c.logFile);
        if (j.contains("window")) {
            const json& w = j["window"];
            c.window.title = w.value("title", c.window.title);
            c.window.width = w.value("width", c.window.width);
            c.window.height = w.value("height", c.window.height);
            c.window.resizable = w.value("resizable", c.window.resizable);
            c.window.fullscreen = w.value("fullscreen", c.window.fullscreen);
            c.window.vsync = w.value("vsync", c.window.vsync);
            c.window.highDpi = w.value("highDpi", c.window.highDpi);
            const std::string api = w.value("graphics", std::string("opengl"));
            if (api == "opengl") c.window.graphics = GraphicsApi::OpenGL;
            else if (api == "none") c.window.graphics = GraphicsApi::None;
            else return makeError(Error::Code::InvalidArgument, "unknown graphics api '" + api + "'");
        }
    } catch (const json::exception& e) {
        return makeError(Error::Code::ParseError, std::string("engine config: ") + e.what());
    }
    if (c.fixedTimestep <= 0.0 || c.fixedTimestep > 1.0)
        return makeError(Error::Code::InvalidArgument, "fixedTimestep must be in (0, 1]");
    if (c.window.width <= 0 || c.window.height <= 0)
        return makeError(Error::Code::InvalidArgument, "window size must be positive");
    return c;
}

std::string EngineConfig::toJson() const {
    json j;
    j["appName"] = appName;
    j["organization"] = organization;
    j["headless"] = headless;
    j["fixedTimestep"] = fixedTimestep;
    j["maxFixedStepsPerFrame"] = maxFixedStepsPerFrame;
    j["maxFrameDelta"] = maxFrameDelta;
    j["targetFps"] = targetFps;
    j["workerThreads"] = workerThreads;
    j["maxFrames"] = maxFrames;
    j["assetsDirectory"] = assetsDirectory;
    j["userDirectory"] = userDirectory;
    j["pluginsDirectory"] = pluginsDirectory;
    j["logFile"] = logFile;
    j["window"] = {{"title", window.title},         {"width", window.width},   {"height", window.height},
                   {"resizable", window.resizable}, {"fullscreen", window.fullscreen},
                   {"vsync", window.vsync},         {"highDpi", window.highDpi},
                   {"graphics", window.graphics == GraphicsApi::OpenGL ? "opengl" : "none"}};
    return j.dump(2);
}

} // namespace agr
