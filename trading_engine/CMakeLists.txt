project(TradingEngine)

# We will add files here as we create them
add_executable(engine
        main.cpp
        Order_Book.cpp
        Order_Book.hpp
        Parser.hpp
        ../common/ITCH_Messages.hpp
        ../common/Types.hpp
        OUCH_Parser.hpp
        ../common/OUCH_Messages.hpp
        ../common/Logger.hpp
)

# Crucial: This allows you to write #include "ITCH_Messages.hpp" in your .cpp files
# instead of #include "../common/ITCH_Messages.hpp", which is brittle.
target_include_directories(engine PRIVATE ${CMAKE_SOURCE_DIR}/common)

target_link_libraries(engine pthread)