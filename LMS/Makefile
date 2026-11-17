CXX := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic -O2 -Iinclude
SOURCES := src/main.cpp src/library.cpp src/security.cpp src/storage.cpp
CORE := src/library.cpp src/security.cpp src/storage.cpp
APP := build/library_app
TEST := build/library_tests

.PHONY: all run test smoke coverage valgrind helgrind clean
all: $(APP)

$(APP): $(SOURCES) include/library.hpp
	@mkdir -p build data
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(APP)

run: all
	./$(APP)

$(TEST): tests/test_library.cpp $(CORE) include/library.hpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) tests/test_library.cpp $(CORE) -o $(TEST) -lgtest_main -lgtest -pthread

test: $(TEST)
	./$(TEST)

smoke: $(APP)
	sh tests/smoke.sh $(APP)

coverage:
	@mkdir -p build/coverage
	$(CXX) -std=c++20 -O0 -g --coverage -Iinclude tests/test_library.cpp $(CORE) -o build/coverage/library_tests -lgtest_main -lgtest -pthread
	./build/coverage/library_tests
	@cd build/coverage && gcov -b -c *.gcno
	@if command -v lcov >/dev/null 2>&1 && command -v genhtml >/dev/null 2>&1; then \
		lcov --capture --directory build/coverage --output-file build/coverage/coverage.info --ignore-errors mismatch,source && \
		lcov --remove build/coverage/coverage.info '/usr/*' '*/tests/*' --output-file build/coverage/coverage.info && \
		genhtml build/coverage/coverage.info --output-directory build/coverage/html; \
	else echo 'Install lcov for HTML coverage; gcov output is in build/coverage.'; fi

valgrind: $(TEST)
	valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all --track-origins=yes --error-exitcode=1 ./$(TEST)

helgrind: $(TEST)
	valgrind --tool=helgrind --error-exitcode=1 ./$(TEST)

clean:
	rm -rf build
