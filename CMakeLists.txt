cmake_minimum_required(VERSION 3.16)
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()
project(7gui523 CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(CTest)

if(MSVC)
    add_compile_options(/utf-8)
endif()

option(BUILD_GUI "Build the GUI game application" ON)

set(QT_COMPONENTS)
if(BUILD_GUI)
    find_package(Qt6 COMPONENTS Widgets Multimedia)
    if(Qt6_FOUND)
        list(APPEND QT_COMPONENTS Qt6::Widgets Qt6::Multimedia)
    else()
        message(WARNING "Qt6 not found, skipping GUI targets")
        set(BUILD_GUI OFF)
    endif()
endif()

file(GLOB_RECURSE CORE_SOURCES "core/*.cpp")
file(GLOB_RECURSE GAME_SOURCES "game/*.cpp")
file(GLOB_RECURSE AI_SOURCES "ai/*.cpp")
file(GLOB_RECURSE UI_SOURCES "ui/*.cpp")

# ── Object Libraries for faster incremental builds ──
add_library(core_obj OBJECT ${CORE_SOURCES})
target_include_directories(core_obj PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_library(game_obj OBJECT ${GAME_SOURCES})
target_include_directories(game_obj PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB_RECURSE AI_SOURCES "ai/*.cpp")
list(FILTER AI_SOURCES EXCLUDE REGEX ".*/ai/cfr/.*")
add_library(ai_obj OBJECT ${AI_SOURCES})
target_include_directories(ai_obj PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB_RECURSE CFR_SOURCES "ai/cfr/*.cpp")
list(FILTER CFR_SOURCES EXCLUDE REGEX ".*/tests/.*")
add_library(cfr_obj OBJECT ${CFR_SOURCES})
target_include_directories(cfr_obj PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(BUILD_GUI)
    add_executable(game
        $<TARGET_OBJECTS:core_obj>
        $<TARGET_OBJECTS:game_obj>
        $<TARGET_OBJECTS:ai_obj>
        ${UI_SOURCES}
        main.cpp
    )

    set_target_properties(game PROPERTIES AUTOMOC ON)

    target_include_directories(game PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(game PRIVATE Qt6::Widgets Qt6::Multimedia)

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/music")
        add_custom_command(TARGET game POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/music"
                "$<TARGET_FILE_DIR:game>/music"
        )
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/kenney_playing-cards-pack/PNG/Cards (large)")
        add_custom_command(TARGET game POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/kenney_playing-cards-pack/PNG/Cards (large)"
                "$<TARGET_FILE_DIR:game>/cards"
        )
    endif()
endif()


add_executable(ai_battle
    $<TARGET_OBJECTS:core_obj>
    $<TARGET_OBJECTS:game_obj>
    $<TARGET_OBJECTS:ai_obj>
    tests/ai_battle.cpp
)
target_include_directories(ai_battle PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(data_collector
    $<TARGET_OBJECTS:core_obj>
    $<TARGET_OBJECTS:game_obj>
    $<TARGET_OBJECTS:ai_obj>
    tests/data_collector.cpp
)
target_include_directories(data_collector PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(debug_one
    $<TARGET_OBJECTS:core_obj>
    $<TARGET_OBJECTS:game_obj>
    $<TARGET_OBJECTS:ai_obj>
    tests/debug_one_game.cpp
)
target_include_directories(debug_one PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(unit_tests
    $<TARGET_OBJECTS:core_obj>
    $<TARGET_OBJECTS:game_obj>
    $<TARGET_OBJECTS:ai_obj>
    tests/test_main.cpp
    tests/test_cardtype.cpp
    tests/test_score.cpp
    tests/test_special.cpp
    tests/test_deck.cpp
    tests/test_player.cpp
    tests/test_search_p0.cpp
    tests/test_rules_v2.cpp
    tests/test_fair_ai.cpp
    tests/test_campaign.cpp
    tests/test_tracker_dual_side.cpp
    tests/test_variant_registry.cpp
)
target_include_directories(unit_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
# These tests use assert(), including in Release builds.
if(MSVC)
    target_compile_options(unit_tests PRIVATE /UNDEBUG)
else()
    target_compile_options(unit_tests PRIVATE -UNDEBUG)
endif()
if(BUILD_TESTING)
    add_test(NAME core_rules COMMAND unit_tests)
endif()

add_executable(genetic_tuner
    $<TARGET_OBJECTS:core_obj>
    $<TARGET_OBJECTS:game_obj>
    $<TARGET_OBJECTS:ai_obj>
    tests/genetic_tuner.cpp
)
target_include_directories(genetic_tuner PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(test_sampler
    $<TARGET_OBJECTS:core_obj>
    $<TARGET_OBJECTS:game_obj>
    $<TARGET_OBJECTS:ai_obj>
    tests/test_sampler.cpp
)
target_include_directories(test_sampler PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(test_sampler_battle
    $<TARGET_OBJECTS:core_obj>
    $<TARGET_OBJECTS:game_obj>
    $<TARGET_OBJECTS:ai_obj>
    tests/test_sampler_battle.cpp
)
target_include_directories(test_sampler_battle PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(test_sampler_sweep
    $<TARGET_OBJECTS:core_obj>
    $<TARGET_OBJECTS:game_obj>
    $<TARGET_OBJECTS:ai_obj>
    tests/test_sampler_sweep.cpp
)
target_include_directories(test_sampler_sweep PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(test_sampler_topk
    $<TARGET_OBJECTS:core_obj>
    $<TARGET_OBJECTS:game_obj>
    $<TARGET_OBJECTS:ai_obj>
    tests/test_sampler_topk.cpp
)
target_include_directories(test_sampler_topk PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(test_bayesian
    $<TARGET_OBJECTS:core_obj>
    $<TARGET_OBJECTS:game_obj>
    $<TARGET_OBJECTS:ai_obj>
    tests/test_bayesian.cpp
)
target_include_directories(test_bayesian PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(test_bayesian_battle
    $<TARGET_OBJECTS:core_obj>
    $<TARGET_OBJECTS:game_obj>
    $<TARGET_OBJECTS:ai_obj>
    tests/test_bayesian_battle.cpp
)
target_include_directories(test_bayesian_battle PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(loss_analysis
    $<TARGET_OBJECTS:core_obj>
    $<TARGET_OBJECTS:game_obj>
    $<TARGET_OBJECTS:ai_obj>
    tests/loss_analysis.cpp
)
target_include_directories(loss_analysis PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(env_server
    $<TARGET_OBJECTS:core_obj>
    $<TARGET_OBJECTS:game_obj>
    $<TARGET_OBJECTS:ai_obj>
    tests/env_server.cpp
)
target_include_directories(env_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(test_battle_fix $<TARGET_OBJECTS:core_obj> $<TARGET_OBJECTS:game_obj> $<TARGET_OBJECTS:ai_obj> tests/test_battle_fix.cpp)
target_include_directories(test_battle_fix PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(test_diag $<TARGET_OBJECTS:core_obj> $<TARGET_OBJECTS:game_obj> $<TARGET_OBJECTS:ai_obj> tests/test_diag.cpp)
target_include_directories(test_diag PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(test_fidelity $<TARGET_OBJECTS:core_obj> $<TARGET_OBJECTS:game_obj> $<TARGET_OBJECTS:ai_obj> tests/test_fidelity.cpp)
target_include_directories(test_fidelity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(acceptance $<TARGET_OBJECTS:core_obj> $<TARGET_OBJECTS:game_obj> $<TARGET_OBJECTS:ai_obj> tests/acceptance.cpp)
target_include_directories(acceptance PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(fair_benchmark $<TARGET_OBJECTS:core_obj> $<TARGET_OBJECTS:game_obj> $<TARGET_OBJECTS:ai_obj> tests/fair_benchmark.cpp)
target_include_directories(fair_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(all_vs_all $<TARGET_OBJECTS:core_obj> $<TARGET_OBJECTS:game_obj> $<TARGET_OBJECTS:ai_obj> $<TARGET_OBJECTS:cfr_obj> tests/all_vs_all.cpp)
target_include_directories(all_vs_all PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(debug_l6 $<TARGET_OBJECTS:core_obj> $<TARGET_OBJECTS:game_obj> $<TARGET_OBJECTS:ai_obj> tests/debug_l6.cpp)
target_include_directories(debug_l6 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(debug_l6b $<TARGET_OBJECTS:core_obj> $<TARGET_OBJECTS:game_obj> $<TARGET_OBJECTS:ai_obj> tests/debug_l6b.cpp)
target_include_directories(debug_l6b PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(debug_l6d $<TARGET_OBJECTS:core_obj> $<TARGET_OBJECTS:game_obj> $<TARGET_OBJECTS:ai_obj> tests/debug_l6d.cpp)
target_include_directories(debug_l6d PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(sim_campaign
    $<TARGET_OBJECTS:core_obj>
    $<TARGET_OBJECTS:game_obj>
    $<TARGET_OBJECTS:ai_obj>
    tests/sim_campaign.cpp
)
target_include_directories(sim_campaign PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(test_cfr
    $<TARGET_OBJECTS:core_obj>
    $<TARGET_OBJECTS:game_obj>
    $<TARGET_OBJECTS:ai_obj>
    $<TARGET_OBJECTS:cfr_obj>
    ai/cfr/tests/test_cfr.cpp
)
target_include_directories(test_cfr PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(MSVC)
    target_compile_options(test_cfr PRIVATE /UNDEBUG)
else()
    target_compile_options(test_cfr PRIVATE -UNDEBUG)
endif()

add_executable(cfr_train
    $<TARGET_OBJECTS:core_obj>
    $<TARGET_OBJECTS:game_obj>
    $<TARGET_OBJECTS:ai_obj>
    $<TARGET_OBJECTS:cfr_obj>
    ai/cfr/tests/cfr_train_main.cpp
)
target_include_directories(cfr_train PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(cfr_battle
    $<TARGET_OBJECTS:core_obj>
    $<TARGET_OBJECTS:game_obj>
    $<TARGET_OBJECTS:ai_obj>
    $<TARGET_OBJECTS:cfr_obj>
    ai/cfr/tests/cfr_battle.cpp
)
target_include_directories(cfr_battle PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})