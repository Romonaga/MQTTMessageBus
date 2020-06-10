#include "mqttsubscriber.h"
#include <sstream>
#include <stdio.h>
#include <stdlib.h>

MQTTSubscriber::MQTTSubscriber(const std::__cxx11::string &address, const std::__cxx11::string &topic, int qos, int timeout, int numRetries) :
    _address(address), _topic(topic), _qos(qos), _timeOut(timeout), _numRetries(numRetries)
{

    _logger = DNRLogger::instance();
    _getDatacb = NULL;
    _callee = NULL;
    _client = NULL;

    char buffer[32];
    srand(time(NULL));
    unsigned int clid = rand();
    sprintf(buffer,"%d",clid);
    _clientID = buffer;

}

MQTTSubscriber::~MQTTSubscriber()
{
    stop();

}

void MQTTSubscriber::setDebugLogging(bool value)
{
    _logger->setDebugOut(value);
}

bool MQTTSubscriber::areCallBacksEnabled()
{
    return (NULL != _cb);
}

void MQTTSubscriber::connectCallback(MQGetDataCallbackFunctionPtr cb, void *p)
{
    _getDatacb = cb;  //CallBack
    _callee = p; //clienpointer
}

void MQTTSubscriber::setPersistance(const std::string& clientID, short qos)
{
    if(_client == NULL)
    {
        _clientID = clientID;
        _qos = qos;
    }
    else
    {
        _logger->logNotice("MQTTSubscriber::setPersistance Needs to be called before Start");
    }

}

bool MQTTSubscriber::start()
{
    try
    {

        if(_client == NULL)
        {
            _subListener = new action_listener(this, "MQTTSubscriber");
            _client = new mqtt::async_client(_address, _clientID);


            if(_client)
            {
                mqtt::connect_options connOpts;
                connOpts.set_keep_alive_interval(20);
                connOpts.set_clean_session((_qos == 0));

                _cb = new callback(this, *_client, connOpts, *_subListener);
                _client->set_callback(*_cb);

                _logger->logInfo("MQTTSubscriber::Start Connecting to the MQTT server");
                _client->connect(connOpts, nullptr, *_cb);
            }
            else
            {
                _logger->logInfo("MQTTSubscriber::Start No MQQ Client Halting");
                return false;

            }
        }
    }
    catch (const mqtt::exception&)
    {
        std::stringstream info;
        info << "MQTTSubscriber::Start Unable to connect to MQTT server: '"
            << _address;
        _logger->logCritical(info.str());
        return false;
    }

    return true;
}

bool MQTTSubscriber::stop()
{
    std::stringstream info;
    try
    {
        if(_client)
        {
            info << "MQTTSubscriber::Stop Disconnecting from the MQTT server";
            _logger->logInfo(info.str());

            //One oddity, you can not unsubscribe if you have durribale consumers.
            //You need to NOT unsubscribe.  So... If QOS is anything BUT ZERO
            //I will not Unsubscribe!
            if(_qos == 0)
            {
                info.str("");
                info << "MQTTSubscriber::Stop QOS is 0 Unsubscribing To: " << _topic;
                _logger->logNotice(info.str());
                _client->unsubscribe(_topic);
            }
            else
            {
                info.str("");
                info << "MQTTSubscriber::Stop QOS is Not 0 Staying Subscribed Topic: " << _topic;
                _logger->logNotice(info.str());

            }
            _client->disconnect()->wait();
            _logger->logInfo("MQTTSubscriber::Stop Disconnected from the MQTT server");
            delete _subListener;
            delete _client;
            delete _cb;

            _client = NULL;

            return true;
        }

    }
    catch (const mqtt::exception& exc)
    {
        info.str("");
        info << "MQTTSubscriber::Stop Exception: " << exc.what();
        _logger->logCritical(info.str());
        return false;

    }

    return true;
}
