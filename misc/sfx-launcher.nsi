!include "meta.nsh"
# useful docs: <https://nsis.sourceforge.io/Docs/Chapter4.html>

OutFile "..\${PROJECT_NAME}-${VERSION_STRING}.exe"
Name "${PROJECT_NAME_C}"
Icon "${ICONPATH}"
SetCompressor /SOLID lzma
SetDateSave off
RequestExecutionLevel user

VIAddVersionKey "CompanyName" "${PROJECT_NAME_C} community"
VIAddVersionKey "FileDescription" "${PROJECT_NAME_C} self-extracting launcher"
VIAddVersionKey "FileVersion" "${VERSION_STRING}"
VIAddVersionKey "ProductName" "${PROJECT_NAME_C}"
VIAddVersionKey "InternalName" "${PROJECT_NAME}"
VIAddVersionKey "LegalCopyright" "(c) 2010-2025 Perttu Ahola (celeron55) and contributors"
# these are required, but don't take arbitrary strings
VIProductVersion "0.0.0.0"
VIFileVersion "0.0.0.0"

!if ${DEVELOPMENT_BUILD} == 1
# since the version string needs to be unique for the "extract once" logic to work,
# and dev builds may not have one we choose to just always extract
!define TARGET_DIR "$TEMP\${PROJECT_NAME}-dev"
!else
!if ${DEVELOPMENT_BUILD} == 0
!define TARGET_DIR "$LOCALAPPDATA\${PROJECT_NAME}\${VERSION_STRING}"
# signals successful extraction
!define DUMMY_FILE "${TARGET_DIR}\.extracted"
!else
!error invalid value for DEVELOPMENT_BUILD
!endif
!endif
!define EXE_FILE "${TARGET_DIR}\bin\${PROJECT_NAME}.exe"

!include "LogicLib.nsh"
!include "FileFunc.nsh"

# including MUI.nsh would cause a warning, so we have to use numbers :(
# these are LCIDs (Language Code Identifiers)
LangString BannerText 1033 "Extracting, please wait..."
LangString BannerText 1031 "Extrahiere, bitte warten..."
LangString ErrorText 1033 "An error occurred!"
LangString ErrorText 1031 "Ein Fehler ist aufgetreten!"

Var needExtract

Function .onInit
	SetSilent silent
	StrCpy $needExtract 1

!if ${DEVELOPMENT_BUILD} == 0
	# the *.* checks if a directory exists
	${If} ${FileExists} "${EXE_FILE}"
	${AndIf} ${FileExists} "${TARGET_DIR}\builtin\*.*"
	${AndIf} ${FileExists} "${TARGET_DIR}\client\*.*"
	${AndIf} ${FileExists} "${TARGET_DIR}\textures\*.*"
	${AndIf} ${FileExists} "${DUMMY_FILE}"
		StrCpy $needExtract 0
	${EndIf}
!endif
FunctionEnd

Section
	ClearErrors
	${If} $needExtract == 1
		Banner::show /NOUNLOAD "$(BannerText)"

!if ${DEVELOPMENT_BUILD} == 1
		RMDir /r "${TARGET_DIR}"
!endif
		CreateDirectory "${TARGET_DIR}"
		SetOutPath "${TARGET_DIR}"
		File /r /x *.nsi /x *.nsh "${INPATH}\*.*"

		${If} ${Errors}
			MessageBox MB_ICONSTOP "$(ErrorText)"
			Abort
		${EndIf}

!if ${DEVELOPMENT_BUILD} == 0
		FileOpen $0 "${DUMMY_FILE}" w
		FileClose $0
		SetFileAttributes "${DUMMY_FILE}" HIDDEN
!endif

		Banner::destroy
	${EndIf}

!if ${DEVELOPMENT_BUILD} == 0
	# replace shortcut with last-launched version regardless of if we had to extract
	# (last param is the description text)
	CreateShortcut /NoWorkingDir "$DESKTOP\${PROJECT_NAME_C}.lnk" "${EXE_FILE}" \
		"" "" "" "" "" "${PROJECT_NAME_C} ${VERSION_STRING}"
	CreateShortcut /NoWorkingDir "$SMPROGRAMS\${PROJECT_NAME_C}.lnk" "${EXE_FILE}" \
		"" "" "" "" "" "${PROJECT_NAME_C} ${VERSION_STRING}"
!endif

	Exec '"${EXE_FILE}"'
SectionEnd
