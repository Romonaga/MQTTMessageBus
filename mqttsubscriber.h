#ifndef MQGETDATA_H
#define MQGETDATA_H



#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <sstream>
#include <cctype>
#include <thread>
#include <chrono>
#include "mqtt/async_client.h"
#include "dnrlogger.h"

//const std::string SERVER_ADDRESS("tcp://base1.local:1883");
//const std::string CLIENT_ID("async_subcribe_cpp");
//const std::string TOPIC("hello");

//const int	QOS = 1;
//const int	N_RETRY_ATTEMPTS = 5;

/////////////////////////////////////////////////////////////////////////////

// Callbacks for the success or failures of requested actions.
// This could be used to initiate further action, but here we just log the
// results to the console.
class callbackSub;
class action_listenerSub;


class  MQTTSubscriber
{
public:
    MQTTSubscriber(const std::string& address, const std::string& topic, int qos, int timeout, int numRetries = 10);
    ~MQTTSubscriber();
    bool start();
    bool stop();

    typedef void (*MQGetDataCallbackFunctionPtr)(void*, mqtt::const_message_ptr msg);

    void connectCallback(MQGetDataCallbackFunctionPtr cb, void *p);
    void setPersistance(const std::string& clientID, short qos);
    void setDebugLogging(bool value);

    bool areCallBacksEnabled();

    MQGetDataCallbackFunctionPtr _getDatacb;

    inline void sleep(int ms)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }


public:
    std::string _address;
    std::string _clientID;
    std::string _topic;
    void* _callee;

    int _qos;
    int _timeOut;
    int _numRetries;
    DNRLogger* _logger;
    callbackSub* _cb;
    mqtt::async_client* _client;
    action_listenerSub* _subListener;
    mqtt::connect_options* _connOpts;
};

class  action_listenerSub : public virtual mqtt::iaction_listener
{
    MQTTSubscriber* parent_;
    std::string name_;

    void on_failure(const mqtt::token& tok) override
    {
        std::stringstream info;

        info << "MQTTSubscriber " << name_ << " failure";
        if (tok.get_message_id() != 0)
            info << " for token: [" << tok.get_message_id() << "]";
        parent_->_logger->logCritical(info.str());
    }

    void on_success(const mqtt::token& tok) override
    {
        std::stringstream info;

        info << "MQTTSubscriber " << name_ << " success";
        if (tok.get_message_id() != 0)
            info << " for token: [" << tok.get_message_id() << "]";
        auto top = tok.get_topics();
        if (top && !top->empty())
            info << " token topic: [" << (*top)[0] << "]";
        parent_->_logger->logNotice(info.str());
    }

public:
    action_listenerSub(MQTTSubscriber* parent, const std::string& name) : parent_(parent), name_(name) {}
};

class  callbackSub : public virtual mqtt::callback,
                    public virtual mqtt::iaction_listener

{
    MQTTSubscriber* parent_;
    int nretry_;
    // Counter for the number of connection retries
    mqtt::async_client& cli_;
    // Options to use if we need to reconnect
    mqtt::connect_options& connOpts_;


    action_listenerSub& subListener_;


    // This deomonstrates manually reconnecting to the broker by calling
    // connect() again. This is a possibility for an application that keeps
    // a copy of it's original connect_options, or if the app wants to
    // reconnect with different options.
    // Another way this can be done manually, if using the same options, is
    // to just call the async_client::reconnect() method.
    void reconnect()
    {
        parent_->_logger->logInfo("MQTTSubscriber Attempting To Reconnect");

        std::this_thread::sleep_for(std::chrono::milliseconds(2500));
        try {
            cli_.connect(connOpts_, nullptr, *this);
        }
        catch (const mqtt::exception& exc)
        {
            std::stringstream info;
            info << "reconnect Error: " << exc.what();
            parent_->_logger->logCritical(info.str());

        }
    }

    // Re-connection failure
    void on_failure(const mqtt::token& tok) override
    {
        std::stringstream info;
        info << "MQTTSubscriber Connection attempt failed Try: " << ++nretry_ << " Out Of: " << parent_->_numRetries;
        parent_->_logger->logInfo(info.str());

        if (nretry_ > parent_->_numRetries)
        {
            parent_->_logger->logCritical("MQTTSubscriber Retries Exceeded.  Halting.");
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

        info << "MQTTSubscriber connected success: "
        << " Subscribing to topic: " << parent_->_topic <<
            " for clientID: " << parent_->_clientID <<
            " using QoS: " << parent_->_qos << " Cause: " << cause;
       parent_->_logger->logNotice(info.str());

        cli_.subscribe(parent_->_topic, parent_->_qos, nullptr, subListener_);
    }

    // Callback for when the connection is lost.
    // This will initiate the attempt to manually reconnect.
    void connection_lost(const std::string& cause) override
    {
        std::stringstream info;

        info << "MQTTSubscriber Connection lost";
        if (!cause.empty())
            info << "\tcause: " << cause;

        info << " Reconnecting...";
        parent_->_logger->logCritical(info.str());
        nretry_ = 0;
        reconnect();
    }

    // Callback for when a message arrives.
    void message_arrived(mqtt::const_message_ptr msg) override
    {

        if(parent_->_getDatacb)
        {
            parent_->_getDatacb(parent_->_callee, msg);
        }
        else
        {
            std::stringstream info;
            info << "MQTTSubscriber Message arrived" << std::endl;
            info << "\ttopic: '" << msg->get_topic() << "'" << std::endl;
            info << "\tpayload: '" << msg->to_string() << "'\n" << std::endl;
            parent_->_logger->logDebug(info.str());

        }
    }

    void delivery_complete(mqtt::delivery_token_ptr token) override {}

public:
    callbackSub(MQTTSubscriber* parent, mqtt::async_client& cli, mqtt::connect_options& connOpts, action_listenerSub& subListener )
                : parent_(parent), nretry_(0), cli_(cli), connOpts_(connOpts), subListener_(subListener) {}
};




#endif // MQGETDATA_H
