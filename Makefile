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

configure: venv platform extension_version

debug: build_extension_library_debug build_extension_with_metadata_debug
release: build_extension_library_release build_extension_with_metadata_release

# build targets for profiling
reldebug: build_extension_library_reldebug build_extension_with_metadata_reldebug

build_extension_library_reldebug: check_configure
	mkdir -p build/reldebug
	EXTRA_CMAKE_FLAGS='-DCMAKE_EXTRA_BUILD_FLAGS=-fno-omit-frame-pointer'
	$(CMAKE_WRAPPER) cmake $(CMAKE_BUILD_FLAGS) -DCMAKE_BUILD_TYPE=RelWithDebInfo -S $(PROJ_DIR) -B cmake_build/reldebug $(EXTRA_CMAKE_FLAGS)
	cmake --build cmake_build/reldebug

	$(PYTHON_VENV_BIN) -c "from pathlib import Path;Path('$(EXTENSION_BUILD_PATH)/reldebug/extension/$(EXTENSION_NAME)').mkdir(parents=True, exist_ok=True)"
	$(PYTHON_VENV_BIN) -c "import shutil;shutil.copyfile('cmake_build/reldebug/$(EXTENSION_LIB_FILENAME)', '$(EXTENSION_BUILD_PATH)/reldebug/$(EXTENSION_LIB_FILENAME)')"

build_extension_with_metadata_reldebug: check_configure build_extension_library_reldebug
	$(PYTHON_VENV_BIN) extension-ci-tools/scripts/append_extension_metadata.py \
			-l $(EXTENSION_BUILD_PATH)/reldebug/$(EXTENSION_FILENAME_NO_METADATA) \
			-o $(EXTENSION_BUILD_PATH)/reldebug/$(EXTENSION_FILENAME) \
			-n $(EXTENSION_NAME) \
			-dv $(TARGET_DUCKDB_VERSION) \
			-evf configure/extension_version.txt \
			-pf configure/platform.txt $(UNSTABLE_C_API_FLAG)
	$(PYTHON_VENV_BIN) -c "import shutil;shutil.copyfile('$(EXTENSION_BUILD_PATH)/reldebug/$(EXTENSION_FILENAME)', '$(EXTENSION_BUILD_PATH)/reldebug/extension/$(EXTENSION_NAME)/$(EXTENSION_FILENAME)')"

test: test_debug
test_debug: test_extension_debug
test_release: test_extension_release

clean: clean_build clean_cmake
clean_all: clean clean_configure
