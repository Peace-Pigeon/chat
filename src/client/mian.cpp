#include "json.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <chrono>
#include <ctime>
#include <unordered_map>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "group.hpp"
#include "user.hpp"
#include "public.hpp"

using namespace std;
using json=nlohmann::json;

// 记录当前系统登录的用户信息
User g_currentUser;

// 记录当前登录用户的好友列表信息
vector<User> g_currentUserFriendList;

// 记录当前登录用户的群组列表信息
vector<Group> g_currentUserGroupList;

// 显示当前登录成功用户的基本信息
void showCurrentUserData();

// 控制主菜单页面程序
bool isMainMenuRunning=false;

// 接收线程
void readTaskHandler(int clientfd);

// 获取系统时间
string getCurrentTime();

// 聊天页面程序
void mainMenu(int);

// help command handler
void help(int,string);

// chat command handler
void chat(int fd,string str);

// addfriend command handler
void addfriend(int fd,string str);

// creategroup command handler
void creategroup(int fd,string str);

// addgroup command handler
void addgroup(int fd,string str);

// groupchat command handler
void groupchat(int fd,string str);

// logout command handler
void logout(int fd,string str);

int main(int argc,char** argv){
    if(argc<3){
        cerr<<"command invalid! example: ./ChatClient 127.0.0.1 6000"<<endl;
        exit(-1);
    }

    // 解析通过命令行参数传递的IP和port
    char *ip=argv[1];
    uint16_t port=atoi(argv[2]);

    // 创建客户端socket
    int clientfd=socket(AF_INET,SOCK_STREAM,0);
    if(clientfd==-1){
        cerr<<"socket create error!"<<endl;
        exit(-1);
    }

    // 填写client需要连接的server信息ip和port
    struct sockaddr_in server;
    memset(&server,0,sizeof(server));

    server.sin_family=AF_INET;
    server.sin_port=htons(port);
    server.sin_addr.s_addr=inet_addr(ip);

    // client和server进行连接
    if(connect(clientfd,(struct sockaddr*)&server,sizeof(server))==-1){
        cerr<<"connect error!"<<endl;
        close(clientfd);
        exit(-1);
    }
    
    // main线程用于接收用户输入, 负责发送数据
    while(true){
        // 显示首页
        cout<<"====================="<<endl;
        cout<<"1. login"<<endl;
        cout<<"2. regidter"<<endl;
        cout<<"3. qiut"<<endl;
        cout<<"====================="<<endl;
        cout<<"choice:";
        int choice=0;
        cin>>choice;
        cin.get();  // 读掉缓冲区残留的回车

        switch (choice)
        {
        case 1: // login
        {
            int id=0;
            char pwd[50]={0};
            cout<<"userid:";
            cin>>id;
            cin.get();
            cout<<"user password:";
            cin.getline(pwd,50);

            json js;
            js["msgid"]=LOGIN_MSG;
            js["id"]=id;
            js["password"]=pwd;

            string request=js.dump();
            cout<<"request:"<<request<<endl;

            int len=send(clientfd,request.c_str(),strlen(request.c_str())+1,0);
            if(len==-1) cerr<<"send reg msg error:"<<request<<endl;
            else{
                char buffer[1024]={0};
                len=recv(clientfd,buffer,1024,0);
                if(len==-1) cerr<<"recv login msg error"<<endl;
                else{
                    json responsejs=json::parse(buffer);
                    if(responsejs["errno"].get<int>()!=0){  // 登录失败
                        cerr<<"登录失败: "<<responsejs["errmsg"]<<endl;
                    }else{  // 登陆成功
                        g_currentUser.setId(responsejs["id"].get<int>());
                        g_currentUser.setName(responsejs["name"]);
                        // 记录当前用户的好友列表信息
                        if(responsejs.contains("friends")){
                            // 初始化
                            g_currentUserFriendList.clear();

                            vector<string> vec=responsejs["friends"];
                            for(string &str:vec){
                                json js=json::parse(str);
                                User user;
                                user.setId(js["id"].get<int>());
                                user.setName(js["name"]);
                                user.setState(js["state"]);
                                g_currentUserFriendList.push_back(user);
                            }
                        }
                        // 记录当前用户的群组列表信息
                        if(responsejs.contains("gourps")){
                            // 初始化
                            g_currentUserGroupList.clear();

                            vector<string> vec1=responsejs["gourps"];
                            for(string &groupstr:vec1){
                                json grpjs=json::parse(groupstr);
                                Group group;
                                group.setId(grpjs["id"].get<int>());
                                group.setName(grpjs["groupname"]);
                                group.setDesc(grpjs["groupdesc"]);

                                vector<string> vec2=grpjs["users"];
                                for(string &userstr:vec2){
                                    GroupUser user;
                                    json js=json::parse(userstr);
                                    user.setId(js["id"].get<int>());
                                    user.setName(js["name"]);
                                    user.setState(js["state"]);
                                    user.setRole(js["role"]);
                                    group.getUsers().push_back(user);
                                }
                                g_currentUserGroupList.push_back(group);
                            }
                        }
                        // 显示登录用户的基本信息
                        showCurrentUserData();

                        // 显示当前用户的离线消息 个人聊天信息或者群组消息
                        if(responsejs.contains("offlinemsg")){
                            vector<string> vec=responsejs["offlinemsg"];
                            for(string &str:vec){
                                json js=json::parse(str);
                                // 个人聊天消息
                                if(js["msgid"].get<int>()==ONE_CHAT_MSG){
                                    cout<<js["time"].get<string>()<<"["<<js["id"]<<"]"<<js["name"].get<string>()<<" said:"<<js["msg"].get<string>()<<endl;
                                    continue;
                                }
                                // 群组聊天消息
                                else{
                                    cout<<"群消息["<<js["groupid"]<<"]"<<js["time"]<<"["<<js["id"]<<"]"<<js["name"]<<" said:"<<js["msg"]<<endl;
                                    continue;
                                }
                            }
                        }
                        // 登录成功, 启动接收线程负责接收数据, 该线程只启动一次
                        static int readthreadnumber=0;
                        if(readthreadnumber==0){
                            std::thread readTask(readTaskHandler,clientfd);
                            readTask.detach();  // 线程分离
                            readthreadnumber++;
                        }
                        // 进入聊天主菜单页面
                        isMainMenuRunning=true;
                        mainMenu(clientfd);
                    }
                }
            }
        }
            break;
        case 2: // register业务
        {
            char name[50]={0};
            char pwd[50]={0};
            cout<<"username:";
            cin.getline(name,50);
            cout<<"user password:";
            cin.getline(pwd,50);

            json js;
            js["msgid"]=REG_MSG;
            js["name"]=name;
            js["password"]=pwd;

            string request=js.dump();

            int len=send(clientfd,request.c_str(),strlen(request.c_str())+1,0);
            if(len==-1) cerr<<"send reg msg error:"<<request<<endl;
            else{
                char buffer[1024]={0};
                len=recv(clientfd,buffer,1024,0);
                if(len==-1) cerr<<"recv reg msg error"<<endl;
                else{
                    json responsejs=json::parse(buffer);
                    if(responsejs["error"].get<int>()!=0){  // 注册失败
                        cerr<<name<<" is already exist, register error!"<<endl;
                    } else{
                        cout<<name<<" register success, userid is "<<responsejs["id"]
                        <<", do not forget it!"<<endl;
                    }
                }
            }
        }
            break;
        case 3: // quit业务
            close(clientfd);
            exit(0);
        default:
        cerr<<"invalid input!"<<endl;
            break;
        }
    }
    return 0;
}

// 显示当前登录成功用户的基本信息
void showCurrentUserData(){
    cout<<"++++++++++++++++++++++++++login user++++++++++++++++++++++++++"<<endl;
    cout<<"current login user => id: "<<g_currentUser.getId()<<" name: "<<g_currentUser.getName()<<endl;
    cout<<"--------------------------friend list-------------------------"<<endl;
    if(!g_currentUserFriendList.empty()){
        for(User &user:g_currentUserFriendList){
            cout<<"friend id: "<<user.getId()<<" name: "<<user.getName()<<" state: "<<user.getState()<<endl;
        }
    }
    cout<<"--------------------------group list--------------------------"<<endl;
    if(!g_currentUserGroupList.empty()){
        for(Group &group:g_currentUserGroupList){
            cout<<"group id: "<<group.getId()<<" name: "<<group.getName()<<" desc: "<<group.getDesc()<<endl;
            cout<<"group users list:"<<endl;
            for(GroupUser &user:group.getUsers()){
                cout<<"id: "<<user.getId()<<" name: "<<user.getName()<<" state: "<<user.getState()<<" role: "<<user.getRole()<<endl;
            }
        }
    }
    cout<<"==============================================================="<<endl;
}

// 接收线程
void readTaskHandler(int clientfd){
    while(isMainMenuRunning){
        char buffer[1024]={0};
        int len=recv(clientfd,buffer,1024,0);
        if(len==-1||len==0){
            close(clientfd);
            exit(-1);
        }

        // 接收Chatserver转发的数据，反序列化生成json数据对象
        json js=json::parse(buffer);

        int msgtype=js["msgid"].get<int>();
        // 个人聊天消息
        if(msgtype==ONE_CHAT_MSG){
            cout<<js["time"].get<string>()<<"["<<js["id"]<<"]"<<js["name"].get<string>()<<" said:"<<js["msg"].get<string>()<<endl;
            continue;
        }
        // 群组聊天消息
        if(msgtype==GROUP_CHAT_MSG){
            cout<<"群消息["<<js["groupid"]<<"]"<<js["time"]<<"["<<js["id"]<<"]"<<js["name"]<<" said:"<<js["msg"]<<endl;
            continue;
        }
    }
}

// 获取系统时间
string getCurrentTime(){
    auto tt=chrono::system_clock::to_time_t(chrono::system_clock::now());
    struct tm* pTime=localtime(&tt);
    char date[60]={0};
    sprintf(date,"%d-%02d-%02d %02d:%02d:%02d",
        int(1900+pTime->tm_year),int(1+pTime->tm_mon),(int)pTime->tm_mday,
        (int)pTime->tm_hour,(int)pTime->tm_min,(int)pTime->tm_sec);
    return std::string(date);
}

// 系统支持的客户端命令处理器
unordered_map<string,string> commandMap={
    {"help","显示所有支持的命令, 格式help"},
    {"chat","点对点聊天, chat:friendid:message"},
    {"addfriend","添加好友, addfriend:friendid"},
    {"creategroup","创建群组, creategroup:groupname:groupdesc"},
    {"addgroup","加入群组, addgroup:groupid"},
    {"groupchat","群聊天, groupchat:groupid:message"},
    {"logout","注销登录, 格式logout"},
};

// 注册系统支持的客户端命令处理器
unordered_map<string,function<void(int,string)>> commandHandlerMap={
    {"help",help},
    {"chat",chat},
    {"addfriend",addfriend},
    {"creategroup",creategroup},
    {"addgroup",addgroup},
    {"groupchat",groupchat},
    {"logout",logout},
};



// 聊天页面程序
void mainMenu(int clientfd){
    help(0,"");

    char buffer[1024]={0};
    while(isMainMenuRunning){
        cin.getline(buffer,1024);
        string commandbuf(buffer);
        string command; // 存储命令
        int index=commandbuf.find(":");
        if(index==-1){
            command=commandbuf;
        }else{
            command=commandbuf.substr(0,index);
        }
        auto it=commandHandlerMap.find(command);
        if(it==commandHandlerMap.end()){
            cerr<<"invalid input command!"<<endl;
            continue;
        }

        // 调用相应的事件处理回调函数, mainMenu对修改封闭, 添加新功能不需要修改mainMenu
        it->second(clientfd,commandbuf.substr(index+1,commandbuf.size()-index));    // 调用命令处理器
    }
}


// help command handler
void help(int fd=0,string str=""){
    cout<<"show all command list below:"<<endl;
    for(auto &p:commandMap){
        cout<<p.first<<":"<<p.second<<endl;
    }
    cout<<endl;
}

// chat command handler
void chat(int fd,string str){
    int index=str.find(":");    // 查找第一个冒号的位置, 将friendid:message分离
    if(index==-1){
        cerr<<"chat command invalid!"<<endl;
        return;
    }
    int friendid=atoi(str.substr(0,index).c_str());
    string message=str.substr(index+1,str.size()-index);

    json js;
    js["msgid"]=ONE_CHAT_MSG;
    js["id"]=g_currentUser.getId();
    js["name"]=g_currentUser.getName();
    js["toid"]=friendid;
    js["msg"]=message;
    js["time"]=getCurrentTime();

    string buffer=js.dump();
    int len=send(fd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(len==-1) cerr<<"send chat msg error:"<<buffer<<endl;
}

// addfriend command handler
void addfriend(int fd,string str){
    int friendid=atoi(str.c_str());
    json js;
    js["msgid"]=ADD_FRIEND_MSG;
    js["id"]=g_currentUser.getId();
    js["friendid"]=friendid;
    string buffer=js.dump();

    int len=send(fd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(len==-1) cerr<<"send addfriend msg error:"<<buffer<<endl;
}

// creategroup command handler
void creategroup(int fd,string str){
    int index=str.find(":");
    if(index==-1){
        cerr<<"creategroup command invalid!"<<endl;
        return;
    }
    string groupname=str.substr(0,index);
    string groupdesc=str.substr(index+1,str.size()-index);

    json js;
    js["msgid"]=CREATE_GROUP_MSG;
    js["id"]=g_currentUser.getId();
    js["groupname"]=groupname;
    js["groupdesc"]=groupdesc;
    string buffer=js.dump();

    int len=send(fd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(len==-1) cerr<<"send creategroup msg error:"<<buffer<<endl;
}

// addgroup command handler
void addgroup(int fd,string str){
    int groupid=atoi(str.c_str());
    json js;
    js["msgid"]=ADD_GROUP_MSG;
    js["id"]=g_currentUser.getId();
    js["groupid"]=groupid;
    string buffer=js.dump();

    int len=send(fd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(len==-1) cerr<<"send addgroup msg error:"<<buffer<<endl;
}

// groupchat command handler
void groupchat(int fd,string str){
    int index=str.find(":");
    if(index==-1){
        cerr<<"groupchat command invalid!"<<endl;
        return;
    }
    int groupid=atoi(str.substr(0,index).c_str());
    string message=str.substr(index+1,str.size()-index);

    json js;
    js["msgid"]=GROUP_CHAT_MSG;
    js["id"]=g_currentUser.getId();
    js["name"]=g_currentUser.getName();
    js["groupid"]=groupid;
    js["msg"]=message;
    js["time"]=getCurrentTime();

    string buffer=js.dump();
    int len=send(fd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(len==-1) cerr<<"send groupchat msg error:"<<buffer<<endl;
}

// logout command handler
void logout(int fd,string str){
    
    json js;
    js["msgid"]=LOGOUT_MSG;
    js["id"]=g_currentUser.getId();
    string buffer=js.dump();

    int len=send(fd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(len==-1) cerr<<"send logout msg error:"<<buffer<<endl;
    else isMainMenuRunning=false;
}