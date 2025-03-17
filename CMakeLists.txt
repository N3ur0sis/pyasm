cmake_minimum_required(VERSION 3.22)

# Nom du projet
project(pyasm)

# Utilisation de C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options de compilation sécurisées
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Werror")

# Définition des dossiers sources et includes
set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")

# Recherche automatique des fichiers sources
file(GLOB_RECURSE SOURCES "${SRC_DIR}/*.cpp")
file(GLOB_RECURSE HEADERS "${INCLUDE_DIR}/*.h")

# Création de l'exécutable
add_executable(pyasm ${SOURCES} ${HEADERS})

# Ajout du dossier include
target_include_directories(pyasm PRIVATE ${INCLUDE_DIR})

# Gestion des warnings spécifiques au compilateur (optionnel)
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(pyasm PRIVATE -Wall -Wextra -Wpedantic -Werror)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(pyasm PRIVATE /W4 /WX)
endif()
# Spécifier le dossier de compilation des fichiers objets
set_target_properties(pyasm PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)