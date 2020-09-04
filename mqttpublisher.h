#ifndef DEVICEMQTTLAYER_H
#define DEVICEMQTTLAYER_H

#include <iostream>
#include <sstream>
#include <syslog.h>
#include <string>
#include <cstring>
#include "dnrlogger.h"

#include "mqtt/async_client.h"

class callbackPub;
class action_listenerPub;



class MQTTPublisher
{

public:
    explicit MQTTPublisher(const std::string& address, const std::string& topic, int qos, int timeout, short numRetrys = 20);
    explicit MQTTPublisher(const std::string& address, const std::string& topic, int qos, int timeout,  const std::string& clientID, short numRetrys = 20);
    ~MQTTPublisher();

    void Stop();
    bool Start();
    bool SendMessage(const std::string& msg);
    bool SendMessage(void *packet, int size);

    void setDebugLogging(bool value);

private:
    void init();

    bool checkConnection();

    inline void sleep(int ms)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }

public:
    std::string _address;
    std::string _topic;
    int _qos;
    int _timeout;
    std::string _clientId;
    int _numRetrys;

    mqtt::async_client* _client;
    callbackPub* _cb;
    action_listenerPub* _actionListener;
     mqtt::connect_options* _connOpts;

    DNRLogger* _logger;


};

class  action_listenerPub : public virtual mqtt::iaction_listener
{
    MQTTPublisher* parent_;
    std::string name_;

    void on_failure(const mqtt::token& tok) override
    {
        std::stringstream info;

        info << name_ << "MQTTPublisher failure";
        if (tok.get_message_id() != 0)
            info << " for token: [" << tok.get_message_id() << "]";
        parent_->_logger->logCritical(info.str());
    }

    void on_success(const mqtt::token& tok) override
    {
        std::stringstream info;

        info << name_ << "MQTTPublisher success";
        if (tok.get_message_id() != 0)
            info << " for token: [" << tok.get_message_id() << "]";
        auto top = tok.get_topics();
        if (top && !top->empty())
            info << " token topic: [" << (*top)[0] << "]";
        parent_->_logger->logNotice(info.str());
    }

public:
    action_listenerPub(MQTTPublisher* parent, const std::string& name) : parent_(parent), name_(name) {}
};

class  callbackPub : public virtual mqtt::callback,
                    public virtual mqtt::iaction_listener

{
    MQTTPublisher* parent_;
    int nretry_;
    // Counter for the number of connection retries
    mqtt::async_client& cli_;
    // Options to use if we need to reconnect
    mqtt::connect_options& connOpts_;


    action_listenerPub& subListener_;


    // This deomonstrates manually reconnecting to the broker by calling
    // connect() again. This is a possibility for an application that keeps
    // a copy of it's original connect_options, or if the app wants to
    // reconnect with different options.
    // Another way this can be done manually, if using the same options, is
    // to just call the async_client::reconnect() method.
    void reconnect()
    {
        parent_->_logger->logInfo("Attempting To Reconnect");

        std::this_thread::sleep_for(std::chrono::milliseconds(2500));
        try {
            cli_.reconnect()->wait_for(std::chrono::milliseconds(2500));// .connect(connOpts_, nullptr, *this);

        }
        catch (const mqtt::exception& exc)
        {
            std::stringstream info;
            info << "MQTTPublisher reconnect Error: " << exc.what();
            parent_->_logger->logCritical(info.str());

        }
    }

    // Re-connection failure
    void on_failure(const mqtt::token& tok) override
    {
        std::stringstream info;
        info << "MQTTPublisher Connection failed";
        parent_->_logger->logCritical(info.str());

        if (++nretry_ > parent_->_numRetrys)
        {
            parent_->_logger->logCritical("Retries Exceeded.  Halting.");

            return;
        }
        reconnect();

    }

    // (Re)connection success
    // Either this or connected() can be used for callbacks.
    void on_success(const mqtt::token& tok) override {}

    // (Re)connection success
    void connected(const std::string& cause) override
    {
        std::stringstream info;

        info << "MQTTPublisher connected success: "
        << " Publishing to topic: " << parent_->_topic <<
            " for clientID: " << parent_->_clientId <<
            " using QoS: " << parent_->_qos << " Cause: " << cause;
       parent_->_logger->logNotice(info.str());

    }

    // Callback for when the connection is lost.
    // This will initiate the attempt to manually reconnect.
    void connection_lost(const std::string& cause) override
    {
        std::stringstream info;

        info << "MQTTPublisher Connection lost";
        if (!cause.empty())
            info << "\tcause: " << cause;

        parent_->_logger->logCritical(info.str());
        nretry_ = 0;
        //reconnect();
    }

    // Callback for when a message arrives. //should no thappen on publisher
    void message_arrived(mqtt::const_message_ptr msg) override
    {


    }

    void delivery_complete(mqtt::delivery_token_ptr token) override {}

public:
    callbackPub(MQTTPublisher* parent, mqtt::async_client& cli, mqtt::connect_options& connOpts, action_listenerPub& subListener )
                : parent_(parent), nretry_(0), cli_(cli), connOpts_(connOpts), subListener_(subListener) {}
};



#endif // DEVICEMQTTLAYER_H
