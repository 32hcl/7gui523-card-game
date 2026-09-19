cmake_minimum_required(VERSION 3.16)
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
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
file(GLOB_RECURSE DATA_SOURCES "data/*.cpp")
file(GLOB_RECURSE DIALOGUE_SOURCES "dialogue/*.cpp")
file(GLOB_RECURSE SHOP_SOURCES "shop/*.cpp")
file(GLOB_RECURSE UI_SOURCES "ui/*.cpp")

if(BUILD_GUI)
    add_executable(game
        ${CORE_SOURCES}
        ${GAME_SOURCES}
        ${AI_SOURCES}
        ${DATA_SOURCES}
        ${DIALOGUE_SOURCES}
        ${SHOP_SOURCES}
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

    if(WIN32 AND Qt6_FOUND)
        add_custom_command(TARGET game POST_BUILD
            COMMAND "${Qt6_DIR}/../../../bin/windeployqt.exe" "$<TARGET_FILE:game>"
        )
    endif()
endif()


add_executable(ai_battle
    ${CORE_SOURCES}
    ${GAME_SOURCES}
    ${AI_SOURCES}
    tests/ai_battle.cpp
)
target_include_directories(ai_battle PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(data_collector
    ${CORE_SOURCES}
    ${GAME_SOURCES}
    ${AI_SOURCES}
    tests/data_collector.cpp
)
target_include_directories(data_collector PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(debug_one
    ${CORE_SOURCES}
    ${GAME_SOURCES}
    ${AI_SOURCES}
    tests/debug_one_game.cpp
)
target_include_directories(debug_one PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(unit_tests
    ${CORE_SOURCES}
    ${GAME_SOURCES}
    ${AI_SOURCES}
    tests/test_main.cpp
    tests/test_cardtype.cpp
    tests/test_score.cpp
    tests/test_special.cpp
    tests/test_deck.cpp
    tests/test_player.cpp
    tests/test_search_p0.cpp
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
    ${CORE_SOURCES}
    ${GAME_SOURCES}
    ${AI_SOURCES}
    tests/genetic_tuner.cpp
)
target_include_directories(genetic_tuner PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(test_sampler
    ${CORE_SOURCES}
    ${GAME_SOURCES}
    ${AI_SOURCES}
    tests/test_sampler.cpp
)
target_include_directories(test_sampler PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(test_sampler_battle
    ${CORE_SOURCES}
    ${GAME_SOURCES}
    ${AI_SOURCES}
    tests/test_sampler_battle.cpp
)
target_include_directories(test_sampler_battle PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(test_sampler_sweep
    ${CORE_SOURCES}
    ${GAME_SOURCES}
    ${AI_SOURCES}
    tests/test_sampler_sweep.cpp
)
target_include_directories(test_sampler_sweep PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(test_sampler_topk
    ${CORE_SOURCES}
    ${GAME_SOURCES}
    ${AI_SOURCES}
    tests/test_sampler_topk.cpp
)
target_include_directories(test_sampler_topk PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(test_bayesian
    ${CORE_SOURCES}
    ${GAME_SOURCES}
    ${AI_SOURCES}
    tests/test_bayesian.cpp
)
target_include_directories(test_bayesian PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(test_bayesian_battle
    ${CORE_SOURCES}
    ${GAME_SOURCES}
    ${AI_SOURCES}
    tests/test_bayesian_battle.cpp
)
target_include_directories(test_bayesian_battle PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(loss_analysis
    ${CORE_SOURCES}
    ${GAME_SOURCES}
    ${AI_SOURCES}
    tests/loss_analysis.cpp
)
target_include_directories(loss_analysis PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(env_server
    ${CORE_SOURCES}
    ${GAME_SOURCES}
    ${AI_SOURCES}
    tests/env_server.cpp
)
target_include_directories(env_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})