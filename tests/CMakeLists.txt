# Same shape as cameraunlock-core/cpp/tests: one executable, one Run*Tests()
# per file. Only the game-process-free half of the mod is under test - the
# reflection walk and the detour need a live UE process, and are covered by
# `pixi run probe` against the game instead.
add_executable(acr_ht_tests
    test_main.cpp
    camera_transform_tests.cpp
    clean_pose_cache_tests.cpp
    config_sanitize_tests.cpp
    exe_path_tests.cpp
    legacy_reader_tests.cpp
)

target_compile_options(acr_ht_tests PRIVATE ${ACR_HT_STRICT_WARNINGS})
target_compile_definitions(acr_ht_tests PRIVATE ACR_HT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

target_link_libraries(acr_ht_tests PRIVATE acr_ht_core)

add_test(NAME acr_ht_tests COMMAND acr_ht_tests)

# The committed config against the table's fresh render, and the saves.
# `acr_ht_config_tests --render-config <path>` is pixi run render-config.
add_executable(acr_ht_config_tests config_tests.cpp)
target_compile_options(acr_ht_config_tests PRIVATE ${ACR_HT_STRICT_WARNINGS} /EHsc)
target_compile_definitions(acr_ht_config_tests PRIVATE ACR_HT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
target_link_libraries(acr_ht_config_tests PRIVATE acr_ht_core)
add_test(NAME acr_ht_config_tests COMMAND acr_ht_config_tests)

# The config differential test: v1.1.0's reader, the newest published build,
# against the frozen reader in src/legacy_config/, and that against the config
# owner's migration into CameraUnlock.ini. The oracle is compiled from byte
# copies in config_differential/oracle with the cameraunlock-core sources it
# included at v1.1.0's core pin, its `acr_ht` and `cameraunlock` namespaces
# renamed so it links beside this build's.
set(ACR_HT_DIFFERENTIAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/config_differential)
set(ACR_HT_ORACLE_DIR ${ACR_HT_DIFFERENTIAL_DIR}/oracle)
add_library(acr_ht_config_oracle STATIC
    ${ACR_HT_ORACLE_DIR}/src/config.cpp
    ${ACR_HT_ORACLE_DIR}/core/src/config/ini_reader.cpp
    ${ACR_HT_ORACLE_DIR}/core/src/logging/file_log.cpp
    ${ACR_HT_DIFFERENTIAL_DIR}/oracle_adapter.cpp
)
target_include_directories(acr_ht_config_oracle PRIVATE
    ${ACR_HT_ORACLE_DIR}/src
    ${ACR_HT_ORACLE_DIR}/core/include
    ${ACR_HT_DIFFERENTIAL_DIR}
)
target_compile_definitions(acr_ht_config_oracle PRIVATE
    acr_ht=acr_ht_oracle
    cameraunlock=cameraunlock_oracle
)

add_executable(acr_ht_config_differential
    ${ACR_HT_DIFFERENTIAL_DIR}/differential_tests.cpp
)
target_include_directories(acr_ht_config_differential PRIVATE
    ${ACR_HT_DIFFERENTIAL_DIR}
)
target_compile_definitions(acr_ht_config_differential PRIVATE
    ACR_HT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
target_compile_options(acr_ht_config_differential PRIVATE ${ACR_HT_STRICT_WARNINGS} /EHsc)
target_link_libraries(acr_ht_config_differential PRIVATE acr_ht_core acr_ht_config_oracle)
add_test(NAME acr_ht_config_differential COMMAND acr_ht_config_differential)

# What the differential test compiles is pinned by the SHA-256 of its text,
# line endings aside: the oracle's copies must stay v1.1.0's (git show
# v1.1.0:src/<file>) and core 3465659's (git -C cameraunlock-core show
# 3465659:cpp/<file>), the first-run files must stay what each release's
# kDefaultIniText in src/config.cpp spelled, the frozen reader is never
# edited, and the core reader it compiles must stay the one v1.1.0 was built
# with.
function(acr_ht_require_hash path expected)
    file(READ "${path}" text)
    string(REPLACE "\r" "" text "${text}")
    string(SHA256 actual "${text}")
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "${path} has changed (SHA-256 of its text ${actual}, pinned "
                            "${expected}). The config differential test compares against "
                            "exactly this file; see tests/CMakeLists.txt.")
    endif()
endfunction()
set(ACR_HT_CORE_DIR ${PROJECT_SOURCE_DIR}/cameraunlock-core/cpp)
foreach(pinned
        "${ACR_HT_ORACLE_DIR}/src/config.cpp|25d16171207287f8eb5bee7d362f2bd7e98a3d8ba0e6b96ce0c713e402dd0074"
        "${ACR_HT_ORACLE_DIR}/src/config.h|3620e3f486de59f07ddbbd9bfacacc88bfed82004d9e4ab0bd6bf2be87ea960a"
        "${ACR_HT_ORACLE_DIR}/src/config_sanitize.h|7c8a40a91983eb3cd494cce21dd1846ecb571fe573a74fb8ceeba14933b196a0"
        "${ACR_HT_ORACLE_DIR}/src/logging.h|c9006a006006aa645cb345f962d3c5924e3ec082fcc92b846866890fc6b9367f"
        "${ACR_HT_ORACLE_DIR}/core/include/cameraunlock/config/ini_reader.h|a7ffb44210ff59672fa97e8e5feaa2cb3e81938fcc0334a384c68bc371b3857a"
        "${ACR_HT_ORACLE_DIR}/core/include/cameraunlock/logging/file_log.h|43bdd2ef8554c78e5f440333463750c13b95110fe672b0b6e273244df9e7d169"
        "${ACR_HT_ORACLE_DIR}/core/include/cameraunlock/protocol/port_utils.h|91bff564d5e279b66527ec5553afcf4d591812dab0e78f71a48ef412e4db7a44"
        "${ACR_HT_ORACLE_DIR}/core/src/config/ini_reader.cpp|e01515c2656aaf533bae4350dc45b702c3e3d4043935743dcc9bd5ea581fbe1c"
        "${ACR_HT_ORACLE_DIR}/core/src/logging/file_log.cpp|73c53c2baa06bbfebe8211f62678aa2b60cb95f604743d3686951ba56b87ea47"
        "${ACR_HT_DIFFERENTIAL_DIR}/data/first-run-v1.0.0.ini|bb2bd21d47d6e65d5669d978b8f9d9160b7470bc3fd6c3ec67c3daeedeb4f6f0"
        "${ACR_HT_DIFFERENTIAL_DIR}/data/first-run-v1.0.1.ini|3ca3e4b7593a3e5f3c3a63ed2230941467ed71b09e2d9fa6f41b3a33f04dafce"
        "${ACR_HT_DIFFERENTIAL_DIR}/data/first-run-v1.0.2.ini|89d0fe52acae43547ee7744d9456576b86d8bd819323c703ae52d6c2f91dfe5a"
        "${ACR_HT_DIFFERENTIAL_DIR}/data/first-run-v1.0.3.ini|a3a4cbe3f2c24d9644fcba5171803d37f6b1c0bf04e545a67bad89bc42a7a698"
        "${ACR_HT_DIFFERENTIAL_DIR}/data/first-run-v1.1.0.ini|cb1c90b096f0180935ec9e0e72728258df08daec6b6a3dde4f593221168c5663"
        "${PROJECT_SOURCE_DIR}/src/legacy_config/legacy_config.h|3d9af92c316945e621cfba36ef98f9fcf6925349d17b77bfc34a0adeb99dbadd"
        "${PROJECT_SOURCE_DIR}/src/legacy_config/legacy_config.cpp|0a92b7d69216fbb70dcb3e1772d6064fc433aebf0fed992774b457464e92e929"
        "${PROJECT_SOURCE_DIR}/src/legacy_config/config_sanitize.h|244828621b31c8cf1e834f3b5a43e1a49aae7ad44755e817d61a1fa3a3194106"
        "${ACR_HT_CORE_DIR}/include/cameraunlock/config/ini_reader.h|a7ffb44210ff59672fa97e8e5feaa2cb3e81938fcc0334a384c68bc371b3857a"
        "${ACR_HT_CORE_DIR}/src/config/ini_reader.cpp|e01515c2656aaf533bae4350dc45b702c3e3d4043935743dcc9bd5ea581fbe1c")
    string(REPLACE "|" ";" pair "${pinned}")
    list(GET pair 0 pinned_path)
    list(GET pair 1 pinned_hash)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${pinned_path}")
    acr_ht_require_hash("${pinned_path}" "${pinned_hash}")
endforeach()
