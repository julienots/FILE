# Creates a project from the 2D showcase template and exports it for the desktop with the
# editor's command line (used by the agr_export_desktop test). Only OUT/ is written; the
# previous project there (created by this test) is replaced.
file(REMOVE_RECURSE ${OUT}/Showcase)
execute_process(COMMAND ${EDITOR} --create-project ${OUT}/Showcase --template showcase2d
                        --export desktop --output ${OUT}/out
                RESULT_VARIABLE code)
if(NOT code EQUAL 0)
    message(FATAL_ERROR "export failed (${code})")
endif()
foreach(f out/Showcase/game/game.json out/Showcase/game/scenes/main.agrscene out/Showcase/game/assets/scripts/platformer_hero.lua)
    if(NOT EXISTS ${OUT}/${f})
        message(FATAL_ERROR "missing ${f}")
    endif()
endforeach()
