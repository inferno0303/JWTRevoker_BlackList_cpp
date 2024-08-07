# JWTRevoker_BlackList

**中文名称：JWT撤回器黑名单**

**英文名称：JWTRevoker_BlackList**

createTime: 2024-06-06

updateTIme: 2024-08-07

# 介绍

基于周期轮换布隆过滤器的JWT撤回器黑名单

## 功能

暂无

## 技术栈

暂无

## 第三方库

JSON library for C++ nlohmann/json: https://github.com/nlohmann/json


## 编译方法

```bash

g++ main.cpp src/BloomFilter/BloomFilter.cpp -o main.exe; .\main.exe
```

## 运行方法

暂无

# 更新记录

### 2024-06-12

实现了SHA256

### 2024-07-16

存档

### 2024-07-21

存档

### 2024-07-25

- 实现了线程安全队列
- 实现了非阻塞事件驱动的消息收发机制
- 实现了向 master 服务器身份认证

### 2024-07-29

- 存档

### 2024-08-07

- 存档