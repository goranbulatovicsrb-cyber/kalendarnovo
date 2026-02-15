cmake_minimum_required(VERSION 3.15)
project(SmjeneKalendar LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static link CRT for standalone exe (no VC++ Redistributable needed)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/utf-8)
endif()

if(MINGW)
    add_link_options(-static -static-libgcc -static-libstdc++)
endif()

add_executable(SmjeneKalendar WIN32 src/main.cpp)

target_compile_definitions(SmjeneKalendar PRIVATE UNICODE _UNICODE)

if(MSVC)
    target_link_libraries(SmjeneKalendar
        gdi32 gdiplus user32 shell32 comctl32 kernel32
    )
elseif(MINGW)
    target_link_libraries(SmjeneKalendar
        gdi32 gdiplus user32 shell32 comctl32 kernel32 ole32
        -mwindows
    )
endif()
