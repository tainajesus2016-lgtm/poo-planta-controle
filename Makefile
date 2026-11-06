PYTHON ?= python3
CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Werror -pedantic
ETAPA ?= $(shell cat ETAPA_ATUAL)
BUILD_DIR ?= build
CPP_SOURCES := $(shell find controlador -name '*.cpp' -type f)
CPP_HEADERS := $(shell find controlador -type f \( -name '*.hpp' -o -name '*.h' \))
CPP_OBJECTS := $(patsubst %.cpp,$(BUILD_DIR)/obj/%.o,$(CPP_SOURCES))
CPP_LIBRARY := $(filter-out $(BUILD_DIR)/obj/controlador/main.o,$(CPP_OBJECTS))
CPP_TESTS := $(shell find tests/cpp -name 'autorais_*.cpp' -type f 2>/dev/null)
CPP_EXECUTABLES := $(patsubst tests/cpp/%.cpp,$(BUILD_DIR)/autorais/%,$(CPP_TESTS))
.SECONDARY:

.PHONY: build test cobertura planta controlador supervisor
build: $(BUILD_DIR)/controlador $(CPP_EXECUTABLES)

$(BUILD_DIR)/controlador: $(CPP_OBJECTS)
	mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD_DIR)/obj/%.o: %.cpp $(CPP_HEADERS)
	mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Icontrolador -Icontrolador/include -MMD -MP -c $< -o $@

$(BUILD_DIR)/autorais/%: $(BUILD_DIR)/obj/tests/cpp/%.o $(CPP_LIBRARY)
	mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $^ -o $@

-include $(shell find $(BUILD_DIR)/obj -name '*.d' -type f 2>/dev/null)

test: build
	$(PYTHON) tools/testar.py --etapa $(ETAPA) --controlador $(BUILD_DIR)/controlador --cpp-tests $(BUILD_DIR)/autorais

cobertura:
	$(PYTHON) tools/cobertura.py --etapa $(ETAPA)

planta:
	$(PYTHON) -m planta.simulador --dados dados

controlador: build
	./$(BUILD_DIR)/controlador --dados dados --uma-vez

supervisor:
	$(PYTHON) -m streamlit run supervisor/app.py --browser.gatherUsageStats=false
