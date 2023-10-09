#ifndef FRIENDMODEL_H
#define FRIENDMODEL_H

#include <vector>
#include "user.hpp"

using namespace std;

// 提供离线消息表的操作接口方法
class FriendModel
{
public:
    // 添加好友关系
    void insert(int userid,int friendid);

    // 返回用户好友列表 friendid
    vector<User> query(int userid);
    
};

#endif // FRIENDMODEL_H