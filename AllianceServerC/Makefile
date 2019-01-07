CC:= g++

CCOPT:=#-g -Wall -Wno-unknown-pragmas --std=c++0x -Werror -Wno-deprecated --std=c++0x

CURRENT_PATH:=$(shell pwd)

SRC_PATH:=$(CURRENT_PATH)/src

lib=$(CURRENT_PATH)/../lib

MYINCLUDE = -I$(lib)/XConfig/include -I/home/gumy/boost_1_67_0/ -I$(lib)/libXGame/include  -I$(lib)/util/include -I$(lib)/recastnavigation/Recast/Include -I$(lib)/recastnavigation/Detour/Include -I$(lib)/libXFindPath/include/ -I$(lib)/XGameData/include  -I$(lib)/xCommand  -I$(lib)/libXServer/include -I$(lib)/libXNet/include  -I$(lib)/libXBase/include -g -Wall -Wno-unknown-pragmas --std=c++0x -Wno-deprecated --std=c++0x 

MYLIB = -L/usr/local/mysql5.6/lib -L$(lib)/libXServer -lXServer -L$(lib)/libXNet -lXNet -L$(lib)/libXBase/ -lXBase
# -L$(lib)/XConfig/ -lXConfig -L$(lib)/XGameData/ -lXGameData -L$(lib)/libXGame -lXGame -L$(lib)/libXFindPath/ -lXFindPath -L$(lib)/recastnavigation/Detour/ -L$(lib)/recastnavigation/Recast/ -lDetour -lRecast -L$(lib)/util/ -lUtil 

INC_PATH:= -I/home/gumy/boost_1_67_0/ -I$(SRC_PATH) -I$(SRC_PATH)/User -I$(SRC_PATH)/Npc -I$(SRC_PATH)/PackEquipObj -I$(SRC_PATH)/Scene -I$(SRC_PATH)/SceneBase -I$(SRC_PATH)/Skill -I$(SRC_PATH)/SubSystem ${MYINCLUDE} -I/usr/local/include/libxml2 -I/usr/local/mysql5.6/include/ -D_DEBUG -D_ALL_SUPER_GM $(DEBUG_FLAG) 

LIB_PATH:= ${MYLIB} -L/usr/lib64/ -L/usr/local/mysql5.6/lib -lxml2 -lpthread -lmysqlclient -llog4cxx  -lrt -ldl



OBJS_PATH:=$(CURRENT_PATH)/obj

SRC_FILE_DIR:=$(wildcard $(SRC_PATH)/*.cpp)

SRC_FILE:=$(notdir $(SRC_FILE_DIR))

OBJS_FILE:=$(patsubst %.cpp,%.o,$(SRC_FILE))

OBJS:=$(patsubst %.o,$(OBJS_PATH)/%.o,$(OBJS_FILE))

define walk
	$(wildcard $(1)) $(foreach e, $(wildcard $(1)/*), $(call walk, $(e)))
endef
OUTOBJS= $(SRC_PATH)/src/%.cpp
BINOBJS =$(patsubst %.cpp,$(OBJS_PATH)/%.o,$(notdir $(filter-out $(OUTOBJS),$(filter %.cpp, $(call walk, $(SRC_PATH))))))

SUBDIRS= 

TARGET:=AllianceServerC

.PHONY : all

all: handle debug	
	@echo "--------- 警告: 当前为调试版本 ---------"
debug: $(TARGET)	

handle:
	@for dir in $(SUBDIRS);\
		do \
			CCOPT='$(CCOPT)' SRCPATH=$(SRC_PATH)/$$dir OBJPATH=$(OBJS_PATH) CXX=$(CC) DIR=$$dir CXXLFLAGS='$(LIB_PATH)' CXXFLAGS='$(INC_PATH)' $(MAKE) -C $(SRC_PATH)/$$dir || exit 1; \
		done
		
$(TARGET) : $(OBJS)	
	$(CC) -rdynamic -o $(TARGET) $(BINOBJS) $(LIB_PATH)
$(OBJS_PATH)/%.o:$(SRC_PATH)/%.cpp
	$(CC) $(CCOPT) $(INC_PATH) -c $< -o $@
	
.PHONY:clean

clean: 
	rm -rf $(TARGET)
	rm -rf $(OBJS)
	@for dir in $(SUBDIRS);\
		do \
			SRCPATH=$(SRC_PATH)/$$dir OBJPATH=$(OBJS_PATH) CXX=$(CC) DIR=$$dir $(MAKE) clean -C $(SRC_PATH)/$$dir; \
		done
