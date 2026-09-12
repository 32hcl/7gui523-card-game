cmake_minimum_required(VERSION 3.16)
project(7gui523 CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia)

file(GLOB CORE_SOURCES "core/*.cpp")
file(GLOB UI_SOURCES "ui/*.cpp")

add_executable(game
    ${CORE_SOURCES}
    ${UI_SOURCES}
    main.cpp
)

target_include_directories(game PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(game PRIVATE Qt6::Widgets Qt6::Multimedia)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/music")
    add_custom_command(TARGET game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/music"
            "$<TARGET_FILE_DIR:game>/music"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/kenney_playing-cards-pack/PNG/Cards (medium)")
    add_custom_command(TARGET game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/kenney_playing-cards-pack/PNG/Cards (medium)"
            "$<TARGET_FILE_DIR:game>/cards"
    )
endif()


add_executable(ai_battle
    ${CORE_SOURCES}
    tests/ai_battle.cpp
)
target_include_directories(ai_battle PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(data_collector
    ${CORE_SOURCES}
    tests/data_collector.cpp
)
target_include_directories(data_collector PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(debug_one
    ${CORE_SOURCES}
    tests/debug_one_game.cpp
)
target_include_directories(debug_one PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(unit_tests
    ${CORE_SOURCES}
    tests/test_main.cpp
    tests/test_cardtype.cpp
    tests/test_score.cpp
    tests/test_special.cpp
    tests/test_deck.cpp
    tests/test_player.cpp
)
target_include_directories(unit_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})