# Top-level build for Scope tools (sampler, filter, scope).
# Builds only the clockwork client library (cw_client), not the full clockwork tree.

BUILD_DIR ?= build
CMAKE ?= cmake
CMAKE_BUILD_TYPE ?= Debug

.PHONY: all configure build sampler filter scope convert_date clean

all: configure
	$(MAKE) -C $(BUILD_DIR)
	cp -f $(BUILD_DIR)/Sampler sampler
	cp -f $(BUILD_DIR)/Filter filter
	cp -f $(BUILD_DIR)/Scope scope

configure: $(BUILD_DIR)/Makefile

$(BUILD_DIR)/Makefile: CMakeLists.txt ScopeConfig.h.in cmake/Modules/FindZeroMQ.cmake
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && $(CMAKE) -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) ..

# Rebuild everything already configured
build: $(BUILD_DIR)/Makefile
	$(MAKE) -C $(BUILD_DIR)

sampler: $(BUILD_DIR)/Makefile
	$(MAKE) -C $(BUILD_DIR) Sampler
	cp -f $(BUILD_DIR)/Sampler sampler

filter: $(BUILD_DIR)/Makefile
	$(MAKE) -C $(BUILD_DIR) Filter
	cp -f $(BUILD_DIR)/Filter filter

scope: $(BUILD_DIR)/Makefile
	$(MAKE) -C $(BUILD_DIR) Scope
	cp -f $(BUILD_DIR)/Scope scope

convert_date: $(BUILD_DIR)/Makefile
	$(MAKE) -C $(BUILD_DIR) convert_date

style:
	astyle --options=.astylerc src/*.cpp src/*.h

clean:
	rm -f sampler filter scope
	rm -rf $(BUILD_DIR)
