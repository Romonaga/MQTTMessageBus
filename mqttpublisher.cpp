#include "mqttpublisher.h"



/*
const std::string ADDRESS("tcp://VengNas.local:1883");
const std::string CLIENTID("AsyncPublisher");
const std::string TOPIC("weatherstation");

const int  QOS = 1;
const long TIMEOUT = 10000L;
 *
 * */

MQTTPublisher::MQTTPublisher(const std::string& address, const std::string& topic, int qos, int timeout,  const std::string& clientID, short numRetrys) :
        _address(address),  _topic(topic), _qos(qos), _timeout(timeout), _clientId(clientID), _numRetrys(numRetrys)
{
    init();

}

MQTTPublisher::MQTTPublisher(const std::string& address, const std::string& topic, int qos, int timeout, short numRetrys) :
    _address(address),  _topic(topic), _qos(qos), _timeout(timeout), _numRetrys(numRetrys)

{

    char buffer[32];
    srand(time(NULL));
    unsigned int clid = rand();
    sprintf(buffer,"%d",clid);
    _clientId = buffer;
    init();

}

void MQTTPublisher::init()
{

    _logger = DNRLogger::instance();
    _client = new mqtt::async_client(_address, _clientId);
    _actionListener = new action_listener(this, "MQTTPublisher");
    mqtt::connect_options connOpts;
    _connOpts = new mqtt::connect_options;
    _connOpts->set_keep_alive_interval(20);
    _connOpts->set_automatic_reconnect(true);
    _connOpts->set_connect_timeout(_timeout);

    _cb = new callback(this, *_client, *_connOpts, *_actionListener);
    _client->set_callback(*_cb);

}

void MQTTPublisher::setDebugLogging(bool value)
{
    _logger->setDebugOut(value);
}

MQTTPublisher::~MQTTPublisher()
{

    Stop();

}

void MQTTPublisher::Stop()
{
    try
    {
        if(_client != nullptr)
        {
            _logger->logInfo("MQTTPublisher Stopping");

            if(_client->is_connected() == true)
            {
                std::vector<mqtt::delivery_token_ptr> toks = _client->get_pending_delivery_tokens();
                while(!toks.empty())
                    ;
                // Disconnect
                mqtt::token_ptr conntok = _client->disconnect();
                conntok->wait();

                delete _client;
                delete _cb;
                delete _connOpts ;

            }

            _logger->logInfo("MQTTPublisher Stopped");

        }
    }
    catch (const mqtt::exception& exc)
    {
        std::stringstream msg;

        msg << "MQTTPublisher Stop() Error: " << exc.what();
        _logger->logCritical(msg.str());
    }
}

bool MQTTPublisher::Start()
{
    try
    {
        /*mqtt::connect_options conopts;
        mqtt::message willmsg("hello", "Last will and testament.", 1, true);
        mqtt::will_options will(willmsg);
        conopts.set_will(will);
*/
        _logger->logInfo("MQTTPublisher Starting");
        if (_client->is_connected() == false)
        {

            mqtt::token_ptr conntok = _client->connect();
            conntok->wait();
            _logger->logInfo("MQTTPublisher Started");
        }
        else
        {
            _logger->logInfo("MQTTPublisher Already Running");
        }

    }
    catch (const mqtt::exception& exc)
    {
        std::stringstream msg;
        msg << "MQTTPublisher Start() Error: " << exc.what();
        _logger->logCritical(msg.str());
        return false;
    }

    return true;
}

bool MQTTPublisher::checkConnection()
{
    short retries = 0;
    std::stringstream info;
    bool retVal = false;

    while( (retVal = _client->is_connected()) == false && retries < _numRetrys)
    {
        retries++;
        unsigned int ms = retries * 200;

        info.str("");
        info << "MQTTPublisher::checkConnection: Connection is down, attempting reconnection: Attempt: " << retries << " out of: " << _numRetrys << " Waiting MS: " << ms;
        _logger->logNotice(info.str());

        sleep(ms);
        Start();

    }

    return retVal;
}

bool MQTTPublisher::SendMessage(void* packet, int size)
{
    bool retVal = false;
    try
    {
        if(checkConnection() == true)
        {
            mqtt::message_ptr pubmsg = mqtt::make_message(_topic, packet, size, _qos, false);
            pubmsg->set_qos(_qos);
            retVal =  _client->publish(pubmsg)->wait_for(std::chrono::milliseconds(_timeout));
        }
        else
        {
            _logger->logCritical("MQTTPublisher::SendMessage Unable To Connect To Broker.  Is it Down?  Retry Attempts failed.");
        }

    }
    catch (const mqtt::exception& exc)
    {
        std::stringstream msg;
        msg << "MQTTPublisher SendMessage() Error: " << exc.what();
        _logger->logCritical(msg.str());

    }

    return retVal;
}

bool MQTTPublisher::SendMessage(const std::string& msg)
{
    return SendMessage((void*)msg.c_str(), msg.length());
}



