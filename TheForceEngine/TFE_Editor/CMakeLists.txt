add_subdirectory(AssetBrowser/)
add_subdirectory(EditorAsset/)
file(GLOB SOURCES "*.cpp")
target_sources(tfe PRIVATE ${SOURCES})
