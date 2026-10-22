# Sanitizer support.
#
# DUNGEON_SANITIZE accepts a comma separated list so that address and
# undefined behaviour sanitizers can be combined:
#
#   -DDUNGEON_SANITIZE=address
#   -DDUNGEON_SANITIZE=undefined
#   -DDUNGEON_SANITIZE=address,undefined
#
# Sanitizers must be applied to every target that is compiled *and* linked,
# otherwise the runtime is missing and the program aborts at startup.

function(dungeon_enable_sanitizers target)
    if(DUNGEON_SANITIZE STREQUAL "none")
        return()
    endif()

    string(REPLACE "," ";" sanitizer_list "${DUNGEON_SANITIZE}")
    set(sanitizer_flags "")

    foreach(sanitizer IN LISTS sanitizer_list)
        if(sanitizer STREQUAL "address")
            list(APPEND sanitizer_flags -fsanitize=address)
        elseif(sanitizer STREQUAL "undefined")
            # Abort on the first UB instead of continuing with a poisoned state.
            list(APPEND sanitizer_flags -fsanitize=undefined -fno-sanitize-recover=undefined)
        elseif(sanitizer STREQUAL "thread")
            list(APPEND sanitizer_flags -fsanitize=thread)
        else()
            message(FATAL_ERROR
                "Unknown sanitizer '${sanitizer}'. "
                "Valid values: none, address, undefined, address,undefined, thread.")
        endif()
    endforeach()

    # Frame pointers keep stack traces readable under the sanitizers.
    list(APPEND sanitizer_flags -fno-omit-frame-pointer -g)

    target_compile_options(${target} PRIVATE ${sanitizer_flags})
    target_link_options(${target} PRIVATE ${sanitizer_flags})
endfunction()
