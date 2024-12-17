.PHONE: format
format:
	clang-format -i */*.*

.PHONY: clean
clean:
	git clean -Xdf


.PHONY: help
help:
	@echo Available targets:
	@echo   format   Format the project using clang-format
	@echo   help     Show this help message
	@echo   clean    Clean the project directory


%:
	@:
