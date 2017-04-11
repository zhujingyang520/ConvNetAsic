##########################################
# General settings of compilation options
##########################################
# TODO: systemc installation path: modify it accordingly
SYSTEMC_ROOT := /home/jingyang/ProgramFiles/systemc-2.3.1a

##########################
# Preprocessor directives
##########################
# TODO: set the following variable if we want to compile the datapath
DATA_PATH := 1
# TODO: set the following variable if we want to compile with debug flag
DEBUG := 1


######################################
# !!No modification above this line!!
######################################
PROJECT := main
CXX := g++
CXXFLAGS := -Wall
LDFLAGS := -lprotobuf -lsystemc
INCDIR := -I. -I$(SYSTEMC_ROOT)/include
LIBDIR := -L$(wildcard $(SYSTEMC_ROOT)/lib-*)

########################
# Source code directory
########################
SRC_DIR := src

##########################################
# Google Protocol Buffer source directory
##########################################
PROTO_SRC_DIR := proto

##################
# Build directory
##################
BUILD_DIR := build
# All of the directories containing code: *.cpp & *.proto
SRC_DIRS := $(shell find * -type d -exec bash -c "find {} -maxdepth 1 \
	\( -name '*.cpp' -o -name '*.proto' \) | grep -q ." \; -print)
# Build directories to be generated
ALL_BUILD_DIRS := $(sort $(BUILD_DIR) $(addprefix $(BUILD_DIR)/, $(SRC_DIRS)))
# Protocol build directory
PROTO_BUILD_DIR := $(BUILD_DIR)/$(PROTO_SRC_DIR)

# tools to be compiled
TOOL_DIR := tools
TOOL_SRCS := $(shell find $(TOOL_DIR) -name "*.cpp")
TOOL_OBJS := $(addprefix $(BUILD_DIR)/, $(TOOL_SRCS:.cpp=.o))
TOOL_BINS := ${TOOL_OBJS:.o=.bin}

#######################
# Get all source files
#######################
CXX_SRCS := $(shell find $(SRC_DIR) -name "*.cpp")
PROTO_SRCS := $(wildcard $(PROTO_SRC_DIR)/*.proto)

###############
# Objects code
###############
CXX_OBJS := $(addprefix $(BUILD_DIR)/, $(CXX_SRCS:.cpp=.o))
PROTO_OBJS := $(addprefix $(BUILD_DIR)/, $(PROTO_SRCS:.proto=.pb.o))
OBJS := $(PROTO_OBJS) $(CXX_OBJS)

###############
# set CXXFLAGS
###############
ifeq ($(DATA_PATH), 1)
	CXXFLAGS += -DDATA_PATH
endif

ifeq ($(DEBUG), 1)
	CXXFLAGS += -DDEBUG -g -O0
else
	CXXFLAGS += -DNDEBUG -O2
endif

####################
# Compilation rules
####################
all:
	@echo "SYSTEMC_ROOT enviornment: $(SYSTEMC_ROOT)"
	make $(PROJECT)
	make tools

tools: $(TOOL_BINS)

$(TOOL_BINS): %.bin : %.o
	$(CXX) $(LIBDIR) -o $@ $< $(LDFLAGS)

$(PROJECT): $(OBJS)
	$(CXX) $(LIBDIR) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.o: %.cpp | $(ALL_BUILD_DIRS)
	$(CXX) $< $(CXXFLAGS) -c -o $@ $(INCDIR)

$(PROTO_BUILD_DIR)/%.pb.o: $(PROTO_SRC_DIR)/%.pb.cc $(PROTO_SRC_DIR)/%.pb.h \
	| $(ALL_BUILD_DIRS)
	$(CXX) $< $(CXXFLAGS) -c -o $@

# rules for protocol buffer
$(PROTO_SRC_DIR)/%.pb.cc $(PROTO_SRC_DIR)/%.pb.h: $(PROTO_SRC_DIR)/%.proto
	protoc --proto_path=$(PROTO_SRC_DIR) --cpp_out=$(PROTO_SRC_DIR) $<

$(ALL_BUILD_DIRS):
	mkdir -p $@

.PHONY: clean

clean:
	rm -fr $(PROJECT) $(BUILD_DIR)
	rm -fr DVEfiles
