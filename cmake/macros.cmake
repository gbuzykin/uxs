# ##############################################################################
# Useful macro definitions

macro(install_win32_pdb TargetName)
  if(WIN32)
    install(
      FILES $<TARGET_PDB_FILE:${TargetName}>
      DESTINATION ${INSTALL_WIN32_SYMBOLS_DIR}
      COMPONENT symbols
      OPTIONAL)
  endif()
endmacro()

macro(add_compile_defs_and_include_dirs_targets TargetName ExportDir)
  if(OPTION_EXPORT_COMPILE_DEFS_AND_INCLUDE_DIRS)
    get_target_property(RealTargetName ${TargetName} ALIASED_TARGET)
    if(NOT RealTargetName)
      set(RealTargetName ${TargetName})
    endif()
    set(COMPILE_DEFS_LIST
        $<TARGET_PROPERTY:${RealTargetName},COMPILE_DEFINITIONS>)
    get_target_property(TargetType ${RealTargetName} TYPE)
    if(TargetType STREQUAL "SHARED_LIBRARY")
      string(MAKE_C_IDENTIFIER ${RealTargetName} TargetNameCIdentifier)
      set(COMPILE_DEFS_LIST
          $<LIST:APPEND,${COMPILE_DEFS_LIST},${TargetNameCIdentifier}_EXPORTS>)
    endif()
    add_custom_target(
      ${TargetName}_compile_defs
      COMMAND
        ${CMAKE_COMMAND} -E echo
        "\"$<STRING:REPLACE,$<JOIN:${COMPILE_DEFS_LIST},;>,\",\\\">\""
        >${ExportDir}/compile-defs.list)
    add_custom_target(
      ${TargetName}_include_dirs
      COMMAND
        ${CMAKE_COMMAND} -E echo
        "\"$<STRING:REPLACE,$<JOIN:$<TARGET_PROPERTY:${RealTargetName},INCLUDE_DIRECTORIES>,;>,\",\\\">\""
        >${ExportDir}/include-dirs.list)
    add_dependencies(${RealTargetName} ${TargetName}_compile_defs
                     ${TargetName}_include_dirs)
  endif()
endmacro()
