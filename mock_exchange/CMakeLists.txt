project(MOCK_EXCHANGE)

add_executable(exchange
        main.cpp
        ../common/ITCH_Messages.hpp
        ../common/Types.hpp
        ../common/OUCH_Parser.hpp
        ../common/OUCH_Messages.hpp
)

target_include_directories(exchange PRIVATE ${CMAKE_SOURCE_DIR}/common)

target_link_libraries(exchange pthread)