# Install Qt DLLs when running `cpack` on Windows builds.
# References:
# - https://stackoverflow.com/questions/41193584/deploy-all-qt-dependencies-when-building
# - https://stackoverflow.com/a/41199492
IF(${QT_NS}_FOUND AND WIN32 AND TARGET ${QT_NS}::qmake AND NOT TARGET ${QT_NS}::windeployqt)
	GET_TARGET_PROPERTY(_qt_qmake_location ${QT_NS}::qmake IMPORTED_LOCATION)

	EXECUTE_PROCESS(
		COMMAND "${_qt_qmake_location}" -query QT_INSTALL_PREFIX
		RESULT_VARIABLE return_code
		OUTPUT_VARIABLE qt_install_prefix
		OUTPUT_STRIP_TRAILING_WHITESPACE
		)

	SET(imported_location "${qt_install_prefix}/bin/windeployqt.exe")

	IF(EXISTS ${imported_location})
		ADD_EXECUTABLE(${QT_NS}::windeployqt IMPORTED)

		SET_TARGET_PROPERTIES(${QT_NS}::windeployqt PROPERTIES
			IMPORTED_LOCATION ${imported_location})
	ENDIF(EXISTS ${imported_location})
ENDIF()
