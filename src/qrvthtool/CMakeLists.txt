PROJECT(qrvthtool)

# Main binary directory. Needed for git_version.h
INCLUDE_DIRECTORIES("${CMAKE_BINARY_DIR}")

# Find Qt5.
# FIXME: Figure out a minimum Qt5 version.
# QtWin::fromHICON was added in Qt 5.2.
SET(Qt5_NO_LINK_QTMAIN 1)
FIND_PACKAGE(Qt5 5.2.0 REQUIRED COMPONENTS Core Gui Widgets)

# Sources.
SET(qrvthtool_SRCS
	qrvthtool.cpp
	RvtHModel.cpp
	RvtHSortFilterProxyModel.cpp
	RvtHItemDelegate.cpp

	widgets/BankEntryView.cpp
	widgets/QTreeViewOpt.cpp

	windows/QRvtHToolWindow.cpp
	)

# Headers with Qt objects.
SET(qrvthtool_MOC_H
	RvtHModel.hpp
	RvtHSortFilterProxyModel.hpp
	RvtHItemDelegate.hpp

	widgets/BankEntryView.hpp
	widgets/QTreeViewOpt.hpp

	windows/QRvtHToolWindow.hpp
	)
QT5_WRAP_CPP(qrvthtool_MOC_SRCS ${qrvthtool_MOC_H})

# UI files.
SET(qrvthtool_UIS
	widgets/BankEntryView.ui

	windows/QRvtHToolWindow.ui
	)
QT5_WRAP_UI(qrvthtool_UIS_H ${qrvthtool_UIS})

######################
# Qt resource files. #
######################

SET(qrvthtool_RCC_SRCS
	resources/hw/hw.qrc
	)

QT5_ADD_RESOURCES(
	qrvthtool_RCC_O ${qrvthtool_RCC_SRCS}
	OPTIONS -no-compress
	)

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

TARGET_LINK_LIBRARIES(qrvthtool rvth)

# Qt libraries
# NOTE: Libraries have to be linked in reverse order.
TARGET_LINK_LIBRARIES(qrvthtool Qt5::Widgets Qt5::Gui Qt5::Core)
IF(WIN32)
	TARGET_LINK_LIBRARIES(qrvthtool Qt5::WinMain)
ENDIF(WIN32)

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

# Win32: image version.
INCLUDE(Win32ImageVersionLinkerFlags)
WIN32_IMAGE_VERSION_LINKER_FLAGS(${VERSION_MAJOR} ${VERSION_MINOR})

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
