# MIT License
#
# Copyright (c) 2026 FamilyPDF Contributors

add_library(OfficeExportPlugin SHARED
    officeexportplugin.cpp
)

target_link_libraries(OfficeExportPlugin PRIVATE Pdf4QtLibCore Pdf4QtLibWidgets Qt6::Core Qt6::Widgets)

set_target_properties(OfficeExportPlugin PROPERTIES
                      VERSION ${PDF4QT_VERSION}
                      SOVERSION ${PDF4QT_VERSION}
                      LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_PLUGINS_DIR}
                      RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_PLUGINS_DIR})

install(TARGETS OfficeExportPlugin RUNTIME DESTINATION ${PDF4QT_PLUGINS_DIR} LIBRARY DESTINATION ${PDF4QT_PLUGINS_DIR})
