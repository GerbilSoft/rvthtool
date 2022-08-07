# RVT-H Tool Qt GUI frontend
CMAKE_MINIMUM_REQUIRED(VERSION 3.1)
CMAKE_POLICY(SET CMP0048 NEW)
IF(POLICY CMP0063)
	# CMake 3.3: Enable symbol visibility presets for all
	# target types, including static libraries and executables.
	CMAKE_POLICY(SET CMP0063 NEW)
ENDIF(POLICY CMP0063)
PROJECT(qrvthtool LANGUAGES CXX)

# Main binary directory. Needed for git_version.h
INCLUDE_DIRECTORIES("${CMAKE_BINARY_DIR}")

IF(QT_VERSION EQUAL 5)
	# Find Qt5.
	# FIXME: Figure out a minimum Qt5 version.
	# QtWin::fromHICON was added in Qt 5.2.
	SET(Qt5_NO_LINK_QTMAIN 1)
	FIND_PACKAGE(Qt5 5.2.0 COMPONENTS Core Gui Widgets)
	IF(Qt5_FOUND AND Qt5Core_FOUND AND Qt5Gui_FOUND AND Qt5Widgets_FOUND)
		# Found Qt5.
		SET(BUILD_QT ON)
		SET(QT_NS Qt5)
		IF(ENABLE_DBUS)
			# QtDBus is used for DockManager.
			FIND_PACKAGE(Qt5DBus)
			SET(HAVE_QtDBus ${Qt5DBus_FOUND})
		ENDIF(ENABLE_DBUS)
	ELSE()
		# Did not find Qt5.
		MESSAGE(WARNING "Qt5 not found. Not building qrvthtool.")
	ENDIF()

	# Qt 5.15/6 has qt_wrap_cpp.
	# We can't depend on having Qt 5.15, though.
	IF(Qt5_VERSION VERSION_LESS 5.15.0)
		FUNCTION(QT_WRAP_CPP outfiles)
			QT5_WRAP_CPP(${outfiles} ${ARGN})
			SET(${outfiles} ${${outfiles}} PARENT_SCOPE)
		ENDFUNCTION(QT_WRAP_CPP)
		FUNCTION(QT_ADD_RESOURCES outfiles)
			QT5_ADD_RESOURCES("${outfiles}" ${ARGN})
			SET("${outfiles}" "${${outfiles}}" PARENT_SCOPE)
		ENDFUNCTION(QT_ADD_RESOURCES)
		FUNCTION(QT_WRAP_UI outfiles)
			QT5_WRAP_UI("${outfiles}" ${ARGN})
			SET("${outfiles}" "${${outfiles}}" PARENT_SCOPE)
		ENDFUNCTION(QT_WRAP_UI)
		FUNCTION(QT_ADD_DBUS_INTERFACES sources)
			QT5_ADD_DBUS_INTERFACES("${sources}" ${ARGN})
			SET("${sources}" "${${sources}}" PARENT_SCOPE)
		ENDFUNCTION(QT_ADD_DBUS_INTERFACES sources)
	ENDIF(Qt5_VERSION VERSION_LESS 5.15.0)
ELSEIF(QT_VERSION EQUAL 6)
	# Find Qt6.
	SET(Qt6_NO_LINK_QTMAIN 1)
	FIND_PACKAGE(Qt6 6.0.0 COMPONENTS Core Gui Widgets)
	IF(Qt6_FOUND AND Qt6Core_FOUND AND Qt6Gui_FOUND AND Qt6Widgets_FOUND)
		# Found Qt6.
		SET(BUILD_QT ON)
		SET(QT_NS Qt6)
		IF(ENABLE_DBUS)
			# QtDBus is used for DockManager.
			FIND_PACKAGE(Qt6DBus)
			SET(HAVE_QtDBus ${Qt6DBus_FOUND})
		ENDIF(ENABLE_DBUS)
	ELSE()
		# Did not find Qt6.
		MESSAGE(WARNING "Qt6 not found. Not building qrvthtool.")
	ENDIF()
ELSE()
	MESSAGE(FATAL_ERROR "Unsupported Qt version: ${QT_VERSION}")
ENDIF()

IF(BUILD_QT)

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
IF(UNIX AND NOT APPLE AND (QT_VERSION EQUAL 5))
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
ENDIF(UNIX AND NOT APPLE AND (QT_VERSION EQUAL 5))


# Write the config.h file.
CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/config.qrvthtool.h.in" "${CMAKE_CURRENT_BINARY_DIR}/config.qrvthtool.h")

# Sources
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
	windows/AboutDialog.cpp
	)

# Headers without Qt objects
SET(qrvthtool_H
	MessageSound.hpp
	)

# Headers with Qt objects
SET(qrvthtool_MOC_H
	RvtHModel.hpp
	RvtHSortFilterProxyModel.hpp
	TranslationManager.hpp
	WorkerObject.hpp

	widgets/BankEntryView.hpp
	widgets/LanguageMenu.hpp
	widgets/MessageWidget.hpp
	widgets/MessageWidgetStack.hpp
	widgets/QTreeViewOpt.hpp
	widgets/QItemView_Text.hpp

	windows/QRvtHToolWindow.hpp
	windows/SelectDeviceDialog.hpp
	windows/AboutDialog.hpp
	)
QT_WRAP_CPP(qrvthtool_MOC_SRCS ${qrvthtool_MOC_H})

# UI files
SET(qrvthtool_UIS
	widgets/BankEntryView.ui

	windows/QRvtHToolWindow.ui
	windows/SelectDeviceDialog.ui
	windows/AboutDialog.ui
	)
QT_WRAP_UI(qrvthtool_UIS_H ${qrvthtool_UIS})

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
	IF(HAVE_QtDBus)
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
		QT_ADD_DBUS_INTERFACES(qrvthtool_DBUS_INTERFACE_SRCS ${DBus_XML_SRCS})
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
	ENDIF(HAVE_QtDBus)
ENDIF()
QT_WRAP_CPP(qrvthtool_TBM_MOC_SRCS ${qrvthtool_TBM_MOC_H})

######################
# Qt resource files. #
######################

SET(qrvthtool_RCC_SRCS
	resources/hw/hw.qrc
	resources/flags/flags.qrc
	)
IF(WIN32 OR APPLE)
	SET(qrvthtool_RCC_SRCS
		${qrvthtool_RCC_SRCS}
		resources/oxygen/oxygen.qrc
		)
ENDIF(WIN32 OR APPLE)

QT_ADD_RESOURCES(
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
	${qrvthtool_SRCS} ${qrvthtool_H}
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
TARGET_LINK_LIBRARIES(qrvthtool PRIVATE ${QT_NS}::Widgets ${QT_NS}::Gui ${QT_NS}::Core)
IF(ENABLE_DBUS AND HAVE_QtDBus)
	TARGET_LINK_LIBRARIES(qrvthtool PRIVATE ${QT_NS}::DBus)
ENDIF(ENABLE_DBUS AND HAVE_QtDBus)
# FIXME: Not available on Qt6; not needed on Qt5 either?
IF(WIN32 AND QT_VERSION EQUAL 5)
	TARGET_LINK_LIBRARIES(qrvthtool PRIVATE ${QT_NS}::WinMain)
ENDIF(WIN32 AND QT_VERSION EQUAL 5)

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
IF(UNIX AND NOT APPLE)
	SET(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -fpic -fPIC")
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpic -fPIC")
ENDIF(UNIX AND NOT APPLE)

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
	# FIXME: Generator expression $<TARGET_PROPERTY:${_target},PDB> didn't work with CPack-3.6.1.
	GET_TARGET_PROPERTY(DEBUG_FILENAME qrvthtool PDB)
	IF(DEBUG_FILENAME)
		INSTALL(FILES "${DEBUG_FILENAME}"
			DESTINATION "${DIR_INSTALL_EXE_DEBUG}"
			COMPONENT "debug"
			)
	ENDIF()
ENDIF(INSTALL_DEBUG)

# FreeDesktop.org specifications
IF(DIR_INSTALL_XDG_ICONS)
	FOREACH(ICON_SIZE 16x16 22x22 24x24 32x32 48x48 64x64 128x128 256x256)
		INSTALL(FILES resources/hw/${ICON_SIZE}/rvth.png
			DESTINATION "${DIR_INSTALL_XDG_ICONS}/hicolor/${ICON_SIZE}/apps"
			RENAME qrvthtool.png
			COMPONENT "xdg"
			)
	ENDFOREACH()
ENDIF(DIR_INSTALL_XDG_ICONS)

IF(DIR_INSTALL_XDG_DESKTOP)
        INSTALL(FILES resources/xdg/qrvthtool.desktop
                DESTINATION "${DIR_INSTALL_XDG_DESKTOP}/com.gerbilsoft.qrvthtool.desktop"
                COMPONENT "xdg"
                )
ENDIF(DIR_INSTALL_XDG_DESKTOP)

# AppStream metainfo XML
IF(DIR_INSTALL_XDG_APPSTREAM)
	INSTALL(FILES com.gerbilsoft.qrvthtool.metainfo.xml
		DESTINATION "${DIR_INSTALL_XDG_APPSTREAM}"
		COMPONENT "xdg")
ENDIF(DIR_INSTALL_XDG_APPSTREAM)

# Install Qt DLLs when running `cpack` on Windows builds.
# References:
# - https://stackoverflow.com/questions/41193584/deploy-all-qt-dependencies-when-building
# - https://stackoverflow.com/a/41199492
INCLUDE(qt_windeploy.cmake)
IF(TARGET ${QT_NS}::windeployqt)
	# execute windeployqt in a tmp directory after build
	ADD_CUSTOM_COMMAND(TARGET qrvthtool POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_CURRENT_BINARY_DIR}/windeployqt"
		COMMAND set PATH=%PATH%$<SEMICOLON>${qt_install_prefix}/bin
		COMMAND ${QT_NS}::windeployqt --dir "${CMAKE_CURRENT_BINARY_DIR}/windeployqt" "$<TARGET_FILE_DIR:qrvthtool>/$<TARGET_FILE_NAME:qrvthtool>"
	)

	# copy deployment directory during installation
	INSTALL(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/windeployqt/"
		DESTINATION "${DIR_INSTALL_EXE}")
ENDIF()

ENDIF(BUILD_QT)
