CXX= g++
CXXFLAGS= -std=c++17

INCLUDE= -I./include
LIB= -lSDL2 -lSDL2_image -lSDL2_ttf

SRCDIR= src
OBJDIR= obj
BINDIR= bin

OBJS= $(addprefix $(OBJDIR)/, sdl2_networkmon.o)
EXEC= $(addprefix $(BINDIR)/, networkmon)

# CREATE DIRECTORIES (IF DON'T ALREADY EXIST)
mkdirs:= $(shell mkdir -p $(OBJDIR) $(BINDIR))


# BUILD EVERYTHING
all: $(EXEC)

$(EXEC): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIB)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $< $(INCLUDE)


# REMOVE OLD FILES
clean:
	rm -f $(OBJS) $(EXEC)
