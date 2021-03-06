#include "commandlistexecutor.h"

CommandListExecutor::CommandListExecutor(QSerialPort* port){
    this->serial = port;
    this->moveToThread(&workerThread);
    workerThread.start();
    executingCmds = false;
    timeoutTimer = new QTimer();
    timeoutTimer->moveToThread(&workerThread);
    timeoutTimer->setInterval(2000);
    timeoutTimer->setSingleShot(true);
    connect(timeoutTimer,SIGNAL(timeout()),this,SLOT(onTimeout()));
    cmdsPending = 0;
    clientCmdBufferSize = 10;
}

CommandListExecutor::~CommandListExecutor(){
    executingCmds = false;
    workerThread.quit();
    workerThread.wait();
}
void CommandListExecutor::executeCmdList(QStringList cmds)
{
    currCmdIdx = 0;
    cmdList = cmds;
    executingCmds = true;

    qDebug(cmdList.at(currCmdIdx).toLocal8Bit());
    // Send first command and wait for answer
    sendCmd(cmdList.at(currCmdIdx++));
    emit onChangeProgress((float)currCmdIdx/cmdList.size());
}


void CommandListExecutor::onRecieveAnswer(QString answ)
{
    if(!executingCmds)
        return;

    timeoutTimer->stop();

    // Busy ? wait for ack
    if(answ.contains("BUSY"))
        return;

    // Ack recieved?
    if(answ.contains("ACK")){
        if(!answ.contains("ACK: 0")){
            qDebug("Error!");
            stop();
        }
        // command was executed
        cmdsPending--;

        if(currCmdIdx < cmdList.size() && cmdsPending < clientCmdBufferSize){
            QString cmd;
            do{
                cmd = cmdList.at(currCmdIdx++);
            }while((cmd.contains(";") || cmd.trimmed().isEmpty()) && currCmdIdx <= cmdList.size());
            if(currCmdIdx > cmdList.size()){
                emit onExecutionFinished();
                executingCmds = false;
            }else{
                //qDebug((QString("Send Next: ") + cmd.toLocal8Bit()).toLocal8Bit());
                sendCmd(cmd);
                emit onChangeProgress((float)currCmdIdx/cmdList.size());
            }
        }
        else{
            emit onExecutionFinished();
            executingCmds = false;
        }
    }
}
void CommandListExecutor::onTimeout()
{   if(!executingCmds)
        return;

    qDebug("Resend command after timeout: ");
    qDebug(cmdList.at(currCmdIdx-1).toLocal8Bit());
    sendCmd(cmdList.at(currCmdIdx-1));
}

void CommandListExecutor::stop(){
    executingCmds = false;
    timeoutTimer->stop();
    emit onExecutionAborted();
}
void CommandListExecutor::sendCmd(QString str)
{
    // command queued in controller
    cmdsPending++;
    emit onSendCommand(str);
    //timeoutTimer->start();
}
