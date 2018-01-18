#ifndef MYTCPSEVER_H
#define MYTCPSEVER_H
#include<QTcpServer>

class MyTcpSever : public QTcpServer
{ Q_OBJECT
public:
    explicit MyTcpSever();
    virtual ~MyTcpSever();
protected:
    void incomingConnection(qintptr socketDescriptor);
signals:
    void newClientConnection(qintptr socketDescriptor);
};

#endif // MYTCPSEVER_H
