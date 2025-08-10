# catch2 is distributed as a standalone header / source amalgamation.
#
# Downloaded from:
#
#      https://github.com/catchorg/Catch2/releases/tag/v3.6.0
#
# The following changes were made to always print in microseconds, fixed format:
#
# -    explicit Duration(double inNanoseconds, Unit units = Unit::Auto)
# +    explicit Duration(double inNanoseconds, Unit units = Unit::Microseconds)
#
# -        return os << duration.value() << ' ' << duration.unitsAsString();
# +        return os << std::fixed << duration.value() << ' ' << duration.unitsAsString();

add_library(catch2 STATIC catch_amalgamated.cpp catch_amalgamated.hpp)
target_compile_definitions(catch2 PRIVATE CATCH_CONFIG_NOSTDOUT CATCH_AMALGAMATED_CUSTOM_MAIN)
add_library(Catch2::Catch2 ALIAS catch2)
target_include_directories(catch2 INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
