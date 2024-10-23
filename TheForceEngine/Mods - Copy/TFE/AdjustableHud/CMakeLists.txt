project(
	AdjustableHud
	HOMEPAGE_URL "https://theforceengine.github.io"
	DESCRIPTION "“The Force Engine’s” built‑in “AdjustableHud” mod"
)

add_custom_target(
	${PROJECT_NAME}
	""
	SOURCES
		AdjustableHud.txt
		Credits.txt
		HudStatusLeftAddon.bm
		HudStatusLeftAddon.png
		HudStatusRightAddon.bm
		HudStatusRightAddon.png
	BYPRODUCTS
		"${CMAKE_INSTALL_DATADIR}/Mods/TFE/${PROJECT_NAME}/AdjustableHud.txt"
		"${CMAKE_INSTALL_DATADIR}/Mods/TFE/${PROJECT_NAME}/Credits.txt"
		"${CMAKE_INSTALL_DATADIR}/Mods/TFE/${PROJECT_NAME}/HudStatusLeftAddon.bm"
		"${CMAKE_INSTALL_DATADIR}/Mods/TFE/${PROJECT_NAME}/HudStatusLeftAddon.png"
		"${CMAKE_INSTALL_DATADIR}/Mods/TFE/${PROJECT_NAME}/HudStatusRightAddon.bm"
		"${CMAKE_INSTALL_DATADIR}/Mods/TFE/${PROJECT_NAME}/HudStatusRightAddon.png"
)

install(
	DIRECTORY
		"${CMAKE_CURRENT_SOURCE_DIR}"
	DESTINATION
		"${CMAKE_INSTALL_DATADIR}/Mods/TFE"
	FILE_PERMISSIONS
		OWNER_READ OWNER_WRITE
		GROUP_READ
		WORLD_READ
	DIRECTORY_PERMISSIONS
		OWNER_READ OWNER_EXECUTE OWNER_WRITE
		GROUP_READ GROUP_EXECUTE GROUP_WRITE
		WORLD_READ WORLD_EXECUTE
	PATTERN
		CMakeLists.txt EXCLUDE
)
