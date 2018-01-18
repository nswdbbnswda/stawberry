#include "mytcpsever.h"

MyTcpSever::MyTcpSever()
{

}


MyTcpSever::~MyTcpSever()
{


}


void MyTcpSever::incomingConnection(qintptr socketDescriptor)
{

    emit newClientConnection(socketDescriptor);

}
