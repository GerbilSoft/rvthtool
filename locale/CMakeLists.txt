# Localization files.
PROJECT(locale)

# rvthtool translations subproject.

IF(QT_VERSION EQUAL 5)
	FIND_PACKAGE(Qt5 COMPONENTS Core LinguistTools)
	IF(Qt5_FOUND AND Qt5Core_FOUND AND Qt5LinguistTools_FOUND)
		# Found Qt5.
	ELSE()
		# Did not find Qt5.
		MESSAGE(WARNING "Qt5 not found. Not building localization files.")
		SET(ENABLE_NLS OFF)
	ENDIF()

	# Qt 5.15/6 has qt_add_translation
	# We can't depend on having Qt 5.15, though.
	IF(NOT COMMAND QT_ADD_TRANSLATION)
		FUNCTION(QT_ADD_TRANSLATION _qm_files)
			QT5_ADD_TRANSLATION("${_qm_files}" ${ARGN})
			SET("${_qm_files}" "${${_qm_files}}" PARENT_SCOPE)
		ENDFUNCTION(QT_ADD_TRANSLATION)
	ENDIF(NOT COMMAND QT_ADD_TRANSLATION)
ELSEIF(QT_VERSION EQUAL 6)
	FIND_PACKAGE(Qt6 COMPONENTS Core LinguistTools)
	IF(Qt6_FOUND AND Qt6Core_FOUND AND Qt6LinguistTools_FOUND)
		# Found Qt6.
	ELSE()
		# Did not find Qt6.
		MESSAGE(WARNING "Qt6 not found. Not building localization files.")
		SET(ENABLE_NLS OFF)
	ENDIF()
ELSE()
	MESSAGE(FATAL_ERROR "Unsupported Qt version: ${QT_VERSION}")
ENDIF()

IF(ENABLE_NLS)

# Compile the translations.
SET(TRANSLATION_FILES
	rvthtool_de.ts
	rvthtool_en.ts
	rvthtool_es.ts
	rvthtool_1337.ts
	)

QT_ADD_TRANSLATION(QM_FILES ${TRANSLATION_FILES})

# Make the destination files available in parent score.
SET(QM_FILES ${QM_FILES} PARENT_SCOPE)
ADD_CUSTOM_TARGET(translations_target DEPENDS ${QM_FILES})

#################
# Installation. #
#################

INCLUDE(DirInstallPaths)

INSTALL(FILES ${QM_FILES}
	DESTINATION "${DIR_INSTALL_TRANSLATIONS}"
	COMPONENT "translations"
	)

# System translations.
# Install on Windows and Mac OS X, or if we're using statically-linked Qt.
# Dynamically-linked Qt uses system translations.
IF(QT_IS_STATIC OR APPLE OR NOT UNIX)
	FILE(GLOB QT_SYS_TRANSLATIONS "${QT_TRANSLATIONS_DIR}/qt_??.qm")
	INSTALL(FILES ${QT_SYS_TRANSLATIONS}
		DESTINATION ${DIR_INSTALL_TRANSLATIONS}
		COMPONENT "translations"
		)
ENDIF(QT_IS_STATIC OR APPLE OR NOT UNIX)

ENDIF(ENABLE_NLS)
