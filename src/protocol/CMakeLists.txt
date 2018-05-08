
set(GENERATED_FILES "")
set(GENERATED_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../generated")
set(GENERATED_H_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../include/generated")

macro(GENERATE_PROTOCOL PROTOCOL_NAME)
  set(PROTOCOL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${PROTOCOL_NAME}.xml")
  set(OUTPUT_PATH_C "${GENERATED_SRC_DIR}/${PROTOCOL_NAME}.c")
  set(OUTPUT_PATH_SERVER_H "${GENERATED_H_DIR}/${PROTOCOL_NAME}-server.h")
  set(OUTPUT_PATH_CLIENT_H "${GENERATED_H_DIR}/${PROTOCOL_NAME}-client.h")
  add_custom_command(OUTPUT "${OUTPUT_PATH_C}"
    VERBATIM
    COMMAND "wayland-scanner" "--include-core-only" "code" "${PROTOCOL_PATH}" "${OUTPUT_PATH_C}"
    DEPENDS "${PROTOCOL_PATH}"
  )
  add_custom_command(OUTPUT "${OUTPUT_PATH_SERVER_H}"
    VERBATIM
    COMMAND "wayland-scanner" "--include-core-only" "server-header" "${PROTOCOL_PATH}" "${OUTPUT_PATH_SERVER_H}"
    DEPENDS "${PROTOCOL_PATH}"
  )
  add_custom_command(OUTPUT "${OUTPUT_PATH_CLIENT_H}"
    VERBATIM
    COMMAND "wayland-scanner" "--include-core-only" "client-header" "${PROTOCOL_PATH}" "${OUTPUT_PATH_CLIENT_H}"
    DEPENDS "${PROTOCOL_PATH}"
  )
  set(GENERATED_FILES ${GENERATED_FILES} "${OUTPUT_PATH_C}" "${OUTPUT_PATH_SERVER_H}" "${OUTPUT_PATH_CLIENT_H}")
endmacro()

# when adding a protocol, don't forget to add the generated .c file to CMake
GENERATE_PROTOCOL("wayland")
GENERATE_PROTOCOL("xdg-shell-unstable-v6")
GENERATE_PROTOCOL("xdg-shell")

add_custom_target(wlcs_protocols
  DEPENDS ${GENERATED_FILES}
  SOURCES ${GENERATED_FILES}
)
