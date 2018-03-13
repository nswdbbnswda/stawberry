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
#include<QListWidgetItem>
#include<QColor>
#include<QSharedMemory>
#include<QTimer>
#include<QFile>
#include<queue>


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
    QList<QProcess*> processPointers;//
    std::map<QProcess*,QListWidgetItem*> procToItem;
    QProcess *pro;
    QSharedMemory *sharememory;
    QStringList startCmdList;
    QTimer *printTimer;//打印共享内存数据的定时器
    std::map<QProcess*,QSharedMemory*> shareObjMap;//共享内存对象Map server
    QFile *taskList;//任务列表交互文件
    QTimer checkTaskList;
    std::queue<QString> taskQueue;//提交任务的队列
    std::map<QString,QStringList> taskMap;
    QTimer executeTask;//定时执行任务
private:
    void    DestroyProcess();
    bool    ShowIpList();
    bool    IsIpExist(const QString &qstr);//判断TCP容器中是否存该地址
    void    InitBroadcast();
    void    BroadCast(ULONG mode);//向局域网广播消息
    void    MakeMsg(char *buf,ULONG command);//制作消息包
    void    MsgBrEntry(const ULONG mode,const QHostAddress &_ip);
    void    MsgAnsEntry();
    bool    FilterGetIp(const QString &_ip);
    void    AnswerMsg(const ULONG mode,const QHostAddress &_ip);//回复消息
    void    SendControlCommand(const QString &iPAddr,const char *pCmd);
    bool    StartSendProcess(const QStringList &qslt);//发送进程
    QProcess*    StartRecvProcess(const QStringList &qslt );//接收进程
    bool    ReadShareMemoryData();//读取共享内存数据
    void    InitShareMem();//初始化共享内存
    QString ChangePort(QString port);
    int     GenerateRandomNumber(int left,int right);//生成一个随机数
    QByteArray  taskCommand;//告诉客户端启动参数
    std::map<QString,QByteArray> ipToTaskLine;//IP地址到任务条的映射
    QByteArray recvCommand;//接收到服务器指令
    std::map<QProcess*,QByteArray> procToLog;//进程指针到日志记录的转换
    QFile *recvList;//接收记录情况
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
    void    EvPrintRate();//打印子进程进度
private slots:
    void    on_refreshButton_clicked();
    void    on_addUserButton_clicked();
    void    on_sendButton_clicked();
    void    on_killProButton_clicked();
    void    AcceptTask();//接受任务
    void    ExeCuteTask();//执行任务
};

#endif // MAINWINDOW_H
