# Find the Clockwork client library used by Humid.
#
# Prefer the humid-bundled clockwork submodule (stage after
# `cd clockwork && make client-install`). Do not silently pick an
# unrelated tree such as /opt/latproc: mismatched headers vs libcw_client.a
# cause compile errors (e.g. missing addSetupResponder) or runtime crashes.
#
# Source work for CHANNEL/ZMQ client fixes: clockwork branch
# prod-client-zmq-fix (line C). Pin that commit in the submodule; see
# docs/CLOCKWORK_CLIENT_BRANCHES.md.

# The client build and its staged library use the headers from iod/src.  These
# headers must stay in lockstep with libcw_client.a: ConnectionManager and its
# derived classes are C++ objects whose layouts differ from clockwork/src.
set(_ClockworkClient_LIB_HINT "${PROJECT_SOURCE_DIR}/clockwork/iod/stage/lib")
set(_ClockworkClient_INC_HINT "${PROJECT_SOURCE_DIR}/clockwork/iod/src")

# If the submodule client is already staged, force it over any cached path
# from a previous configure (common on panels that once used /opt/latproc).
if (EXISTS "${_ClockworkClient_LIB_HINT}/libcw_client.a")
  set(ClockworkClient_LIBRARY "${_ClockworkClient_LIB_HINT}/libcw_client.a"
      CACHE FILEPATH "Clockwork client library" FORCE)
endif()
if (EXISTS "${_ClockworkClient_INC_HINT}/value.h")
  set(ClockworkClient_INCLUDE_DIR "${_ClockworkClient_INC_HINT}"
      CACHE PATH "Clockwork client headers" FORCE)
endif()

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(ClockworkClient
  DEFAULT_MSG
  ClockworkClient_LIBRARY
  ClockworkClient_INCLUDE_DIR)

if (ClockworkClient_FOUND)
  set(ClockworkClient_LIBRARIES ${ClockworkClient_LIBRARY})
endif()

mark_as_advanced(ClockworkClient_LIBRARY ClockworkClient_INCLUDE_DIR)
