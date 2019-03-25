PROJECT(qrvthtool)

# Main binary directory. Needed for git_version.h
INCLUDE_DIRECTORIES("${CMAKE_BINARY_DIR}")

# Find Qt5.
# FIXME: Figure out a minimum Qt5 version.
# QtWin::fromHICON was added in Qt 5.2.
SET(Qt5_NO_LINK_QTMAIN 1)
FIND_PACKAGE(Qt5 5.2.0 COMPONENTS Core Gui Widgets)
IF(Qt5_FOUND AND Qt5Core_FOUND AND Qt5Gui_FOUND AND Qt5Widgets_FOUND)
	# Found Qt5.
	SET(BUILD_QT5 ON)
	IF(ENABLE_DBUS)
		# QtDBus is used for DockManager.
		FIND_PACKAGE(Qt5DBus)
		SET(HAVE_Qt5DBus ${Qt5DBus_FOUND})
	ENDIF(ENABLE_DBUS)
ELSE()
	# Did not find Qt5.
	MESSAGE(WARNING "Qt5 not found. Not building qrvthtool.")
ENDIF()

IF(BUILD_QT5)

# Create the Win32 manifest file.
IF(WIN32)
	CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/resources/win32/qrvthtool.exe.manifest.in" "${CMAKE_CURRENT_BINARY_DIR}/resources/win32/qrvthtool.exe.manifest" @ONLY)
ENDIF(WIN32)

# Translations directory.
INCLUDE(DirInstallPaths)
IF(WIN32 OR APPLE)
        # Win32 and Mac OS X don't use `make install`.
        SET(QRVTHTOOL_TRANSLATIONS_DIRECTORY "")
ELSE()
        # Unix system.
        SET(QRVTHTOOL_TRANSLATIONS_DIRECTORY "${CMAKE_INSTALL_PREFIX}/${DIR_INSTALL_TRANSLATIONS}")
ENDIF()

# KDE5: Check for KWidgetsAddons headers for notification sounds.
IF(UNIX AND NOT APPLE)
	# Reference: http://www.proli.net/2014/06/21/porting-your-project-to-qt5kf5/
	# Find KDE5 Extra CMake Modules.
	FIND_PACKAGE(ECM ${REQUIRE_KDE5} 0.0.11 NO_MODULE)
	IF(ECM_MODULE_PATH AND ECM_KDE_MODULE_DIR)
		# Make sure ECM's CMake files don't create an uninstall rule.
		SET(KDE_SKIP_UNINSTALL_TARGET TRUE)

		# Don't add KDE tests to the CTest build.
		SET(KDE_SKIP_TEST_SETTINGS TRUE)

		# Include KDE5 CMake modules.
		LIST(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR})
		# NOTE: The following are not needed for rvthtool and cause
		# duplicate moc generation, but we're leaving it here for reference.
		#INCLUDE(KDEInstallDirs)
		#INCLUDE(KDECMakeSettings)
		#INCLUDE(KDECompilerSettings)

		# Get rid of the explicit C90 setting.
		# NOTE: Not needed because KDECompilerSettings isn't used.
		#STRING(REPLACE "-std=iso9899:1990" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")

		# Find KDE5. (TODO: Version?)
		FIND_PACKAGE(KF5 COMPONENTS WidgetsAddons)
		IF(KF5WidgetsAddons_FOUND)
			SET(HAVE_KF5WIDGETSADDONS 1)

			# KDE code uses pthread_once().
			FIND_PACKAGE(Threads REQUIRED)
		ENDIF(KF5WidgetsAddons_FOUND)
	ENDIF()
ENDIF(UNIX AND NOT APPLE)


# Write the config.h file.
CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/config.qrvthtool.h.in" "${CMAKE_CURRENT_BINARY_DIR}/config.qrvthtool.h")

# Sources.
SET(qrvthtool_SRCS
	qrvthtool.cpp
	RvtHModel.cpp
	RvtHSortFilterProxyModel.cpp
	TranslationManager.cpp
	WorkerObject.cpp
	MessageSound.cpp

	widgets/BankEntryView.cpp
	widgets/LanguageMenu.cpp
	widgets/MessageWidget.cpp
	widgets/MessageWidgetStack.cpp
	widgets/QTreeViewOpt.cpp
	widgets/QItemView_Text.cpp

	windows/QRvtHToolWindow.cpp
	windows/SelectDeviceDialog.cpp
	)

# Headers with Qt objects.
SET(qrvthtool_MOC_H
	RvtHModel.hpp
	RvtHSortFilterProxyModel.hpp
	TranslationManager.hpp
	WorkerObject.hpp
	MessageSound.hpp

	widgets/BankEntryView.hpp
	widgets/LanguageMenu.hpp
	widgets/MessageWidget.hpp
	widgets/MessageWidgetStack.hpp
	widgets/QTreeViewOpt.hpp
	widgets/QItemView_Text.hpp

	windows/QRvtHToolWindow.hpp
	windows/SelectDeviceDialog.hpp
	)
QT5_WRAP_CPP(qrvthtool_MOC_SRCS ${qrvthtool_MOC_H})

# UI files.
SET(qrvthtool_UIS
	widgets/BankEntryView.ui

	windows/QRvtHToolWindow.ui
	windows/SelectDeviceDialog.ui
	)
QT5_WRAP_UI(qrvthtool_UIS_H ${qrvthtool_UIS})

# TaskbarButtonManager
SET(qrvthtool_TBM_SRCS
	TaskbarButtonManager/TaskbarButtonManager.cpp
	TaskbarButtonManager/TaskbarButtonManagerFactory.cpp
	)
SET(qrvthtool_TBM_MOC_H
	TaskbarButtonManager/TaskbarButtonManager.hpp
	)
SET(qrvthtool_TBM_H TaskbarButtonManager/TaskbarButtonManager_p.hpp)

IF(WIN32)
	# Windows: Use ITaskbarList3. (Win7+)
	SET(qrvthtool_TBM_SRCS
		${qrvthtool_TBM_SRCS}
		TaskbarButtonManager/Win7TaskbarList.cpp
		)
	SET(qrvthtool_TBM_MOC_H
		${qrvthtool_TBM_MOC_H}
		TaskbarButtonManager/Win7TaskbarList.hpp
		)
ELSEIF(UNIX AND NOT APPLE)
	IF(HAVE_Qt5DBus)
		# DockManager D-Bus API
		SET(DBus_XML_SRCS
			dbus/net.launchpad.DockItem.xml
			dbus/net.launchpad.DockManager.xml
			)
		SET_SOURCE_FILES_PROPERTIES(${DBus_XML_SRCS} PROPERTIES
			INCLUDE dbus/DBusMetatypes.hpp
			)

		# Enable DockManager / Unity API support.
		# NOTE: The D-Bus interfaces are generated in ${CMAKE_CURRENT_BINARY_DIR},
		# *not* ${CMAKE_CURRENT_BINARY_DIR}/dbus/.
		QT5_ADD_DBUS_INTERFACES(
			qrvthtool_DBUS_INTERFACE_SRCS
			${DBus_XML_SRCS}
			)
		SET(qrvthtool_TBM_SRCS
			${qrvthtool_TBM_SRCS}
			${qrvthtool_DBUS_INTERFACE_SRCS}
			TaskbarButtonManager/DockManager.cpp
			TaskbarButtonManager/UnityLauncher.cpp
			)
		SET(qrvthtool_TBM_MOC_H
			${qrvthtool_TBM_MOC_H}
			TaskbarButtonManager/DockManager.hpp
			TaskbarButtonManager/UnityLauncher.hpp
			)
	ENDIF(HAVE_Qt5DBus)
ENDIF()
QT5_WRAP_CPP(qrvthtool_TBM_MOC_SRCS ${qrvthtool_TBM_MOC_H})

######################
# Qt resource files. #
######################

SET(qrvthtool_RCC_SRCS
	resources/hw/hw.qrc
	resources/flags/flags.qrc
	)

QT5_ADD_RESOURCES(
	qrvthtool_RCC_O ${qrvthtool_RCC_SRCS}
	OPTIONS -no-compress
	)

#########################
# Win32 resource files. #
#########################

IF(WIN32)
        SET(qrvthtool_RC resources/win32/resource.rc)
ENDIF(WIN32)

#########################
# Build the executable. #
#########################

# TODO: Set Win32 when compiling release build
# to disable the command prompt window.
ADD_EXECUTABLE(qrvthtool WIN32 MACOSX_BUNDLE
	${qrvthtool_SRCS}
	${qrvthtool_UIS_H}
	${qrvthtool_MOC_SRCS}
	${qrvthtool_RCC_O}
	${qrvthtool_RC}
	${qrvthtool_TBM_SRCS}
	${qrvthtool_TBM_MOC_SRCS}
	)
ADD_DEPENDENCIES(qrvthtool git_version)
DO_SPLIT_DEBUG(qrvthtool)
SET_WINDOWS_SUBSYSTEM(qrvthtool WIN32)
SET_WINDOWS_NO_MANIFEST(qrvthtool)
SET_WINDOWS_ENTRYPOINT(qrvthtool WinMain OFF)

TARGET_INCLUDE_DIRECTORIES(qrvthtool
	PUBLIC	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
	PRIVATE	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/..>
	)

TARGET_LINK_LIBRARIES(qrvthtool PRIVATE rvth)

# Qt libraries
# NOTE: Libraries have to be linked in reverse order.
TARGET_LINK_LIBRARIES(qrvthtool PRIVATE Qt5::Widgets Qt5::Gui Qt5::Core)
IF(ENABLE_DBUS AND HAVE_Qt5DBus)
	TARGET_LINK_LIBRARIES(qrvthtool PRIVATE Qt5::DBus)
ENDIF(ENABLE_DBUS AND HAVE_Qt5DBus)
IF(WIN32)
	TARGET_LINK_LIBRARIES(qrvthtool PRIVATE Qt5::WinMain)
ENDIF(WIN32)

IF(CMAKE_THREAD_LIBS_INIT)
	TARGET_LINK_LIBRARIES(qrvthtool PRIVATE ${CMAKE_THREAD_LIBS_INIT})
ENDIF(CMAKE_THREAD_LIBS_INIT)

IF(HAVE_KF5WIDGETSADDONS)
	# NOTE: The library isn't actually needed, but we need to
	# link to the target in order to get the headers.
	TARGET_LINK_LIBRARIES(qrvthtool PRIVATE KF5::WidgetsAddons)
ENDIF(HAVE_KF5WIDGETSADDONS)

# Define -DQT_NO_DEBUG in release builds.
SET(CMAKE_C_FLAGS_RELEASE   "-DQT_NO_DEBUG ${CMAKE_C_FLAGS_RELEASE}")
SET(CMAKE_CXX_FLAGS_RELEASE "-DQT_NO_DEBUG ${CMAKE_CXX_FLAGS_RELEASE}")

# Qt options:
# - Fast QString concatenation. (Qt 4.6+, plus 4.8-specific version)
# - Disable implicit QString ASCII casts.
ADD_DEFINITIONS(-DQT_USE_FAST_CONCATENATION
	-DQT_USE_FAST_OPERATOR_PLUS
	-DQT_USE_QSTRINGBUILDER
	-DQT_NO_CAST_FROM_ASCII
	-DQT_NO_CAST_TO_ASCII
	-DQT_STRICT_ITERATORS
	-DQT_NO_URL_CAST_FROM_STRING
	)

# Qt5 requires "-fpic -fPIC" due to reduced relocations.
SET(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -fpic -fPIC")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpic -fPIC")

# Win32: image version.
INCLUDE(Win32ImageVersionLinkerFlags)
WIN32_IMAGE_VERSION_LINKER_FLAGS(${VERSION_MAJOR} ${VERSION_MINOR})

# Translations.
IF(ENABLE_NLS)
	ADD_DEPENDENCIES(qrvthtool translations_target)
ENDIF(ENABLE_NLS)

#################
# Installation. #
#################

INCLUDE(DirInstallPaths)

INSTALL(TARGETS qrvthtool
	RUNTIME DESTINATION "${DIR_INSTALL_EXE}"
	LIBRARY DESTINATION "${DIR_INSTALL_DLL}"
	ARCHIVE DESTINATION "${DIR_INSTALL_LIB}"
	COMPONENT "program"
	)
IF(INSTALL_DEBUG)
IF(MSVC)
	INSTALL(FILES "${CMAKE_CURRENT_BINARY_DIR}/qrvthtool.pdb"
		DESTINATION "${DIR_INSTALL_EXE_DEBUG}"
		COMPONENT "debug"
		)
ELSEIF(SPLIT_DEBUG)
	INSTALL(FILES "${CMAKE_CURRENT_BINARY_DIR}/qrvthtool.debug"
		DESTINATION "${DIR_INSTALL_EXE_DEBUG}"
		COMPONENT "debug"
		)
ENDIF()
ENDIF(INSTALL_DEBUG)

# FreeDesktop.org specifications.
IF(UNIX AND NOT APPLE)
	# Icons.
	FOREACH(ICON_SIZE 16x16 22x22 24x24 32x32 48x48 64x64 128x128 256x256)
		INSTALL(FILES resources/hw/${ICON_SIZE}/rvth.png
			DESTINATION share/icons/hicolor/${ICON_SIZE}/apps
			RENAME qrvthtool.png
			COMPONENT "desktop-icon"
			)
	ENDFOREACH()

        # Desktop file.
        INSTALL(FILES resources/xdg/qrvthtool.desktop
                DESTINATION share/applications
                COMPONENT "desktop-icon"
                )
ENDIF(UNIX AND NOT APPLE)

ENDIF(BUILD_QT5)
