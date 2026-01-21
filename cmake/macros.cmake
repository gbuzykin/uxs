# ##############################################################################
# Useful macro definitions

macro(install_win32_pdb TargetName)
  if(WIN32)
    install(
      FILES $<TARGET_PDB_FILE:${TargetName}>
      DESTINATION ${SYMBOLS_DIR}
      COMPONENT symbols
      OPTIONAL)
  endif()
endmacro()

macro(add_compile_defs_and_include_dirs_targets TargetName ExportDir)
  if(OPTION_EXPORT_COMPILE_DEFS_AND_INCLUDE_DIRS)
    string(MAKE_C_IDENTIFIER ${TargetName} TargetNameCIdentifier)
    add_custom_target(
      ${TargetName}_compile_defs
      COMMAND
        ${CMAKE_COMMAND} -E echo
        "\"$<JOIN:$<LIST:APPEND,$<TARGET_PROPERTY:${TargetName},COMPILE_DEFINITIONS>,${TargetNameCIdentifier}_EXPORTS>,;>\""
        >${ExportDir}/compile-defs.list)
    add_custom_target(
      ${TargetName}_include_dirs
      COMMAND
        ${CMAKE_COMMAND} -E echo
        "\"$<JOIN:$<TARGET_PROPERTY:${TargetName},INCLUDE_DIRECTORIES>,;>\""
        >${ExportDir}/include-dirs.list)
    add_dependencies(${TargetName} ${TargetName}_compile_defs
                     ${TargetName}_include_dirs)
  endif()
endmacro()
