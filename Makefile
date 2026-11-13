.PHONY: clean clean_all

PROJ_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

# Main extension configuration
EXTENSION_NAME=read_stat

EXTENSION_DEBUG_FLAGS=-Og -ggdb3

# Set to 1 to enable Unstable API (binaries will only work on TARGET_DUCKDB_VERSION, forwards compatibility will be broken)
# WARNING: When set to 1, the duckdb_extension.h from the TARGET_DUCKDB_VERSION must be used, using any other version of
#          the header is unsafe.
USE_UNSTABLE_C_API=0

# The DuckDB version to target
TARGET_DUCKDB_VERSION=v1.5.6

all: configure release

# Include makefiles from DuckDB
include extension-ci-tools/makefiles/c_api_extensions/base.Makefile
include extension-ci-tools/makefiles/c_api_extensions/c_cpp.Makefile

configure: patch_readstat venv platform extension_version

debug: patch_readstat build_extension_library_debug build_extension_with_metadata_debug
release: patch_readstat build_extension_library_release build_extension_with_metadata_release

test: test_debug
test_debug: test_extension_debug
test_release: test_extension_release

READSTAT_PATCH := $(PROJ_DIR)patches/0001-readstat-pull-reader.patch

.PHONY: patch_readstat unpatch_readstat

patch_readstat:
	@if git -C third_party/readstat apply --reverse --check "$(READSTAT_PATCH)" >/dev/null 2>&1; then \
		echo "ReadStat pull-reader patch already applied"; \
	elif git -C third_party/readstat apply --check "$(READSTAT_PATCH)"; then \
		git -C third_party/readstat apply "$(READSTAT_PATCH)"; \
		echo "Applied ReadStat pull-reader patch"; \
	else \
		echo "ERROR: ReadStat patch does not apply; update patches/0001-readstat-pull-reader.patch" >&2; \
		exit 1; \
	fi

unpatch_readstat:
	git -C third_party/readstat apply --reverse "$(READSTAT_PATCH)"

clean: clean_build clean_cmake
clean_all: clean clean_configure
