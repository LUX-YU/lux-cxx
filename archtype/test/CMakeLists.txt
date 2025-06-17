add_executable(
	archtype_test
	archtype_test.cpp
)

target_link_libraries(
	archtype_test
	PRIVATE
	lux::cxx::archtype
)

set(ENABLE_COMPARE_WITH_ENTT false CACHE BOOL "Enable compare archtype with entt")
if(ENABLE_COMPARE_WITH_ENTT)
	find_package(EnTT    CONFIG REQUIRED)
	find_path(ENTT_INCLUDE_DIRS "entt/entt.hpp")
	if(NOT ENTT_INCLUDE_DIRS)
	    message(FATAL_ERROR "EnTT include directory not found.")
	else()
	    message(STATUS "EnTT include directory found: ${ENTT_INCLUDE_DIRS}")
	endif()

	add_executable(
		entt_test
		entt_test.cpp
	)

	target_link_libraries(
		entt_test
		PRIVATE
		lux::cxx::archtype
	)

	target_include_directories(
		entt_test
		PRIVATE
		${ENTT_INCLUDE_DIRS}
	)

endif(ENABLE_COMPARE_WITH_ENTT)