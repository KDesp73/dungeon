# Compiler warning configuration shared by every target in the project.
#
# The warning set is intentionally strict: the codebase is new, so there is no
# legacy to grandfather in. Warnings are never suppressed silently - if a
# warning is too noisy for a legitimate pattern, the flag is removed here and
# the reason is recorded in docs/architecture.md.

function(dungeon_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
        if(DUNGEON_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
        return()
    endif()

    target_compile_options(${target} PRIVATE
        -Wall                     # generic
        -Wextra                   # extra checks beyond -Wall
        -Wpedantic                # reject non-standard constructs
        -Wshadow                  # shadowed variables
        -Wconversion              # implicit narrowing conversions
        -Wsign-conversion         # signed/unsigned mixing
        -Wcast-qual               # casts that drop const/volatile
        -Wdouble-promotion        # float -> double
        -Wformat=2                # printf style format checking
        -Wimplicit-fallthrough    # missing fallthrough markers
        -Wnull-dereference        # null pointer dereferences
        -Wold-style-cast          # C-style casts
        -Woverloaded-virtual      # virtual functions hidden by overloads
        -Wredundant-move          # needless std::move
        -Wundef                   # undefined macros evaluate to 0
        -Wunused                  # unused entities
    )

    if(DUNGEON_WARNINGS_AS_ERRORS)
        target_compile_options(${target} PRIVATE -Werror)
    endif()
endfunction()
