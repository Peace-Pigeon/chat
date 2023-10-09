#include "redis.hpp"
#include <iostream>

using namespace std;

Redis::Redis() : _publish_context(nullptr), _subscribe_context(nullptr){}

Redis::~Redis(){
    if (_publish_context != nullptr)
        redisFree(_publish_context);
    if (_subscribe_context != nullptr)
        redisFree(_subscribe_context);
}

// 连接redis服务器
bool Redis::connect(){
    // 负责publish消息的上下文连接
    _publish_context = redisConnect("127.0.0.1",6379);
    if(_publish_context == nullptr){
        cerr << "connect redis failed!" << endl;
        return false;
    }
    _subscribe_context=redisConnect("127.0.0.1",6379);
    if(_subscribe_context == nullptr){
        cerr << "connect redis failed!" << endl;
        return false;
    }

    // 在独立线程中完成监听通道的操作
    thread t([&](){
        observer_channel_message();
    });
    t.detach();
    cout<<"connect redis-server success!"<<endl;
    return true;
}

// 向redis指定的通道channel发布消息
bool Redis::publish(int channel, string message){
    redisReply *reply = (redisReply *)redisCommand(_publish_context, "PUBLISH %d %s", channel, message.c_str());
    if (reply == nullptr){
        cerr << "publish command failed!" << endl;
        return false;
    }
    freeReplyObject(reply);
    return true;
}

// 向redis指定的通道subscribe订阅消息, 这里只负责发送命令，不阻塞接收redis server响应消息
bool Redis::subscribe(int channel){
    // SUBSCRIBE命令本身会造成线程阻塞，直到有消息过来或者超时, 这里只做订阅操作，不接收通道中的消息
    // 通道消息的接收专门在observer_channel_message函数中的独立线程中进行
    // 只负责发送命令，不阻塞接收redis server响应消息, 否则和线程同步的机制就混乱了
    if(redisAppendCommand(_subscribe_context,"SUBSCRIBE %d",channel) != REDIS_OK){
        cerr << "subscribe command failed!" << endl;
        return false;
    }
    // redisBufferWrite可以循环发送缓冲区, 直到缓冲区中的数据全部发送完毕
    int done=0;
    while (!done)
    {
        if(redisBufferWrite(_subscribe_context,&done) == REDIS_ERR){
            cerr << "subscribe command failed!" << endl;
            return false;
        }
    }
    return true;
}

// 向redis指定的通道unsubscribe取消订阅消息
bool Redis::unsubscribe(int channel){
    if(redisAppendCommand(_subscribe_context,"UNSUBSCRIBE %d",channel) == REDIS_ERR){
        cerr << "unsubscribe command failed!" << endl;
        return false;
    }
    // redisBufferWrite可以循环发送缓冲区, 直到缓冲区中的数据全部发送完毕
    int done=0;
    while (!done){
        if(redisBufferWrite(_subscribe_context,&done) == REDIS_ERR){
            cerr << "unsubscribe command failed!" << endl;
            return false;
        }
    }
    return true;
}

// 在独立线程中接收订阅通道中的消息
void Redis::observer_channel_message(){
    redisReply *reply = nullptr;
    while(redisGetReply(this->_subscribe_context,(void **)&reply)==REDIS_OK){
        // 订阅收到的消息是一个带三元素的数组
        if(reply!=nullptr&&reply->element[2]!=nullptr&&reply->element[2]->str!=nullptr){
            // 给service层上报通道消息
            _notify_message_handler(atoi(reply->element[1]->str),reply->element[2]->str);
        }
        freeReplyObject(reply);
    }
    cerr << ">>>>>>>>>>>>>>>>>>>>>> observer_channel_message quit <<<<<<<<<<<<<<<<<<<<<<" << endl;
}

// 初始化向业务层上报通道消息的回调对象
void Redis::init_notify_handler(function<void(int, string)> fn){
    _notify_message_handler = fn;
}