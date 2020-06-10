#-------------------------------------------------
#
# Project created by QtCreator 2016-07-22T14:13:34
#
#-------------------------------------------------

QT       -= gui
QMAKE_CXXFLAGS += -std=c++11

TARGET = MQTTMessageBus
TEMPLATE = lib

CONFIG += staticlib

DEFINES += MQTTMESSAGEBUS_LIBRARY

SOURCES += \
    mqttpublisher.cpp \
    mqttsubscriber.cpp

HEADERS +=\
    mqttpublisher.h \
    mqttsubscriber.h

#____________________________________________
#MQTT support
LIBS += -lpaho-mqttpp3 -lpaho-mqtt3a 
#____________________________________________



unix {
    target.path = /usr/local/lib
    INSTALLS += target
}


unix {
    header.path = /usr/local/include
    header.files = *.h*
    INSTALLS += header
}


