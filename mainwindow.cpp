#include "mainwindow.h"
#include "ui_mainwindow.h"
#include<QThread>
#include<QTime>



MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{

    ui->setupUi(this);

    sharememory = new QSharedMemory();//构造实例对象
    sharememory->setKey("AutoSend");//为实例对象指定关键字(给共享内存命名)
    sharememory->detach();//将共享内存与本进程分离
    //创建共享内存
    sharememory->create(1);//1字节

    //初始化共享内存
    InitShareMem();



    server = new MyTcpSever;
    socketDescriptor = 0;
    if (!server->isListening()){//监听
        if (!server->listen(QHostAddress::AnyIPv4, 5000)){exit(0);}
    }
    else{exit(0);}
    QObject::connect(server, SIGNAL(newClientConnection(qintptr)), this, SLOT(EvNewConnection(qintptr)));//当有新的连接的时候，就会执行槽函数


    InitBroadcast();//初始化广播上线系统
    BroadCast(IPMSG_BR_ENTRY);//向局域网发送广播上线消息
    ui->Numbers->setText(QString::number (ipMap.size(),10 ));//显示用户数量
}

MainWindow::~MainWindow()
{
    if(sharememory) delete sharememory;
    delete ui;
    if(server){delete server;server = NULL;}
    if(udpSock){ delete udpSock; udpSock = NULL;}
    DestroyProcess();//干掉链表中没有被终止的发送进程
}

/*关闭所有进程*/
void MainWindow::DestroyProcess()
{
    foreach (QProcess* var,processPointers) {
        var->kill();
    }
    processPointers.clear();

}


/*显示IP*/
bool MainWindow::ShowIpList()
{
    ui->Numbers->setText(QString::number (ipMap.size(),10 ));//显示用户数量
    ui->listWidget->clear();
    QString qstr;
    if(ipMap.empty()){return false;}
    std::map<std::string,QTcpSocket*>::iterator iter;
    for(iter = ipMap.begin();iter != ipMap.end(); iter++){
        qstr = QString::fromStdString(iter->first);
        ui->listWidget->addItem(qstr);
    }
    return true;
}

/*判断容器中是否存在该地址*/
bool MainWindow::IsIpExist(const QString &qstr)
{

    std::map<std::string,QTcpSocket*>::iterator it = ipMap.find(qstr.toStdString().c_str());//看看map中是否有这个地址
    if(it != ipMap.end()){  return true; }//如果找map中有这个IP地址
    else {return false;}
}

/*有主机主动连接本主机*/
void MainWindow::EvNewConnection(qintptr ptr1)
{
    QHostAddress ipaddr;
    socketDescriptor = ptr1;
    QTcpSocket *pSocket = new QTcpSocket;//创建套接字用于接收数据
    pSocket->setSocketDescriptor(socketDescriptor);//设置套接字描述符
    ipaddr = pSocket->peerAddress();//获取客户端的IP地址
    ipMap[ipaddr.toString().toStdString()] = pSocket;//把套接字指针加到map中
    connect(ipMap[ipaddr.toString().toStdString()],SIGNAL(readyRead()),this,SLOT(EvReceiveCommand()));//当一个服务端请求连接的时候 把一个新的套接字和接收槽连接上
    connect(ipMap[ipaddr.toString().toStdString()],SIGNAL(disconnected()),this,SLOT(EvLeaveProc()));//断开连接处理
    ShowIpList();//更新界面上的IP地址列表

}

/*进程启动了*/
void MainWindow::EvProStart()
{

}

/*进程退出了*/
void MainWindow::EvProExit()
{
    QProcess *tmp = (QProcess *)sender();//获得信号的发送者指针
    ui->listWidget_2->removeItemWidget(procToItem[tmp]);//把对应的item清理掉
    //干掉进程对象
    processPointers.removeAt(processPointers.indexOf(tmp));//删除存储在链表中的这个进程指针
    delete procToItem[tmp];
    delete tmp;
    if(!ReadShareMemoryData()){
        qDebug()<<"端口冲突启动失败";
    }
}


/*接收指令*/
void MainWindow::EvReceiveCommand()
{
    //接收短消息 并且解析出IP地址和端口号码
    QTcpSocket *socket = (QTcpSocket *)sender();//获得信号的发送者
    QHostAddress ipaddr = socket->peerAddress();//获取客户端的IP地址
    QByteArray temp1;
    temp1 = socket->readAll();

    //启动接收进程
    QStringList arguments1;//默认保存到应用程序根目录
    arguments1 <<"-c"<<"-i"<<ipaddr.toString()<<"-p"<<temp1.data()<<"-d"<<QCoreApplication::applicationDirPath() + "/DOWNLOAD"; //添加启动参数 -c  -i 127.0.0.1  -p 5002 -d G:/DE
    StartRecvProcess(arguments1);//启动接收进程
}


/*有主机下线*/
void MainWindow::EvLeaveProc()
{
    QTcpSocket *socket = (QTcpSocket *)sender();//获得信号的发送者
    QHostAddress ipaddr = socket->peerAddress();//获取IP地址

    delete ipMap[ipaddr.toString().toStdString()];//释放套接字
    ipMap.erase(ipaddr.toString().toStdString());//把这个元素从map中移除  //内存尚未释放
    ipList.removeAt(ipList.indexOf(ipaddr.toString()));//把UDP地址链表中的该下线主机地址也给清理掉
    ShowIpList();//更新界面上的IP地址列表
}




/*接收数据报*/
void MainWindow::EvUdp()
{
    QHostAddress client_address;//声明一个QHostAddress对象用于保存发送端的信息

    while(udpSock->hasPendingDatagrams())
    {
        quint16 recPort = 0;
        QByteArray datagram;
        datagram.resize(udpSock->pendingDatagramSize());//datagram大小为等待处理数据报的大小才能接收数据;
        udpSock->readDatagram(datagram.data(),datagram.size(), &client_address, &recPort);//接收数据报

        //查看一下是什么指令
        switch(atol(datagram.data()))
        {
        case IPMSG_BR_ENTRY:
        {
            if(FilterGetIp(client_address.toString())){//有网络上的其他主机上线
                MsgBrEntry(IPMSG_ANSENTRY,client_address);//给该主机回复"我在"消息

                if( -1 == ipList.indexOf(client_address.toString())){//看看我的IP列表中没有该IP
                    ipList.append(client_address.toString());//把它添加到我的IP列表中
                    //  qDebug()<<client_address.toString();

                }
            }
        }
            break;
        case IPMSG_ANSENTRY:
            if(FilterGetIp(client_address.toString()))//过滤发给自己的消息
            {
                if( -1 == ipList.indexOf(client_address.toString())){//链表中不存在这个地址
                    ipList.append(client_address.toString());//把地址插入到链表
                    //qDebug()<<client_address.toString();//表示这是网络中处于在线状态的一个主机，并且本机第一次与该机进行建立通讯连接
                    EvConTcp(client_address.toString());//发起TCP连接
                    ShowIpList();//更新IP地址列表
                }
            }
            break;
        }
    }
}


/*刷新用户列表*/
void MainWindow::EvReFresh()
{
    BroadCast(IPMSG_BR_ENTRY);//向局域网发送广播上线消息
}


/*打印子进程输出*/
void MainWindow::EvPrint()
{
//    QProcess *tmp = (QProcess *)sender();//获得信号的发送者指针
//   // qDebug()<<tmp->readAll();
//    //ui->listWidget_2->clear();
//    QString qstrContext = ui->textEdit->toPlainText();
//    ui->listWidget_2->addItem(qstrContext);
}


/*发起TCP连接*/
bool MainWindow::EvConTcp(QString qstrIp)
{
    if(IsIpExist(qstrIp)){//判断连接是否已存在
        std::cout<<"The address already exists!";
        return false;//返回,不进行添加操作
    }
    //建立新连接
    QTcpSocket *pSocket = new QTcpSocket;
    pSocket->connectToHost(qstrIp.toStdString().c_str(),5000);//尝试用这个连接这个IP地址
    if(pSocket->waitForConnected()){//如果连接成功
        //std::cout<<"Successfully connected to "<<qstrIp.toStdString().c_str()<<"!"<<std::endl;//提示连接成功
        ipMap[qstrIp.toStdString()] = pSocket;//把这个指针加入到map中
        QObject::connect(ipMap[qstrIp.toStdString()],SIGNAL(readyRead()),this,SLOT(EvReceiveCommand()));//把每一个主动连接的套接字都连接到接收槽上
        QObject::connect(ipMap[qstrIp.toStdString()],SIGNAL(disconnected()),this,SLOT(EvLeaveProc()));//断开连接处理
        return true;
    }
    else{std::cout<<"Failed to connect to  "<<qstrIp.toStdString().c_str()<<"!"<<std::endl; delete pSocket;} //连接失败了
    return false;
}

/*发送文件*/
void MainWindow::EvSendFile(QString qstrIpAddr, QStringList qstrContext)// -s -p 5001 -d G:/moon
{
    if(!IsIpExist(qstrIpAddr)){//如果map中没有这个地址,退出
        std::cout<<"The address does not exist"<<std::endl;//提示地址不存在，不能进行命令发送
        return;
    }
    QString qsPort = QString::number(GenerateRandomNumber(10000,20000),10);//随机选取一个10000-20000之间的端口作为初始尝试启动发送进程端口

    QStringList arguments1;
    arguments1<<qstrContext;
    arguments1.append("-p");
    arguments1.append(qsPort);
    qsPort = arguments1.at(arguments1.indexOf("-p") + 1);//找到端口号


    while(true) {
        if(StartSendProcess(arguments1)) {//启动本地文件发送进程
             SendControlCommand(qstrIpAddr,qsPort.toStdString().data());//发送端启动成功,发送端口号给接收端
             break;
        }
        else { //端口冲突了,进行处理
            qsPort = ChangePort(qsPort);//换一个端口
            arguments1.replace((arguments1.indexOf("-p") + 1),qsPort);//修改一下启动AutoSend程序所需要的命令行
        }
    }
}


/*初始化UDP*/
void MainWindow::InitBroadcast()
{
    udpSock = new QUdpSocket(this);//创建一个UDP套接字
    udpSock->bind(DEST_PORT,QUdpSocket::ShareAddress); //绑定LOCAL_PORT端口作为数据输出口
    connect(udpSock,SIGNAL(readyRead()),this,SLOT(EvUdp()));//收到广播消息进行处理
}



/*进行广播*/
void MainWindow::BroadCast(ULONG mode)
{
    //制作数据报
    char buf[20];
    MakeMsg(buf,mode);
    QByteArray datagram = QByteArray(buf);
    //发送数据报
    udpSock->writeDatagram(datagram.data(),datagram.size(),QHostAddress::Broadcast,DEST_PORT);//向网络上所有主机的12811端口发送数据
}

/*制作消息包*/
void MainWindow::MakeMsg(char *buf,ULONG command)
{
    int			cmd = GET_MODE(command);//把ULONG类型通过宏转换成整型数
    bool		is_br_cmd =	cmd == IPMSG_BR_ENTRY ||//如果命令是IPMSG_BR_ENTRY，IPMSG_BR_EXIT，IPMSG_BR_ABSENCE，
            cmd == IPMSG_BR_EXIT  ||//IPMSG_NOOPERATION四个之中的其中一个该表达式的值就为真
            cmd == IPMSG_BR_ABSENCE ||
            cmd == IPMSG_NOOPERATION ? true : false;
    sprintf(buf, "%u",command);//制作格式化UDP 数据
}


/*回复"我在"消息*/
void MainWindow::MsgBrEntry(const ULONG mode,const QHostAddress &_ip)
{
    AnswerMsg(mode,_ip);
}



/*收到了别人发给本机的"我在"消息*/
void MainWindow::MsgAnsEntry()
{

}

/*过滤IP*/
bool MainWindow::FilterGetIp(const QString &_ip)
{
    //使用allAddresses命令获得所有的ip地址
    QList<QHostAddress> list = QNetworkInterface::allAddresses();
    //对找到的每一个IP地址进行比对
    foreach (QHostAddress address,list)
    {
        if(address.protocol() == QAbstractSocket::IPv4Protocol && address.toString() == _ip)
            return false;//如果本机所有的IPV4地址都不与传过来的这个IP地址相当，那么就说明该IP地址是一个其他主机的IPV4地址
    }
    return true;
}

/*回复消息*/
void MainWindow::AnswerMsg(const ULONG mode,const QHostAddress & _ip)
{
    //制作数据报
    char buf[20];
    MakeMsg(buf,mode);
    QByteArray datagram = QByteArray(buf);
    //发送数据报
    udpSock->writeDatagram(datagram.data(),datagram.size(),_ip,DEST_PORT);//给发来UDP广播消息的主机进行回复
}


/*启动发送进程*/
bool MainWindow::StartSendProcess(const QStringList &qslt )
{

    QString program1 = QCoreApplication::applicationDirPath() + "/AutoSend.exe";//待启动程序路径
    QProcess  *tmp = new QProcess;
    processPointers.append(tmp);//把进程对象指针存在容器中
    QListWidgetItem *item = new QListWidgetItem("正在发送任务："+ui->textEdit->toPlainText());


    item->setBackgroundColor(QColor(255, 0, 0, 255));

    procToItem[tmp] = item;

    connect(tmp,SIGNAL(finished(int,QProcess::ExitStatus)),this,SLOT(EvProExit()));//子进程退出了
    connect(tmp,SIGNAL(started()),this,SLOT(EvProStart()));

    tmp->start(program1, qslt);
    tmp->waitForStarted();//等待进程启动

    //等待1s让进程启动等待查看共享内存结果
    QThread::sleep(1);
    if(ReadShareMemoryData()) {
    ui->listWidget_2->addItem(item);
    return true;
    }
    else {
       return false;
    }
}


/*启动接收进程*/
void MainWindow::StartRecvProcess(const QStringList &qslt)
{

    QString program1 = QCoreApplication::applicationDirPath() + "/AutoSend.exe";//待启动程序路径
    QProcess  *tmp = new QProcess;//每启动一个进程都为这个进程创建一个对象
    processPointers.append(tmp);//把进程对象指针存在容器中

    QListWidgetItem *item = new QListWidgetItem("正在接收任务...");
    item->setBackgroundColor(QColor(0, 255, 0, 255));


    procToItem[tmp] = item;
    ui->listWidget_2->addItem(item);

    connect(tmp,SIGNAL(finished(int,QProcess::ExitStatus)),this,SLOT(EvProExit()));//进程退出
    connect(tmp,SIGNAL(started()),this,SLOT(EvProStart()));//进程启动

    tmp->start(program1, qslt);
    tmp->waitForStarted();//等待进程启动

}


/*读取共享内存数据*/
bool  MainWindow::ReadShareMemoryData()
{
    //阻塞等待进程外部进程更新共享内存数据
    return '1' == *((char*)sharememory->data());
}

//初始化共享内存
void MainWindow::InitShareMem()
{
    //向共享内存中写入数据
    sharememory->lock();//获得共享内存权限
    char* byte = (char*)sharememory->data();
    byte[0] = '1';
    sharememory->unlock();
}

//把端口号数值变换一下
QString MainWindow::ChangePort(QString port)
{
    bool ok;
    int dec = port.toInt(&ok,10) + 1;
    return QString::number(dec,10);
}


/*发送控制指令*/
void MainWindow::SendControlCommand(const QString &iPAddr,const char *pCmd)
{
    std::string numSocket = iPAddr.toStdString();//获得套接字号码
    ipMap[iPAddr.toStdString()]->write(pCmd);//把端口号扔给接收端
    ipMap[iPAddr.toStdString()]->waitForBytesWritten();//等待发送完毕
}


/*刷新*/
void MainWindow::on_refreshButton_clicked()
{
    EvReFresh();//向局域网广播我在消息
    ShowIpList();//显示IP列表
    qDebug()<<"链表中有"<<processPointers.size()<<"个进程指针";
    GenerateRandomNumber(10000,20000);
}


/*添加用户*/
void MainWindow::on_addUserButton_clicked()
{
    EvConTcp(ui->lineEdit->displayText());
    ShowIpList();
}


/*发送文件*/
void MainWindow::on_sendButton_clicked()
{
    //获得IP地址
    if(-1 == ui->listWidget->currentRow()){ return;} //如果没有行被选中,Send按钮不做任何响应
    QString ipAddr = ui->listWidget->currentItem()->text();//获得IP地址
    QString qstrContext = ui->textEdit->toPlainText();//获得输入的文本
    QStringList fonts = qstrContext.split(" ");//以空格为分隔符
    EvSendFile(ipAddr,fonts);//发送文件

}


/*杀掉所有子进程*/
void MainWindow::on_killProButton_clicked()
{
    DestroyProcess();
}


//产生随机数
int MainWindow::GenerateRandomNumber(int left,int right)
{
    qsrand(QTime(0,0,0).secsTo(QTime::currentTime()));
    int srandNum = left + qrand() % (right - left);
    qDebug()<<srandNum;
    return srandNum;
}







