#ifndef GROUPUSER_H
#define GROUPUSER_H

#include "user.hpp"

// 群组用户，多了一个role角色信息，从User类直接继承，复用User的其它信息
class GroupUser : public User
{
public:
    
    // 群组中的角色信息
    string getRole(){return this->role;}
    void setRole(string role){this->role=role;}
    
private:
    string role;
};

#endif  // GROUPUSER_H