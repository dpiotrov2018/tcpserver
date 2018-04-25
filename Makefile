################################################################################
#
#  CTcpServer class test makefile
#
#  Author      : Dmitry Piotrovsky for Percepto
#
################################################################################

CXX := g++
CXXFLAGS := -std=c++11 -O3 -Wall -c -fmessage-length=0
LIBS := -lpthread

SRCDIR := ./src
OBJDIR := ./tmp

TARGET := tcpserver

CPP_SRCS += \
$(SRCDIR)/CTcpServer.cpp \
$(SRCDIR)/test_server.cpp 

OBJS += \
$(OBJDIR)/CTcpServer.o \
$(OBJDIR)/test_server.o 

CPP_DEPS += \
$(OBJDIR)/CTcpServer.d \
$(OBJDIR)/test_server.d 


all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) -o "$@" $(OBJS) $(LIBS)

$(OBJDIR)/%.o: ./src/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"

clean:
	rm -rf $(OBJDIR) ./$(TARGET)
