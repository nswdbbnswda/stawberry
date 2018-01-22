#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include<QDebug>
#include<QTcpSocket>
#include<string>
#include<QProcess>
#include<QByteArray>
#include<QFile>
#include<QByteArray>
#include<QCoreApplication>
#include<QTime>
#include<QList>
#include<QTcpSocket>
#include"mytcpsever.h"
#include<iostream>
#include<QHostAddress>
#include<windows.h>
#include<QStringList>
#include<QUdpSocket>
#include<QList>
#include<map>
#include"IPMsg.h"
#include<QNetworkInterface>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
private:
    Ui::MainWindow *ui;
    MyTcpSever *server;
    int socketDescriptor;
    std::map<std::string,QTcpSocket*> ipMap;//TCP地址容器
    std::map<QString,QProcess *> pObjectProcess;//进程指针容器
    QUdpSocket *udpSock;//UDP接收套接字
    QList<QString> ipList;//储存所有广播上线得到IP地址
    bool    ShowIpList();
private:
    bool    IsIpExist(const QString &qstr);//判断TCP容器中是否存该地址
    void    InitBroadcast();
    void    BroadCast(ULONG mode);//向局域网广播消息
    void    MakeMsg(char *buf,ULONG command);//制作消息包
    void    MsgBrEntry(const ULONG mode,const QHostAddress &_ip);
    void    MsgAnsEntry();
    bool    FilterGetIp(const QString &_ip);
    void    AnswerMsg(const ULONG mode,const QHostAddress &_ip);//回复消息
    void    SendControlCommand(const QString &iPAddr,const char *pCmd);
    void    StartSendProcess(const QStringList &qslt);
public slots:
    void    EvReceiveCommand();
    void    EvNewConnection(qintptr ptr1);
    void    EvProStart();//启动了子进程
    void    EvProExit();//退出了子进程
    bool    EvConTcp(QString qstrIp);//连接指定的IP地址
    void    EvSendFile(QString qstrIpAddr,QStringList qstrContext);
    void    EvLeaveProc();
    void    EvUdp();//对UDP数据报进行处理
    void    EvReFresh();//刷新列表
    void    EvPrint();//打印子进程中的标准输入输出数据
private slots:
    void on_refreshButton_clicked();
    void on_addUserButton_clicked();
    void on_sendButton_clicked();
};

#endif // MAINWINDOW_H
