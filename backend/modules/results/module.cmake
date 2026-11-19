# results' own build steps (11.C11): the explanation pages' library compiled into libfusion as byte
# arrays with their SHA-256, so pex writes it byte for byte and never looks for it on disk (07.C3, S6.18).
# Included by fusion_add_module(results), which names the target fusion_${name} and the folder ${dir}.

# vis-network's standalone build at the release section 11 pins (07.C3), checked by its SHA-256. The
# two URLs serve the npm package's file; npm's signed integrity hash of vis-network-10.1.5.tgz matched
# the tarball whose standalone/umd/vis-network.min.js has this SHA-256 (653,713 bytes, 2026-11-17).
include(FetchContent)
set(FUSION_VIS_NETWORK_VERSION 10.1.5)
set(FUSION_VIS_NETWORK_SHA256 b427322ad953d70ab75d1b798ebb8349863502f244e02de3e12a4ab7dfeddf2c)
FetchContent_Declare(vis_network
  URL "https://cdn.jsdelivr.net/npm/vis-network@${FUSION_VIS_NETWORK_VERSION}/standalone/umd/vis-network.min.js"
      "https://unpkg.com/vis-network@${FUSION_VIS_NETWORK_VERSION}/standalone/umd/vis-network.min.js"
  URL_HASH SHA256=${FUSION_VIS_NETWORK_SHA256}
  DOWNLOAD_NO_EXTRACT TRUE)
FetchContent_MakeAvailable(vis_network)

# One generated source per asset: <symbol>.cpp holds the bytes, their size and their SHA-256.
set(_results_assets
  "vis_network_min_js|${vis_network_SOURCE_DIR}/vis-network.min.js"
  "fusion_explain_js|${dir}/assets/fusion-explain.js"
  "fusion_explain_css|${dir}/assets/fusion-explain.css"
  "licenses_txt|${dir}/assets/LICENSES.txt")
foreach(_entry IN LISTS _results_assets)
  string(REPLACE "|" ";" _parts "${_entry}")
  list(GET _parts 0 _symbol)
  list(GET _parts 1 _input)
  set(_output "${CMAKE_CURRENT_BINARY_DIR}/results_assets/${_symbol}.cpp")
  add_custom_command(
    OUTPUT "${_output}"
    COMMAND "${CMAKE_COMMAND}" "-DINPUT=${_input}" "-DOUTPUT=${_output}" "-DSYMBOL=${_symbol}"
            -P "${dir}/embed_asset.cmake"
    DEPENDS "${_input}" "${dir}/embed_asset.cmake"
    COMMENT "Embedding ${_symbol} into fusion_results (11.C11)"
    VERBATIM)
  target_sources(fusion_${name} PRIVATE "${_output}")
endforeach()
