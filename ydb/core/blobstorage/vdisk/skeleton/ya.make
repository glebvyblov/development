LIBRARY()

OWNER(g:kikimr)

PEERDIR(
    ydb/core/base
    ydb/core/blobstorage/vdisk/hulldb/base
    ydb/core/protos
)

SRCS(
    defs.h
    blobstorage_db.cpp
    blobstorage_db.h
    blobstorage_monactors.cpp
    blobstorage_monactors.h
    blobstorage_skeleton.cpp
    blobstorage_skeleton.h
    blobstorage_skeletonerr.h
    blobstorage_skeletonfront.cpp
    blobstorage_skeletonfront.h
    blobstorage_syncfull.cpp
    blobstorage_syncfull.h
    blobstorage_syncfullhandler.cpp
    blobstorage_syncfullhandler.h
    blobstorage_takedbsnap.h
    skeleton_capturevdisklayout.h
    skeleton_compactionstate.cpp
    skeleton_compactionstate.h
    skeleton_events.h
    skeleton_loggedrec.cpp
    skeleton_loggedrec.h
    skeleton_mon_dbmainpage.cpp
    skeleton_mon_dbmainpage.h
    skeleton_mon_util.h
    skeleton_oos_logic.cpp
    skeleton_oos_logic.h
    skeleton_oos_tracker.cpp
    skeleton_oos_tracker.h
    skeleton_overload_handler.cpp
    skeleton_overload_handler.h
    skeleton_vmultiput_actor.cpp
    skeleton_vmultiput_actor.h
    skeleton_vmovedpatch_actor.cpp 
    skeleton_vmovedpatch_actor.h 
    skeleton_vpatch_actor.cpp 
    skeleton_vpatch_actor.h 
)

END()

RECURSE_FOR_TESTS(
    ut
)
