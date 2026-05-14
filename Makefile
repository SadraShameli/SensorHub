LLVM_PREFIX  := $(shell brew --prefix llvm 2>/dev/null)
CLANG_FORMAT := $(LLVM_PREFIX)/bin/clang-format
PIO          := $(HOME)/.platformio/penv/bin/pio

CPP_FILES    := $(shell find src test/test_native -name "*.cpp")
H_FILES      := $(shell find include lib/sensorhub_core/include -name "*.h")
ALL_SOURCES  := $(CPP_FILES) $(H_FILES)

.PHONY: format
format:
	$(CLANG_FORMAT) -i $(ALL_SOURCES)

.PHONY: compiledb
compiledb:
	$(PIO) run -t compiledb -e debug

.PHONY: check
check:
	$(PIO) check -e debug

.PHONY: clean
clean:
	git clean -Xdf

.PHONY: help
help:
	@echo "Available targets:"
	@echo "  format     Format source files with clang-format"
	@echo "  compiledb  Regenerate compile_commands.json for IDE integration"
	@echo "  check      Run PlatformIO static analysis (cppcheck by default)"
	@echo "  clean      Remove untracked/ignored files"
	@echo "  help       Show this help message"

%:
	@:
