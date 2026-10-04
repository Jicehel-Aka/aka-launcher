cmake_minimum_required(VERSION 3.16)

# Bibliotheque Gamebuino-AKA : placer (ou lier) le composant "gamebuino" dans
# ./components/gamebuino, ou definir AKA_COMPONENTS=/chemin/vers/components
# (dossier qui contient gamebuino/).
if(DEFINED ENV{AKA_COMPONENTS})
    set(EXTRA_COMPONENT_DIRS "$ENV{AKA_COMPONENTS}/gamebuino")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(aka_launcher)
