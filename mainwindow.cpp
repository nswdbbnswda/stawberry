#include "mainwindow.h"
#include "ui_mainwindow.h"
#include<QThread>
#include<QTime>
#include<QFile>
#include<QCoreApplication>



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
    EvConTcp("127.0.0.1");
    ShowIpList();


    //定时查看所有共享内存区的进度情况
    printTimer = new QTimer(this);
    connect(printTimer,SIGNAL(timeout()),this,SLOT(EvPrintRate()));
    printTimer->start(1000);




    //创建任务交互文件
    taskList = new QFile(QCoreApplication::applicationDirPath()+ "/" + "tasklist.txt");
    //创建接收日志文件
    recvList = new QFile(QCoreApplication::applicationDirPath()+ "/" + "Recvlist.txt");



    //定时查询任务
    connect(&checkTaskList,SIGNAL(timeout()),this,SLOT(AcceptTask()));
    checkTaskList.start(2000);//每1秒查询一下任务交互文件并且处理好提交任务


    //定时执行任务
    connect(&executeTask,SIGNAL(timeout()),this,SLOT(ExeCuteTask()));
    executeTask.start(2000);//每间隔2毫秒执行一条任务


}

MainWindow::~MainWindow()
{
    if(recvList->isOpen()) recvList->close();
    if(recvList) delete recvList;
    if(taskList) delete taskList;
    if(sharememory) delete sharememory;
    if(printTimer)  delete printTimer;
    delete ui;
    if(server){delete server;server = NULL;}
    if(udpSock){ delete udpSock; udpSock = NULL;}
    DestroyProcess();//干掉链表中没有被终止的发送进程
}


//定时查找任务列表文件，如果找到该文件,定时处理该文件上的任务
void MainWindow::AcceptTask()
{
    if(taskList->exists()) qDebug()<<"发现任务文件！";
    else {
        qDebug()<<"还没有发现任务文件！";
        return;
    }
    //判断文件打开情况
    if(!taskList->isOpen()) {
     taskList->open(QIODevice::ReadOnly);//打开任务文件
    }

    if(taskList->atEnd()) {qDebug()<<"所有任务清单已经处理完毕！";
        taskList->close();//关闭文件
        taskList->remove();//删除任务文件
    }

    QByteArray temp = taskList->readLine();

    //解析IP地址
    int left = temp.indexOf("<");
    int right = temp.indexOf(">");
    //该行没有正确的IP地址
    if((-1 == left) && (-1 == right)) return;//放弃对本行的处理
    //如果执行到这里了，认为IP<>格式正确，尝试把<>中的IP地址拆出来
    QString ipQStr = temp.mid(left + 1,right - left - 1);//把IP地址拆分出来


    //IP到任务条的映射
    ipToTaskLine[ipQStr] = temp;   //192.168.1.2  ->   <192.168.1.2> |c:/big| *d:/fuck*


    //解析路径
    int start = temp.indexOf("|");
    int end = temp.lastIndexOf("|");
    //路径格式必须是这种格式的|c:/big|d:/test|
    if((-1 == start ) || ( -1 == end) || start == end) return;//地址格式错误，放弃


    //获得路径片段
    QString qpathList = temp.mid(start + 1,end - start - 1);// c:/big|d:/temp.txt
    QStringList pathlsit = qpathList.split("|");//拆分好路径片段并且保存到路径链表中


    //已经解析出IP和路径，但是尚未做有效性处理
    taskQueue.push(ipQStr);
    taskMap[ipQStr] = pathlsit;
}

//定时执行一条任务
void MainWindow::ExeCuteTask()
{
    //查看要发送的目标IP地址是否存在
    if(!taskQueue.size()) return;//队列为空说没没有可执行的任务直接退出就可以了

    QString ipAddr = taskQueue.front();//获得IP地址

    //处理字符串链表
    QStringList fonts;
    fonts.append("-s");
    foreach (QString var,taskMap[ipAddr]) {
       fonts.append("-d");
       fonts.append(var);
    }
    //清除任务
    taskQueue.pop();
    taskMap.erase(ipAddr);
    //显示当前任务指令
    qDebug()<<"当前任务指令："<<ipAddr<<fonts;
    EvSendFile(ipAddr,fonts);//发送文件
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


    //判断接收日志文件打开情况
    if(!recvList->isOpen()) {//没有打开就打开
        recvList->open(QIODevice::ReadWrite|QIODevice::Append);//打开任务文件
    }

    //写日志，说明本次任务已经接收完毕
    recvList->write(procToLog[tmp]);
    //recvList->waitForBytesWritten();
    recvList->flush();
    procToLog.erase(tmp);



    ui->listWidget_2->removeItemWidget(procToItem[tmp]);//把对应的item清理掉
    //干掉进程对象
    processPointers.removeAt(processPointers.indexOf(tmp));//删除存储在链表中的这个进程指针

    //清理共享内存对象
    delete shareObjMap[tmp];
    shareObjMap.erase(tmp);


    delete procToItem[tmp];
    procToItem.erase(tmp);
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
    QByteArray port;

    recvCommand = socket->readAll();

    qDebug()<<"接收到的指令是:"<<recvCommand;

    //解析端口号
    int left_p = recvCommand.indexOf("?");
    int right_p = recvCommand.lastIndexOf("?");
    if(left_p == right_p || -1 == left_p || -1 == right_p) {
        qDebug()<<"没有正确的接收到端口号";
        return;
    }
    port = recvCommand.mid(left_p + 1,right_p - left_p - 1);
    qDebug()<<"Left is "<<left_p<<"Right is "<<right_p<<"端口号是"<<port;


    //解析储存路径（服务端指定的，默认该地址一定存在）
    QString savePathQStr;
    int left_s = recvCommand.indexOf("*");
    int right_s = recvCommand.lastIndexOf("*");
    if(left_s == right_s || -1 == left_s || -1 == right_s) {
        qDebug()<<"没有正确的储存路径";
        savePathQStr = QCoreApplication::applicationDirPath() + "/DOWNLOAD";//如果没有解析到有效的路径就保存到默认的路径上
    }
    else{
        savePathQStr = recvCommand.mid(left_s + 1,right_s - left_s - 1);
    }


    //整理接收到的任务指令，并且记录下来，等到AutoSend进程结束的时候（接收完毕的时候，写一个日志记录）
    recvCommand.remove(left_p,right_p - left_p + 1);//删除端口号
    qDebug()<<"去掉端口号"<<recvCommand;



    //把IP地址替换成 发送者的
    int left_i = recvCommand.indexOf("<");
    int right_i = recvCommand.lastIndexOf(">");
    if(left_i == right_i || -1 == left_i || -1 == right_i) {
        qDebug()<<"没解析出正确的IP地址";
        return;
    }
    //替换IP地址
    recvCommand.remove(left_i,right_i - left_i + 1);
    //写入发送者的IP地址
    recvCommand.prepend(">");
    recvCommand.prepend(ipaddr.toString().toStdString().c_str());
    recvCommand.prepend("<");
    qDebug()<<"修正后的任务指令"<<recvCommand;

    //保存发送者IP地址到任务日志记录，当接收完毕的时候，把该条信息作为接收日志记录





    //启动接收进程
    QStringList arguments1;//默认保存到应用程序根目录
    arguments1 <<"-c"<<"-i"<<ipaddr.toString()<<"-p"<<port.data()<<"-d"<<savePathQStr; //添加启动参数 -c  -i 127.0.0.1  -p 5002 -d G:/DE
    QProcess *tmProc = StartRecvProcess(arguments1);//启动接收进程

    //记录接收进程指针和日志的对应关系
    procToLog[tmProc] = recvCommand;
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
    QProcess *tmp = (QProcess *)sender();//获得信号的发送者指针
    QListWidgetItem *item = procToItem[tmp];
    //item->setText(tmp->readLine());
}


//从共享内存中取出子进程的进度并且输出到UI
void MainWindow::EvPrintRate()
{
     qDebug()<<shareObjMap.size();
      std::map<QProcess*,QSharedMemory*>::iterator iter;//定义一个迭代指针iter
    for(iter = shareObjMap.begin(); iter != shareObjMap.end(); iter++) {
        if(!iter->second) return;
        if(!iter->second->attach()) return;
        qDebug()<<(char*) iter->second->data();
        QString itemContext = QString((char*)iter->second->data());
        itemContext.remove("\n");
        if(!procToItem[iter->first]) return;
        procToItem[iter->first]->setText(itemContext);
        iter->second->detach();
    }
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
        qDebug()<<"The address does not exist";//提示地址不存在，不能进行命令发送
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

             QString  taskCmd = ipToTaskLine[qstrIpAddr];
             taskCmd.append("?");
             taskCmd.append(qsPort);
             taskCmd.append("?");

             qDebug()<<"发送给客户端的指令是："<<taskCmd;
             SendControlCommand(qstrIpAddr,taskCmd.toStdString().data());//发送端启动成功,发送端口号给接收端
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
    connect(tmp,SIGNAL(readyRead()),this,SLOT(EvPrint()));//打印子进程标准输出

    tmp->start(program1, qslt);
    tmp->waitForStarted();//等待进程启动

    //等待1s让进程启动等待查看共享内存结果
    QThread::sleep(1);
    if(ReadShareMemoryData()) {
        ui->listWidget_2->addItem(item);//启动子进程成功在UI上提示

        //找到子进程的共享内存
        QString memID = qslt.at(qslt.indexOf("-p") + 1);//找到共享内存ID

        //为这个子进程实例化共享内存对象
        shareObjMap[tmp] = new QSharedMemory;
        shareObjMap[tmp]->setKey(memID);

        return true;
    }
    else {
       return false;
    }
}


/*启动接收进程*/
QProcess*  MainWindow::StartRecvProcess(const QStringList &qslt)
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


    //找到子进程的共享内存
     QString MemID =  "c" + qslt.at(qslt.indexOf("-p") + 1);//找到共享内存ID

     //为这个子进程实例化共享内存对象
    shareObjMap[tmp] = new QSharedMemory;
    shareObjMap[tmp]->setKey(MemID);

    return tmp;

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


/*产生随机数*/
int MainWindow::GenerateRandomNumber(int left,int right)
{
    qsrand(QTime(0,0,0).secsTo(QTime::currentTime()));
    int srandNum = left + qrand() % (right - left);
    qDebug()<<srandNum;
    return srandNum;
}













