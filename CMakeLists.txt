cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
project(duc VERSION 1.4.5)

set(CMAKE_C_STANDARD 11)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_definitions(PACKAGE_VERSION="${CMAKE_PROJECT_VERSION}")
add_compile_definitions(_GNU_SOURCE)


option(ENABLE_CAIRO "Enable cairo drawing" ON)
option(ENABLE_OPENGL "Enable OpenGL drawing" OFF)
option(ENABLE_UI "Enable ncurses ui" ON)
option(ENABLE_X11 "Enable X11" ON)


if (NOT TARGET _duc_lib_stub)
    add_library(_duc_lib_stub INTERFACE)
    add_library(duc::LinkedLibs ALIAS _duc_lib_stub)
endif ()

include(FindPkgConfig)

if (ENABLE_CAIRO)
    pkg_check_modules(cairo_libs REQUIRED IMPORTED_TARGET cairo pango pangocairo)
    if (cairo_libs_FOUND)
        add_compile_definitions(ENABLE_CAIRO)
        target_link_libraries(_duc_lib_stub INTERFACE PkgConfig::cairo_libs)
    else ()
        message(FATAL_ERROR "The cairo libraries(cairo, pango, pangocairo) were not found, which is needed for graph support. Either install the cairo development libraries, or compile without graph support (-DENABLE_CAIRO=OFF).")
    endif ()

endif ()


if (ENABLE_OPENGL)
    if (ENABLE_X11)
        message(FATAL_ERROR "Only one graphical user interface can be configured, use -DENABLE_X11=OFF when using -DENABLE_OPENGL=ON")
    endif ()
    find_package(glfw3 REQUIRED)
    add_compile_definitions(ENABLE_OPENGL)
    target_link_libraries(_duc_lib_stub INTERFACE glfw)
endif ()


if (ENABLE_UI)
    pkg_check_modules(ncursesw QUIET IMPORTED_TARGET ncursesw)
    if (ncursesw_FOUND)
        add_compile_definitions(ENABLE_UI)
        add_compile_definitions(HAVE_LIBNCURSESW)
        add_compile_definitions(HAVE_NCURSES_H) # Bypass ncurses include dir checking as we already put that into the include path when linking the lib
        target_link_libraries(_duc_lib_stub INTERFACE PkgConfig::ncursesw)
    else ()
        pkg_check_modules(ncurses QUIET IMPORTED_TARGET ncurses)
        if (ncurses_FOUND)
            add_compile_definitions(ENABLE_UI)
            add_compile_definitions(HAVE_LIBNCURSES)
            include(CheckIncludeFiles)
            check_include_files("ncurses/ncurses.h" HAVE_NCURSES_NCURSES_H)
            if (HAVE_NCURSES_NCURSES_H)
                add_compile_definitions(HAVE_NCURSES_NCURSES_H)
            endif ()
            target_link_libraries(_duc_lib_stub INTERFACE PkgConfig::ncurses)
        else ()
            message(FATAL_ERROR "The ncurses library was not found, which is needed for ui support. Either install the ncurses development libraries, or compile without ui support (-DENABLE_UI=OFF)")
        endif ()
    endif ()
endif ()


if (ENABLE_X11)
    find_package(X11)
    if (X11_FOUND)
        add_compile_definitions(ENABLE_X11)
        target_link_libraries(_duc_lib_stub INTERFACE X11)
    else ()
        message(FATAL_ERROR "The X11 library was not found, which is needed for x11 gui support.")
    endif ()
endif ()


if (NOT DEFINED WITH_DB_BACKEND)
    set(WITH_DB_BACKEND tokyocabinet)
endif ()

if (WITH_DB_BACKEND STREQUAL tokyocabinet)
    pkg_check_modules(tokyocabinet REQUIRED IMPORTED_TARGET tokyocabinet)
    set(DB_BACKEND_LIB PkgConfig::tokyocabinet)
    add_compile_definitions(ENABLE_TOKYOCABINET)
elseif (WITH_DB_BACKEND STREQUAL leveldb)
    find_package(leveldb REQUIRED)
    find_package(Threads REQUIRED)
    set(DB_BACKEND_LIB leveldb::leveldb)
    add_compile_definitions(ENABLE_LEVELDB)
elseif (WITH_DB_BACKEND STREQUAL sqlite3)
    find_package(SQLite3 REQUIRED)
    set(DB_BACKEND_LIB SQLite::SQLite3)
    add_compile_definitions(ENABLE_SQLITE)
elseif (WITH_DB_BACKEND STREQUAL lmdb)
    pkg_check_modules(lmdb REQUIRED IMPORTED_TARGET lmdb)
    set(DB_BACKEND_LIB PkgConfig::lmdb)
    add_compile_definitions(ENABLE_LMDB)
elseif (WITH_DB_BACKEND STREQUAL kyotocabinet)
    pkg_check_modules(kyotocabinet REQUIRED IMPORTED_TARGET kyotocabinet)
    set(DB_BACKEND_LIB PkgConfig::kyotocabinet)
    add_compile_definitions(ENABLE_KYOTOCABINET)
else ()
    message(FATAL_ERROR "Unsupported db-backend")
endif ()
add_compile_definitions(DB_BACKEND="${WITH_DB_BACKEND}")
target_link_libraries(_duc_lib_stub INTERFACE ${DB_BACKEND_LIB})


set(LIB_DUC_SOURCE
        src/libduc/buffer.c
        src/libduc/buffer.h
        src/libduc/db.c
        src/libduc/db.h
        src/libduc/db-tokyo.c
        src/libduc/db-kyoto.c
        src/libduc/db-leveldb.c
        src/libduc/db-sqlite3.c
        src/libduc/db-lmdb.c
        src/libduc/dir.c
        src/libduc/duc.c
        src/libduc/duc.h
        src/libduc/index.c
        src/libduc/private.h
        src/libduc/canonicalize.c
        src/libduc/varint.c
        src/libduc/varint.h
        src/libduc/uthash.h
        src/libduc/utlist.h
        src/libduc/utstring.h
        )

set(DUC_GLAD_SOURCE
        src/glad/glad.c
        src/glad/KHR/khrplatform.h
        src/glad/glad/glad.h
        )

set(LIB_DUC_GRAPH_SOURCE
        src/libduc-graph/graph.c
        src/libduc-graph/graph-cairo.c
        src/libduc-graph/graph-opengl.c
        src/libduc-graph/graph-svg.c
        src/libduc-graph/graph-html.c
        src/libduc-graph/graph-private.h
        src/libduc-graph/duc-graph.h
        )

set(DUC_SOURCE
        src/duc/cmd-cgi.c
        src/duc/cmd-graph.c
        src/duc/cmd-gui.c
        src/duc/cmd-guigl.c
        src/duc/cmd.h
        src/duc/cmd.h
        src/duc/cmd-index.c
        src/duc/cmd-info.c
        src/duc/cmd-ls.c
        src/duc/cmd-ui.c
        src/duc/cmd-xml.c
        src/duc/cmd-json.c
        src/duc/ducrc.c
        src/duc/ducrc.h
        src/duc/main.c
        )


file(WRITE ${CMAKE_BINARY_DIR}/config.h)        # Empty. Make include happy

include_directories(src/libduc src/libduc-graph src/glad)

add_executable(duc ${LIB_DUC_SOURCE} ${DUC_GLAD_SOURCE} ${LIB_DUC_GRAPH_SOURCE} ${DUC_SOURCE})
target_link_libraries(duc PRIVATE dl m duc::LinkedLibs)
