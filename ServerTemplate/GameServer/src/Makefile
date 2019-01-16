
OBJS_FILE:=$(patsubst %.cpp,%.o,$(notdir $(wildcard $(SRCPATH)/*.cpp)))
OBJS:=$(patsubst %.o,$(OBJPATH)/%.o,$(OBJS_FILE))


all:$(OBJS)    

$(OBJPATH)/%.o:$(SRCPATH)/%.cpp
    $(CXX)  $(CXXFLAGS) $(CCOPT) -c $< -o $@        

clean:
    rm -rf $(OBJS)
