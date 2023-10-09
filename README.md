# 项目介绍

## 项目实现

本项目主要实现了一个聊天室的功能(含有客户端与服务器端,均无图形化界面), 包括了用户的注册, 登录, 退出, 以及聊天室的创建, 加入, 退出, 以及聊天室的消息发送, 接收等功能.

## 项目的环境依赖

**数据库:** 主要采用了MySQL与Redis数据库.

**编译环境:** 本项目采用了C++11标准, 使用了CMake进行编译.

**第三方依赖:** 第三方库主要有两个, 一个是nlohmann编写的json库, 这里已经在项目中添加, 另一个是陈硕写的muduo库, 这里没有添加, 需要自行下载.

*注意:* muduo需要依赖boost库, 请自行下载boost库.

**分布式:** 采用了nginx实现负载均衡, 需要下载相关软件与nginx相关的依赖库.

## MySQL数据库表格

User表

|字段名称|字段类型|字段说明|约束|
|--|--|--|--|
|id|INT|用户id|PRIMARY KEY、AUTO_INCREMENT|
|name|VARCHAR(50)|用户名|NOT NULL, UNIQUE|
|password|VARCHAR(50)|用户密码|NOT NULL|
|state|ENUM('online', 'offline')|当前登录状态|DEFAULT 'offline'|

Friend表

|字段名称|字段类型|字段说明|约束|
|--|--|--|--|
|userid|INT|用户id|NOT NULL、联合主键|
|friendid|INT|好友id|NOT NULL、联合主键|

AllGroup表

|字段名称|字段类型|字段说明|约束|
|--|--|--|--|
|id|INT|组id|PRIMARY KEY、AUTO_INCREMENT|
|groupname|VARCHAR(50)|组名|NOT NULL, UNIQUE|
|groupdesc|VARCHAR(50)|组功能描述|DEFAULT ''|

GroupUser表

|字段名称|字段类型|字段说明|约束|
|--|--|--|--|
|groupid|INT|组id|NOT NULL、联合主键|
|userid|INT|用户id|NOT NULL、联合主键|
|grouprole|ENUM('creator', 'normal')|用户在组中的角色|DEFAULT 'normal'|

OfflineMessage表

|字段名称|字段类型|字段说明|约束|
|--|--|--|--|
|userid|INT|用户id|NOT NULL|
|message|VARCHAR(50)|离线消息（存储Json字符串）|NOT NULL|


## 项目文件结构

+ bin: 主要用来存放编译后的可执行文件
+ include: 主要用来存放头文件, 这里包含了server, client和共用的头文件
+ src: 主要用来存放源文件, 这里包含了server, client文件
+ thirdparty: 主要用来存放第三方库, 这里包含了json库
+ chat.sql: 项目的数据库文件
+ CMakeLists.txt: 项目的CMake文件
+ README.md: 项目的说明文件


## 运行项目

1. 在MySQL数据中创建好数据库, 名称为chat, 并且创建好上述的表格, 修改src/server/db/db.h中的数据库的用户名和密码.
2. 若redis端口号不是默认的6379, 则需要修改src/server/redis/redis.cpp中的redis端口号.
3. 更改安装nginx的配置文件(默认安装后路径为`/usr/local/nginx/conf/`), 并且将nginx.conf文件中添加如下内容:

```nginx
# nginx tcp loadbalance config
stream {
    upstream MyServer {
        server 127.0.0.1:6000 weight=1 max_fails=3 fail_timeout=30s;
        server 127.0.0.1:6002 weight=1 max_fails=3 fail_timeout=30s;
    }
    server {
        proxy_connect_timeout 1s;
        listen 8000;
        proxy_pass MyServer;
        tcp_nodelay on;
    }
}
```

*注意:* 这里课根据自己的业务作出修改, 我这里默认启用两个服务器, 一个端口号为6000, 另一个端口号为6002.

4. 创建build文件夹, 并且在build文件夹中执行`cmake ..`和`make`命令, 生成可执行文件.
5. 在bin文件夹中执行`./ChatServer 127.0.0.1 6000`和`./ChatServer 127.0.0.1 6002`命令, 启动服务器.
6. 在bin文件夹中执行`./ChatClient 127.0.0.1 8000`, 启动客户端.