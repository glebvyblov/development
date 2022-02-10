LIBRARY(common)

OWNER(g:kikimr)

SRCS(
    aws.cpp
    command.cpp
    common.cpp
    examples.cpp
    format.cpp
    interruptible.cpp
    normalize_path.cpp
    parameters.cpp
    pretty_table.cpp
    print_operation.cpp
    print_utils.cpp
    profile_manager.cpp
    query_stats.cpp
    recursive_list.cpp
    root.cpp
    tabbed_table.cpp
    ydb_updater.cpp
    yt.cpp
)

PEERDIR(
    library/cpp/getopt 
    library/cpp/json/writer
    library/cpp/yaml/as
    ydb/public/lib/json_value
    ydb/public/lib/operation_id
    ydb/public/lib/yson_value
    ydb/public/sdk/cpp/client/draft
    ydb/public/sdk/cpp/client/ydb_result
    ydb/public/sdk/cpp/client/ydb_scheme
    ydb/public/sdk/cpp/client/ydb_types/credentials
)

GENERATE_ENUM_SERIALIZATION(formats.h)

END()

RECURSE_FOR_TESTS(
    ut
)
