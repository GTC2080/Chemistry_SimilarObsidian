# Reason: This file centralizes compiler warning defaults without mixing them into project logic.

if(MSVC)
  add_compile_options(/W4 /permissive-)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()
