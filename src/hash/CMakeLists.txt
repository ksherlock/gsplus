set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_library(hash md2.c md4.c md5.c sha1.c sha3.c sha3_test.c blake2s.c blake2b.c)
