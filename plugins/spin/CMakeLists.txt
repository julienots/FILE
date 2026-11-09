# Example dynamic plugin: adds a reflected "Spin" component and its system.
add_library(agr_plugin_spin MODULE SpinPlugin.cpp)
target_link_libraries(agr_plugin_spin PRIVATE agr::runtime agr::compiler_options)
set_target_properties(agr_plugin_spin PROPERTIES
    PREFIX "lib"
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    FOLDER "Plugins")
