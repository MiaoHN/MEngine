# cmake/CompilerWarnings.cmake
#
# Centralized, per-compiler warning flags shared by all MEngine targets.
# Keeping them in one place makes the toolchain story explicit and lets us
# adjust warnings per platform without duplicating logic in every target.

function(mengine_enable_warnings target)
  if(MSVC)
    # cl.exe and clang-cl share the MSVC flag syntax.
    target_compile_options(${target} PRIVATE /W4 /permissive- /Zc:__cplusplus)
  else()
    # GCC and Clang (including AppleClang) share the GCC-style flag syntax.
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endfunction()

# Quiet a few warnings that newer Clang enables *by default* and that vendored
# third-party libraries trip over. Apply to vendored targets only, never to
# MEngine's own targets (their warnings are meaningful).
function(mengine_quiet_third_party target)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(${target} PRIVATE -Wno-nontrivial-memcall)
  endif()
endfunction()
