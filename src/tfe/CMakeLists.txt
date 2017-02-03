set(CMAKE_INCLUDE_CURRENT_DIR ON)

target_compile_definitions(tfe PUBLIC HAVE_TFE)

add_library(tfe tfe.c tfearch.c tfesupp.c)
